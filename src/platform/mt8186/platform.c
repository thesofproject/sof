// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek. All rights reserved.
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
//         Tinghan Shen <tinghan.shen@mediatek.com>

#include <sof/compiler_info.h>
#include <sof/debug/debug.h>
#include <sof/drivers/edma.h>
#include <rtos/interrupt.h>
#include <sof/ipc/msg.h>
#include <rtos/timer.h>
#include <sof/fw-ready-metadata.h>
#include <sof/lib/agent.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/sof.h>
#include <sof/trace/dma-trace.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <kernel/ext_manifest.h>
#include <sof_versions.h>
#include <errno.h>
#include <stdint.h>
#include <xtensa/hal.h>
#include <platform/drivers/timer.h>

struct sof;

static const struct sof_ipc_fw_ready ready
	__section(".fw_ready") = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
	/* dspbox is for DSP initiated IPC, hostbox is for host initiated IPC */
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

#define NUM_MTK_WINDOWS 6

const struct ext_man_windows xsram_window
		__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") __unused = {
	.hdr = {
		.type = EXT_MAN_ELEM_WINDOW,
		.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_windows), EXT_MAN_ALIGN),
	},
	.window = {
		.ext_hdr	= {
			.hdr.cmd = SOF_IPC_FW_READY,
			.hdr.size = sizeof(struct sof_ipc_window),
			.type	= SOF_IPC_EXT_WINDOW,
		},
		.num_windows	= NUM_MTK_WINDOWS,
		.window	= {
			{
				.type	= SOF_IPC_REGION_UPBOX,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0,
				.size	= MAILBOX_DSPBOX_SIZE,
				.offset	= MAILBOX_DSPBOX_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_DOWNBOX,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0,
				.size	= MAILBOX_HOSTBOX_SIZE,
				.offset	= MAILBOX_HOSTBOX_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_DEBUG,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0,
				.size	= MAILBOX_DEBUG_SIZE,
				.offset	= MAILBOX_DEBUG_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_TRACE,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0,
				.size	= MAILBOX_TRACE_SIZE,
				.offset	= MAILBOX_TRACE_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_STREAM,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0,
				.size	= MAILBOX_STREAM_SIZE,
				.offset	= MAILBOX_STREAM_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_EXCEPTION,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0,
				.size	= MAILBOX_EXCEPTION_SIZE,
				.offset	= MAILBOX_EXCEPTION_OFFSET,
			},
		},
	}
};

static SHARED_DATA struct timer timer_shared = {
	.id = OSTIMER0,
	.irq = MTK_DSP_IRQ_OSTIMER32,
};

/* Override the default MPU setup. This table matches the memory map
 * of the 'sample_controller' core and will need to be modified for
 * other cores.
 * NOTE: This table sets up all of external memory as shared uncached.
 * For best results, edit the LSP memory map to create a separate
 * section in shared memory, place all sections that need to be uncached
 * into that section, and only map that section uncached. See README
 * for more details.
 */
const struct xthal_MPU_entry __xt_mpu_init_table[] __section(".ResetVector.text") = {
	XTHAL_MPU_ENTRY(0x00000000, 1, XTHAL_AR_RWXrwx, XTHAL_MEM_DEVICE),		// unused
	XTHAL_MPU_ENTRY(0x4e100000, 1, XTHAL_AR_RWXrwx, XTHAL_MEM_WRITEBACK),		// sram
	XTHAL_MPU_ENTRY(0x4e180000, 1, XTHAL_AR_NONE, XTHAL_MEM_DEVICE),		// unused
	XTHAL_MPU_ENTRY(0x60000000, 1, XTHAL_AR_RWXrwx, XTHAL_MEM_WRITEBACK),		// dram
	XTHAL_MPU_ENTRY(0x60500000, 1, XTHAL_AR_RWXrwx, XTHAL_MEM_NON_CACHEABLE),	// dram
	XTHAL_MPU_ENTRY(0x61100000, 1, XTHAL_AR_NONE, XTHAL_MEM_DEVICE),		// unused
};

const unsigned int __xt_mpu_init_table_size __section(".ResetVector.text") =
	ARRAY_SIZE(__xt_mpu_init_table);

int platform_boot_complete(uint32_t boot_message)
{
	mailbox_dspbox_write(0, &ready, sizeof(ready));

	/* now interrupt host to tell it we are done booting */
	trigger_irq_to_host_req();

	/* boot now complete so we can relax the CPU */
	/* For now skip this to gain more processing performance
	 * for SRC component.
	 */
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);

	return 0;
}

extern void dump_adsppll(int line);

int platform_init(struct sof *sof)
{
	int ret;

	sof->platform_timer = platform_shared_get(&timer_shared, sizeof(timer_shared));
	sof->cpu_timers = sof->platform_timer;

	platform_interrupt_init();
	platform_clock_init(sof);

	scheduler_init_edf();

	/* init low latency domains and schedulers */
	sof->platform_timer_domain = timer_domain_init(sof->platform_timer, PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);
	platform_timer_start(sof->platform_timer);

	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* init DMA */
	ret = dmac_init(sof);
	if (ret < 0)
		return -ENODEV;

	/* Init platform domain */
	sof->platform_dma_domain = dma_multi_chan_domain_init(&sof->dma_info->dma_array[0], 1,
							      PLATFORM_DEFAULT_CLOCK, false);
	scheduler_init_ll(sof->platform_dma_domain);

	/* initialize the host IPC mechanims */
	ipc_init(sof);

	ret = dai_init(sof);
	if (ret < 0)
		return ret;

#if CONFIG_TRACE
	/* Initialize DMA for Trace*/
	trace_point(TRACE_BOOT_PLATFORM_DMA_TRACE);
	dma_trace_init_complete(sof->dmat);
#endif

	/* show heap status */
	heap_trace_all(1);

	return 0;
}

int platform_context_save(struct sof *sof)
{
	return 0;
}

void platform_wait_for_interrupt(int level)
{
	arch_wait_for_interrupt(level);
}

