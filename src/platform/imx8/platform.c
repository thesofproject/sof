// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/compiler_info.h>
#include <sof/debug/debug.h>
#include <sof/drivers/edma.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/driver.h>
#include <sof/drivers/mu.h>
#include <sof/drivers/timer.h>
#include <sof/fw-ready-metadata.h>
#include <sof/lib/agent.h>
#include <sof/lib/clk.h>
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
#include <version.h>
#include <errno.h>
#include <stdint.h>

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
/* opt-in; reproducible build by default */
#if BLD_COUNTERS
		.build = SOF_BUILD, /* See version-build-counter.cmake */
		.date = __DATE__,
		.time = __TIME__,
#else
		.build = -1,
		.date = "dtermin.\0",
		.time = "fwready.\0",
#endif
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
		.src_hash = SOF_SRC_HASH,
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

#define NUM_IMX_WINDOWS		6

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
		.num_windows	= NUM_IMX_WINDOWS,
		.window	= {
			{
				.type	= SOF_IPC_REGION_UPBOX,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0, // TODO: set later
				.size	= MAILBOX_DSPBOX_SIZE,
				.offset	= MAILBOX_DSPBOX_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_DOWNBOX,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0, // TODO: set later
				.size	= MAILBOX_HOSTBOX_SIZE,
				.offset	= MAILBOX_HOSTBOX_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_DEBUG,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0, // TODO: set later
				.size	= MAILBOX_DEBUG_SIZE,
				.offset	= MAILBOX_DEBUG_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_TRACE,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0, // TODO: set later
				.size	= MAILBOX_TRACE_SIZE,
				.offset	= MAILBOX_TRACE_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_STREAM,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0, // TODO: set later
				.size	= MAILBOX_STREAM_SIZE,
				.offset	= MAILBOX_STREAM_OFFSET,
			},
			{
				.type	= SOF_IPC_REGION_EXCEPTION,
				.id	= 0,	/* map to host window 0 */
				.flags	= 0, // TODO: set later
				.size	= MAILBOX_EXCEPTION_SIZE,
				.offset	= MAILBOX_EXCEPTION_OFFSET,
			},
		},
	}
};

static SHARED_DATA struct timer timer = {
	.id = TIMER0, /* internal timer */
	.irq = IRQ_NUM_TIMER0,
};

int platform_boot_complete(uint32_t boot_message)
{
	mailbox_dspbox_write(0, &ready, sizeof(ready));

	/* now interrupt host to tell it we are done booting */
	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GCR, IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1), 0);

	/* boot now complete so we can relax the CPU */
	/* For now skip this to gain more processing performance
	 * for SRC component.
	 */
	/* clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ); */

	return 0;
}

int platform_init(struct sof *sof)
{
	int ret;

	sof->platform_timer = &timer;
	sof->cpu_timers = &timer;

#ifdef __ZEPHYR__
	/* initialize cascade interrupts before any usage */
	interrupt_init(sof);
#endif

	platform_interrupt_init();
	platform_clock_init(sof);
	scheduler_init_edf();

	/* init low latency domains and schedulers */
	sof->platform_timer_domain =
		timer_domain_init(sof->platform_timer, PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(sof->platform_timer_domain);

#ifndef __ZEPHYR__
	platform_timer_start(sof->platform_timer);
#endif

	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);

	/* init DMA */
	ret = dmac_init(sof);
	if (ret < 0)
		return -ENODEV;

	/* Init EDMA platform domain */
	sof->platform_dma_domain = dma_multi_chan_domain_init
			(&sof->dma_info->dma_array[0], 1,
			 PLATFORM_DEFAULT_CLOCK, false);

	/* i.MX platform DMA domain will be full synchronous, no time dependent */
	sof->platform_dma_domain->full_sync = true;
	scheduler_init_ll(sof->platform_dma_domain);

	/* initialize the host IPC mechanims */
	ret = ipc_init(sof);
	if (ret < 0)
		return ret;

	ret = dai_init(sof);
	if (ret < 0)
		return -ENODEV;

#ifndef __ZEPHYR__
#if CONFIG_TRACE
	/* Initialize DMA for Trace*/
	trace_point(TRACE_BOOT_PLATFORM_DMA_TRACE);
	dma_trace_init_complete(sof->dmat);
#endif

	/* show heap status */
	heap_trace_all(1);
#endif /* __ZEPHYR__ */

	return 0;
}

int platform_context_save(struct sof *sof)
{
	return 0;
}
