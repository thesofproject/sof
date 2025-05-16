// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2024 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <rtos/clk.h>
#include <platform/lib/memory.h>
#include <kernel/ext_manifest.h>
#include <sof/platform.h>
#include <sof/debug/debug.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/lib/agent.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof_versions.h>
#include <sof/ipc/schedule.h>

/* General platform glue code.  In a Zephyr build, most of this is
 * vestigial or degenerate, or at least evolving in that direction.
 */

void mtk_dai_init(struct sof *sof);

#define MBOX0 DEVICE_DT_GET(DT_INST(0, mediatek_mbox))
#define MBOX1 DEVICE_DT_GET(DT_INST(1, mediatek_mbox))

/* Use the same UUID as in "ipc-zephyr.c", which is actually an Intel driver */
SOF_DEFINE_REG_UUID(zipc_task);

static void mbox_cmd_fn(const struct device *mbox, void *arg)
{
	/* We're in ISR context.  This unblocks the IPC task thread,
	 * which calls ipc_do_cmd(), which calls back into
	 * ipc_platform_do_cmd() below, which then calls ipc_cmd().
	 */
	ipc_schedule_process(ipc_get());
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	/* mailbox_validate() checks the command length (that's all it
	 * vaildates) and copies the incoming command from the host
	 * window to the comp_data buffer in the IPC object.
	 */
	struct ipc_cmd_hdr *hdr = mailbox_validate();

	if (hdr)
		ipc_cmd(hdr);
	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	mtk_adsp_mbox_signal(MBOX0, 1);
}

static void mtk_ipc_send(const void *msg, size_t sz)
{
	mailbox_dspbox_write(0, msg, sz);
	mtk_adsp_mbox_signal(MBOX1, 0);
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();

	if (ipc->is_notification_pending)
		return -EBUSY;

	ipc->is_notification_pending = true;
	mtk_ipc_send(msg->tx_data, msg->tx_size);
	return 0;
}

static void mbox_reply_fn(const struct device *mbox, void *arg)
{
	ipc_get()->is_notification_pending = false;
}

/* "Host Page Table" support.  The platform is responsible for
 * providing a buffer into which the IPC layer reads a DMA "page
 * table" from the host.  This isn't really a page table, it's a
 * packed array of PPN addresses (basically a scatter/gather list)
 * used to configure the buffer used for dummy_dma, which is a "DMA"
 * driver that works by directly copying data in shared memory.  And
 * somewhat confusingly, it's itself configured at runtime by "DMA"
 * over the same mechanism (instead of e.g. by a IPC command, which
 * would fit just fine!).  All of this is degenerate with MTK anyway,
 * because the actual addresses being passed are in a DRAM region
 * dedicated for the purpose and are AFAICT guaranteed contiguous.
 *
 * Note: the 256 byte page table size is fixed by protocol in the
 * linux driver, but here in SOF it's always been a platform symbol.
 * But it's not tunable!  Don't touch it.
 */
static uint8_t hostbuf_ptable[256];
static struct ipc_data_host_buffer mtk_host_buffer;

struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc)
{
	return &mtk_host_buffer;
}

/* Called out of ipc_init(), which is called out of platform_init() below */
int platform_ipc_init(struct ipc *ipc)
{
	mtk_host_buffer.page_table = hostbuf_ptable;
	mtk_host_buffer.dmac = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST,
				       DMA_ACCESS_SHARED);

	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(zipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	mtk_adsp_mbox_set_handler(MBOX0, 0, mbox_cmd_fn, NULL);
	mtk_adsp_mbox_set_handler(MBOX1, 1, mbox_reply_fn, NULL);
	return 0;
}

int platform_context_save(struct sof *sof)
{
	return 0;
}

static int set_cpuclk(int clock, int hz)
{
	return clock == 0 && hz == CONFIG_XTENSA_CCOUNT_HZ ? 0 : -EINVAL;
}

/* This is required out of dma_multi_chan_domain but nothing
 * defines it in Zephyr builds.  Stub with a noop here,
 * knowing that MTK "DMA" "devices" don't have interrupts.
 */
void interrupt_clear_mask(uint32_t irq, uint32_t mask)
{
}

/* Dummy CPU clock driver that supports one known frequency.  This
 * hardware has clock scaling support, but it hasn't historically been
 * exercised so we have nothing to test against.
 */
void clocks_init(struct sof *sof)
{
	static const struct freq_table freqs[] = {
		{ .freq = CONFIG_XTENSA_CCOUNT_HZ,
		  .ticks_per_msec = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000, }
	};
	static struct clock_info clks[] = {
		{ .freqs_num = ARRAY_SIZE(freqs),
		  .freqs = freqs,
		  .notification_id = NOTIFIER_ID_CPU_FREQ,
		  .notification_mask = NOTIFIER_TARGET_CORE_MASK(0),
		  .set_freq = set_cpuclk, },
	};
	sof->clocks = clks;
}

int platform_init(struct sof *sof)
{
	clocks_init(sof);
	scheduler_init_edf();
	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);
	mtk_dai_init(sof);
	ipc_init(sof);
	sof->platform_dma_domain =
		dma_multi_chan_domain_init(&sof->dma_info->dma_array[0],
					   sof->dma_info->num_dmas,
					   PLATFORM_DEFAULT_CLOCK, false);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);
	return 0;
}

int platform_boot_complete(uint32_t boot_message)
{
	static const struct sof_ipc_fw_ready fw_ready_cmd = {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_fw_ready),
		.version = {
			.hdr.size = sizeof(struct sof_ipc_fw_version),
			.micro = SOF_MICRO,
			.minor = SOF_MINOR,
			.major = SOF_MAJOR,
			.tag = SOF_TAG,
			.abi_version = SOF_ABI_VERSION,
			.src_hash = SOF_SRC_HASH,
		},
		.flags = DEBUG_SET_FW_READY_FLAGS,
	};

	mtk_ipc_send(&fw_ready_cmd, sizeof(fw_ready_cmd));
	return 0;
}

/* Extended manifest window record.  Note the alignment attribute is
 * critical as rimage demands allocation in units of 16 bytes, yet the
 * C struct records emitted into the section are not in general padded
 * and will pack tighter than that!  (Really this is an rimage bug, it
 * should separately validate each symbol in the section and re-pack
 * the array instead of relying on the poor linker to do it).
 */
#define WINDOW(region)				\
	{ .type = SOF_IPC_REGION_##region,	\
	  .size = MTK_IPC_WIN_SIZE(region),	\
	.  offset = MTK_IPC_WIN_OFF(region), }
struct ext_man_windows mtk_man_win __section(".fw_metadata") __aligned(EXT_MAN_ALIGN) = {
	.hdr = {
		.type = EXT_MAN_ELEM_WINDOW,
		.elem_size = ROUND_UP(sizeof(struct ext_man_windows), EXT_MAN_ALIGN)
	},
	.window = {
		.ext_hdr = {
			.hdr.cmd = SOF_IPC_FW_READY,
			.hdr.size = sizeof(struct sof_ipc_window),
			.type = SOF_IPC_EXT_WINDOW,
		},
		.num_windows = 6,
		.window = {
			// Order doesn't match memory layout for historical
			// reasons.  Shouldn't matter, but don't rock the boat...
			WINDOW(UPBOX),
			WINDOW(DOWNBOX),
			WINDOW(DEBUG),
			WINDOW(TRACE),
			WINDOW(STREAM),
			WINDOW(EXCEPTION),
		},
	},
};
