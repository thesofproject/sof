/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <platform/memory.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <platform/dai.h>
#include <platform/clk.h>
#include <platform/timer.h>
#include <platform/platcfg.h>
#include <uapi/ipc/info.h>
#include <sof/mailbox.h>
#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/interrupt.h>
#include <sof/sof.h>
#include <sof/work.h>
#include <sof/clk.h>
#include <sof/ipc.h>
#include <sof/trace.h>
#include <sof/agent.h>
#include <sof/io.h>
#include <sof/dma-trace.h>
#include <sof/audio/component.h>
#include <sof/drivers/timer.h>
#include <sof/cpu.h>
#include <sof/notifier.h>
#include <config.h>
#include <string.h>
#include <version.h>

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
#ifdef CONFIG_DEBUG
		/* only added in debug for reproducability in releases */
		.build = SOF_BUILD,
		.date = __DATE__,
		.time = __TIME__,
#endif
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
	},
	.debug = DEBUG_SET_FW_READY_FLAGS,
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

struct work_queue_timesource platform_generic_queue[] = {
{
	.timer	 = {
		.id = TIMER1,	/* internal timer */
		.irq = IRQ_NUM_TIMER2,
	},
	.clk		= CLK_CPU(0),
	.notifier	= NOTIFIER_ID_CPU_FREQ,
	.timer_set	= arch_timer_set,
	.timer_clear	= arch_timer_clear,
	.timer_get	= arch_timer_get_system,
},
};

struct timer *platform_timer =
	&platform_generic_queue[PLATFORM_MASTER_CORE_ID].timer;

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

	trace_point(TRACE_BOOT_PLATFORM_MBOX);

	/* clear mailbox for early trace and debug */
	bzero((void *)MAILBOX_BASE, IPC_MAX_MAILBOX_BYTES);

	trace_point(TRACE_BOOT_PLATFORM_SHIM);
	platform_init_shim();

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	clock_init();

	/* init work queues and clocks */
	trace_point(TRACE_BOOT_SYS_WORK);
	init_system_workq(&platform_generic_queue[PLATFORM_MASTER_CORE_ID]);

	trace_point(TRACE_BOOT_PLATFORM_TIMER);
	platform_timer_start(platform_timer);

	/* init the system agent */
	sa_init(sof);

	/* Set CPU to default frequency for booting */
	trace_point(TRACE_BOOT_SYS_CPU_FREQ);
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

	/* Initialize DMA for Trace*/
	dma_trace_init_complete(sof->dmat);

	/* show heap status */
	heap_trace_all(1);

	return 0;
}
