// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/debug/debug.h>
#include <sof/drivers/dw-dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/notifier.h>
#include <sof/lib/shim.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/sof.h>
#include <sof/trace/dma-trace.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <version.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static const struct sof_ipc_fw_ready ready
	__attribute__((section(".fw_ready"))) = {
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
#if CONFIG_DEBUG
		/* only added in debug for reproducability in releases */
		.build = SOF_BUILD,
		.date = __DATE__,
		.time = __TIME__,
#endif
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

#define NUM_HSW_WINDOWS		6
static const struct sof_ipc_window sram_window = {
	.ext_hdr	= {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_window) +
			sizeof(struct sof_ipc_window_elem) * NUM_HSW_WINDOWS,
		.type	= SOF_IPC_EXT_WINDOW,
	},
	.num_windows	= NUM_HSW_WINDOWS,
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
};

struct timer timer = {
	.id = TIMER1, /* internal timer */
	.irq = IRQ_NUM_TIMER2,
};

struct timer *platform_timer = &timer;

struct ll_schedule_domain *platform_timer_domain;
struct ll_schedule_domain *platform_dma_domain;

int platform_boot_complete(uint32_t boot_message)
{
	uint32_t outbox = MAILBOX_HOST_OFFSET >> 3;

	mailbox_dspbox_write(0, &ready, sizeof(ready));
	mailbox_dspbox_write(sizeof(ready), &sram_window,
			     sram_window.ext_hdr.hdr.size);

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCD, outbox | SHIM_IPCD_BUSY);

	/* boot now complete so we can relax the CPU */
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_DEFAULT_CPU_HZ);

	return 0;
}

/* init shim registers */
static void platform_init_shim(void)
{
	/* disable power gate */
	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL,
			   SHIM_CLKCTL_DCPLCG,
			   SHIM_CLKCTL_DCPLCG);

	/* disable parity check */
	io_reg_update_bits(SHIM_BASE + SHIM_CSR, SHIM_CSR_PCE, 0);

	/* enable DMA finsh on ssp ports */
	io_reg_update_bits(SHIM_BASE + SHIM_CSR2,
			   SHIM_CSR2_SDFD_SSP0 | SHIM_CSR2_SDFD_SSP1,
			   SHIM_CSR2_SDFD_SSP0 | SHIM_CSR2_SDFD_SSP1);
}

int platform_init(struct sof *sof)
{
	struct dai *ssp0;
	struct dai *ssp1;
	int ret;

	/* clear mailbox for early trace and debug */
	trace_point(TRACE_BOOT_PLATFORM_MBOX);
	bzero((void *)MAILBOX_BASE, MAILBOX_SIZE);

	trace_point(TRACE_BOOT_PLATFORM_SHIM);
	platform_init_shim();

	/* init timers, clocks and schedulers */
	trace_point(TRACE_BOOT_PLATFORM_TIMER);
	platform_timer_start(platform_timer);

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	clock_init();

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_edf();

	/* init low latency timer domain and scheduler */
	platform_timer_domain =
		timer_domain_init(platform_timer, PLATFORM_DEFAULT_CLOCK,
				  CONFIG_SYSTICK_PERIOD);
	scheduler_init_ll(platform_timer_domain);

	/* init low latency multi channel DW-DMA domain and scheduler */
	platform_dma_domain =
		dma_multi_chan_domain_init(
				&dma[PLATFORM_DW_DMA_INDEX],
				PLATFORM_NUM_DW_DMACS,
				PLATFORM_DEFAULT_CLOCK, true);
	scheduler_init_ll(platform_dma_domain);

	/* init the system agent */
	trace_point(TRACE_BOOT_PLATFORM_AGENT);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* Set CPU to default frequency for booting */
	trace_point(TRACE_BOOT_PLATFORM_CPU_FREQ);
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);

	/* set SSP clock to 25M */
	trace_point(TRACE_BOOT_PLATFORM_SSP_FREQ);
	clock_set_freq(CLK_SSP, 25000000);

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	ret = dmac_init();
	if (ret < 0)
		return -ENODEV;

	/* initialise the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init(sof);

	trace_point(TRACE_BOOT_PLATFORM_DAI);
	ret = dai_init();
	if (ret < 0)
		return -ENODEV;

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	ssp0 = dai_get(SOF_DAI_INTEL_SSP, 0, DAI_CREAT);
	if (ssp0 == NULL)
		return -ENODEV;
	dai_probe(ssp0);

	ssp1 = dai_get(SOF_DAI_INTEL_SSP, 1, DAI_CREAT);
	if (ssp1 == NULL)
		return -ENODEV;
	dai_probe(ssp1);

#if CONFIG_TRACE
	/* Initialize DMA for Trace*/
	trace_point(TRACE_BOOT_PLATFORM_DMA_TRACE);
	dma_trace_init_complete(sof->dmat);
#endif

	/* show heap status */
	heap_trace_all(1);

	return 0;
}
