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
	return 0; /* number of words read - not currently used on this platform */
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;
	/* Use struct ipc_data *iipc = ipc_get_drvdata(ipc); if needed */

	/* perform command */
	hdr = mailbox_validate();
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

	/* now send the message */
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

	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->dh_buffer.page_table = rzalloc(SOF_MEM_FLAG_USER,
					     PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->dh_buffer.page_table)
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
