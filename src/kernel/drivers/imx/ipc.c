// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <rtos/panic.h>
#include <rtos/interrupt.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/schedule.h>
#include <sof/drivers/mu.h>
#include <rtos/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <rtos/spinlock.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <ipc/trace.h>

#if CONFIG_IPC_MAJOR_4
#include <sof/ipc/common.h>
#include <ipc4/header.h>
#include <rimage/sof/user/manifest.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ipc_task, CONFIG_SOF_LOG_LEVEL);

/* thanks to the fact that ARM's GIC is supported
 * by Zephyr there's no need to clear interrupts
 * explicitly. This should already be done by Zephyr
 * after executing the ISR. This macro is used for
 * linkage purposes on ARM64-based platforms.
 */
#define interrupt_clear(irq)

SOF_DEFINE_REG_UUID(ipc_task);

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

#if CONFIG_IPC_MAJOR_4
/*
 * IPC4 module manifest, emitted into the ".module" ELF section and re-emitted
 * by rimage into the ext-manifest module table. Module id == table index, so
 * BASEFW must stay first; UUIDs must match uuid-registry.txt.
 */
#define IMX_MAN_MODULE(mname, uuid_a, uuid_b, uuid_c, uuid_d...)	\
	{								\
		.module = {						\
			.name = mname,					\
			.uuid = {					\
				.a = uuid_a,				\
				.b = uuid_b,				\
				.c = uuid_c,				\
				.d = { uuid_d },			\
			},						\
			.type = {					\
				.load_type = SOF_MAN_MOD_TYPE_BUILTIN,	\
				.domain_ll = 1,				\
			},						\
			.affinity_mask = 1,				\
			.instance_max_count = 8,			\
		},							\
	}

static const struct sof_man_module_manifest imx_man_modules[]
	__section(".module") __used = {
	IMX_MAN_MODULE("BASEFW", 0x0e398c32, 0x5ade, 0xba4b,
		       0x93, 0xb1, 0xc5, 0x04, 0x32, 0x28, 0x0e, 0xe4),
	IMX_MAN_MODULE("COPIER", 0x9ba00c83, 0xca12, 0x4a83,
		       0x94, 0x3c, 0x1f, 0xa2, 0xe8, 0x2f, 0x9d, 0xda),
	IMX_MAN_MODULE("GAIN", 0x61bca9a8, 0x18d0, 0x4a18,
		       0x8e, 0x7b, 0x26, 0x39, 0x21, 0x98, 0x04, 0xb7),
	IMX_MAN_MODULE("MIXIN", 0x39656eb2, 0x3b71, 0x4049,
		       0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09),
	IMX_MAN_MODULE("MIXOUT", 0x3c56505a, 0x24d7, 0x418f,
		       0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0),
};

const struct sof_man_module *platform_ipc4_get_module(uint32_t module_id)
{
	if (module_id >= ARRAY_SIZE(imx_man_modules))
		return NULL;

	return &imx_man_modules[module_id].module;
}

/*
 * The 8-byte IPC4 header does not fit the 4-byte MU data register, so it
 * travels in the SRAM slot in front of each mailbox payload.
 */
static void imx_ipc4_read_hdr(struct ipc_cmd_hdr *hdr)
{
	volatile struct ipc_cmd_hdr *slot = (void *)MAILBOX_HOSTBOX_HDR_BASE;

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_HDR_BASE,
				 sizeof(*hdr));
	hdr->pri = slot->pri;
	hdr->ext = slot->ext;

	tr_dbg(&ipc_tr, "ipc4: msg rx <- pri 0x%08x ext 0x%08x", hdr->pri, hdr->ext);
}

static void imx_ipc4_write_hdr(const struct ipc_msg *msg)
{
	volatile struct ipc_cmd_hdr *slot = (void *)MAILBOX_DSPBOX_HDR_BASE;

	slot->pri = msg->header;
	slot->ext = msg->extension;
	dcache_writeback_region((__sparse_force void __sparse_cache *)MAILBOX_DSPBOX_HDR_BASE,
				sizeof(*slot));

	tr_dbg(&ipc_tr, "ipc4: msg tx -> pri 0x%08x ext 0x%08x size %u",
	       msg->header, msg->extension, msg->tx_size);
}

/* send SOF_IPC4_FW_READY as a notification (no payload) */
int imx_ipc4_boot_complete(uint32_t boot_message)
{
	struct ipc_cmd_hdr header;
	struct ipc_msg msg = { 0 };

	ipc_boot_complete_msg(&header, 0);
	msg.header = header.pri | boot_message;
	msg.extension = header.ext;

	return ipc_platform_send_msg(&msg);
}
#endif /* CONFIG_IPC_MAJOR_4 */

static void irq_handler(void *arg)
{
	struct ipc *ipc = arg;
	uint32_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

	tr_dbg(&ipc_tr, "ipc: irq isr 0x%x", status);

	/* reply message(done) from host */
	if (status & IMX_MU_xSR_GIPn(IMX_MU_VERSION, 1)) {
		/* Clear GP pending interrupt #1 */
		imx_mu_write(IMX_MU_xSR_GIPn(IMX_MU_VERSION, 1),
			     IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		ipc->is_notification_pending = false;
	}

	/* new message from host */
	if (status & IMX_MU_xSR_GIPn(IMX_MU_VERSION, 0)) {
		/* Clear GP pending interrupt #0 */
		imx_mu_write(IMX_MU_xSR_GIPn(IMX_MU_VERSION, 0),
			     IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		ipc_schedule_process(ipc);
	}
}

int ipc_platform_compact_write_msg(struct ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

int ipc_platform_compact_read_msg(struct ipc_cmd_hdr *hdr, int words)
{
#if CONFIG_IPC_MAJOR_4
	if (words != 2)
		return 0;

	imx_ipc4_read_hdr(hdr);

	return 2; /* number of words read */
#else
	return 0; /* number of words read - not currently used on this platform */
#endif
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;
	/* Use struct ipc_data *iipc = ipc_get_drvdata(ipc); if needed */

#if CONFIG_IPC_MAJOR_4
	hdr = ipc_compact_read_msg();
#else
	hdr = mailbox_validate();
#endif

	/* perform command */
	ipc_cmd(hdr);

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	int ret;

	/* make sure GIR0 and GIR1 are not already set before asserting GIR0 */
	ret = poll_for_register_delay(MU_BASE + IMX_MU_xCR(IMX_MU_VERSION, IMX_MU_GCR),
					IMX_MU_xCR_GIRn(IMX_MU_VERSION, 0),
					0,
					100);
	if (ret < 0)
		tr_err(&ipc_tr, "failed poll for GIR0");


	ret = poll_for_register_delay(MU_BASE + IMX_MU_xCR(IMX_MU_VERSION, IMX_MU_GCR),
					IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1),
					0,
					100);
	if (ret < 0)
		tr_err(&ipc_tr, "failed poll for GIR1");

	/* request GP interrupt #0 - notify host that reply is ready */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GCR, IMX_MU_xCR_GIRn(IMX_MU_VERSION, 0), 0);

	// TODO: signal audio work to enter D3 in normal context
	/* are we about to enter D3 ? */
#ifdef CONFIG_XTENSA
	if (ipc->pm_prepare_D3) {
		while (1)
			/*
			 * Note, that this function is now called with
			 * interrupts disabled, so this wait will never even
			 * return anyway
			 */
			wait_for_interrupt(0);
	}
#endif /* CONFIG_XTENSA */
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();
	uint32_t gir0_set, gir1_set, control;

	control = imx_mu_read(IMX_MU_xCR(IMX_MU_VERSION, IMX_MU_GCR));
	gir1_set = control & IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1);
	gir0_set = control & IMX_MU_xCR_GIRn(IMX_MU_VERSION, 0);

	/* can't send notification when one is in progress */
	if (ipc->is_notification_pending || gir0_set || gir1_set)
		return -EBUSY;

#if CONFIG_IPC_MAJOR_4
	imx_ipc4_write_hdr(msg);
#endif

	/* now send the message */
	if (msg->tx_size)
		mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	tr_dbg(&ipc_tr, "ipc: msg tx -> 0x%x", msg->header);

	ipc->is_notification_pending = true;

	/* now interrupt host to tell it we have sent a message */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GCR, IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1), 0);
	return 0;
}

void ipc_platform_send_msg_direct(const struct ipc_msg *msg)
{
	/* TODO: add support */
}

#if CONFIG_HOST_PTABLE
struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc)
{
	struct ipc_data *iipc = ipc_get_drvdata(ipc);

	return &iipc->dh_buffer;
}
#endif

int platform_ipc_init(struct ipc *ipc)
{
#if CONFIG_HOST_PTABLE
	struct ipc_data *iipc;

	iipc = rzalloc(SOF_MEM_FLAG_USER, sizeof(*iipc));
	if (!iipc) {
		tr_err(&ipc_tr, "Unable to allocate IPC private data");
		return -ENOMEM;
	}
	ipc_set_drvdata(ipc, iipc);
#else
	ipc_set_drvdata(ipc, NULL);
#endif

#if CONFIG_IPC_MAJOR_4
	/* SW regs survive a reload; clear stale LLP slots */
	bzero((void *)MAILBOX_SW_REG_BASE, MAILBOX_SW_REG_SIZE);
	dcache_writeback_region((__sparse_force void __sparse_cache *)MAILBOX_SW_REG_BASE,
				MAILBOX_SW_REG_SIZE);
#endif

	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->dh_buffer.page_table = rzalloc(SOF_MEM_FLAG_USER,
					     PLATFORM_PAGE_TABLE_SIZE);
	if (!iipc->dh_buffer.page_table)
		sof_panic(SOF_IPC_PANIC_IPC);
	bzero(iipc->dh_buffer.page_table, PLATFORM_PAGE_TABLE_SIZE);
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	iipc->dh_buffer.dmac = sof_dma_get(SOF_DMA_DIR_HMEM_TO_LMEM, 0, SOF_DMA_DEV_HOST,
					   SOF_DMA_ACCESS_SHARED);
#else
	iipc->dh_buffer.dmac = dma_get(SOF_DMA_DIR_HMEM_TO_LMEM, 0, SOF_DMA_DEV_HOST,
				       SOF_DMA_ACCESS_SHARED);
#endif
	if (!iipc->dh_buffer.dmac) {
		tr_err(&ipc_tr, "Unable to find DMA for host page table");
		sof_panic(SOF_IPC_PANIC_IPC);
	}
#endif

	/* Disable interrupt for DSP Core */
	interrupt_disable(PLATFORM_IPC_INTERRUPT, ipc);

	/* Disable interrupt from MU:
	 * GP #0 for Host -> DSP message notification
	 * GP #1 for DSP -> Host message confirmation
	 * GP #2 and #3 not used
	 */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GIER, 0,
		       IMX_MU_xCR_GIEn(IMX_MU_VERSION, 0) |
		       IMX_MU_xCR_GIEn(IMX_MU_VERSION, 1) |
		       IMX_MU_xCR_GIEn(IMX_MU_VERSION, 2) |
		       IMX_MU_xCR_GIEn(IMX_MU_VERSION, 3));

	/* Clear all pending interrupts from MU */
	imx_mu_write(IMX_MU_xSR_GIPn(IMX_MU_VERSION, 0) |
		     IMX_MU_xSR_GIPn(IMX_MU_VERSION, 1) |
		     IMX_MU_xSR_GIPn(IMX_MU_VERSION, 2) |
		     IMX_MU_xSR_GIPn(IMX_MU_VERSION, 3),
		     IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

	/* Clear pending interrupt for DSP Core */
	interrupt_clear(PLATFORM_IPC_INTERRUPT);

	/* Configure interrupt for DSP Core */
	interrupt_register(PLATFORM_IPC_INTERRUPT, irq_handler, ipc);
	interrupt_enable(PLATFORM_IPC_INTERRUPT, ipc);

	/* Enable interrupt from MU:
	 * enable GP #0 for Host -> DSP message notification
	 * enable GP #1 for DSP -> Host message confirmation
	 */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GIER, IMX_MU_xCR_GIEn(IMX_MU_VERSION, 0) |
			IMX_MU_xCR_GIEn(IMX_MU_VERSION, 1), 0);

	return 0;
}

#if CONFIG_IPC_POLLING

int ipc_platform_poll_init(void)
{
	return 0;
}

/* tell host we have completed command */
void ipc_platform_poll_set_cmd_done(void)
{
	/* enable GP interrupt #0 - accept new messages */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GIER, IMX_MU_xCR_GIEn(IMX_MU_VERSION, 0), 0);

	/* request GP interrupt #0 - notify host that reply is ready */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GCR, IMX_MU_xCR_GIRn(IMX_MU_VERSION, 0), 0);
}

/* read the IPC register for any new command messages */
int ipc_platform_poll_is_cmd_pending(void)
{
	uint32_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

	/* new message from host */
	if (status & IMX_MU_xSR_GIPn(IMX_MU_VERSION, 0)) {
		/* Disable GP interrupt #0 */
		imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GIER, 0, IMX_MU_xCR_GIEn(IMX_MU_VERSION, 0));

		/* Clear GP pending interrupt #0 */
		imx_mu_write(IMX_MU_xSR_GIPn(IMX_MU_VERSION, 0),
			     IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		/*new message */
		return 1;
	}

	/* no new message */
	return 0;
}

int ipc_platform_poll_is_host_ready(void)
{
	uint32_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

	/* reply message(done) from host */
	if (status & IMX_MU_xSR_GIPn(IMX_MU_VERSION, 1)) {
		/* Disable GP interrupt #1 */
		imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GIER, 0, IMX_MU_xCR_GIEn(IMX_MU_VERSION, 1));

		/* Clear GP pending interrupt #1 */
		imx_mu_write(IMX_MU_xSR_GIPn(IMX_MU_VERSION, 1),
			     IMX_MU_xSR(IMX_MU_VERSION, IMX_MU_GSR));

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		/* unmask GP interrupt #1 */
		imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GIER, IMX_MU_xCR_GIEn(IMX_MU_VERSION, 1), 0);

		/* host done */
		return 1;
	}

	/* host still pending */
	return 0;
}

int ipc_platform_poll_tx_host_msg(struct ipc_msg *msg)
{
	/* can't send notification when one is in progress */
	if (imx_mu_read(IMX_MU_xCR(IMX_MU_VERSION, IMX_MU_GCR)) &
		IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1))
		return 0;

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	/* now interrupt host to tell it we have sent a message */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GCR, IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1), 0);

	/* message sent */
	return 1;
}

#endif
