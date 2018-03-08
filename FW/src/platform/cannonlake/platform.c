/*
 * Copyright (c) 2017, Intel Corporation
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
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#include <platform/memory.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <platform/clk.h>
#include <platform/timer.h>
#include <platform/interrupt.h>
#include <uapi/ipc.h>
#include <reef/mailbox.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/reef.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/ipc.h>
#include <reef/agent.h>
#include <reef/io.h>
#include <reef/trace.h>
#include <reef/audio/component.h>
#include <config.h>
#include <string.h>
#include <version.h>

static const struct sof_ipc_fw_ready ready = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
	.version = {
		.build = REEF_BUILD,
		.minor = REEF_MINOR,
		.major = REEF_MAJOR,
		.date = __DATE__,
		.time = __TIME__,
		.tag = REEF_TAG,
	},
};

#define SRAM_WINDOW_HOST_OFFSET(x)	(0x80000 + x * 0x20000)

#define NUM_CNL_WINDOWS			5

static const struct sof_ipc_window sram_window = {
	.ext_hdr	= {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_window) +
			sizeof(struct sof_ipc_window_elem) * NUM_CNL_WINDOWS,
		.type	= SOF_IPC_EXT_WINDOW,
	},
	.num_windows	= NUM_CNL_WINDOWS,
	.window[0]	= {
		.type	= SOF_IPC_REGION_REGS,
		.id	= 0,	/* map to host window 0 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_SW_REG_SIZE,
		.offset	= 0,
	},
	.window[1]	= {
		.type	= SOF_IPC_REGION_UPBOX,
		.id	= 0,	/* map to host window 0 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_DSPBOX_SIZE,
		.offset	= MAILBOX_SW_REG_SIZE,
	},
	.window[2]	= {
		.type	= SOF_IPC_REGION_DOWNBOX,
		.id	= 1,	/* map to host window 1 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_HOSTBOX_SIZE,
		.offset	= 0,
	},
	.window[3]	= {
		.type	= SOF_IPC_REGION_DEBUG,
		.id	= 2,	/* map to host window 2 */
		.flags	= 0, // TODO: set later
		.size	= SRAM_DEBUG_SIZE,
		.offset	= 0,
	},
	.window[4]	= {
		.type	= SOF_IPC_REGION_TRACE,
		.id	= 3,	/* map to host window 3 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_TRACE_SIZE,
		.offset	= 0,
	},

};

static struct work_queue_timesource platform_generic_queue = {
	.timer	 = {
		.id = TIMER3, /* external timer */
		.irq = IRQ_EXT_TSTAMP0_LVL2(0),
	},
	.clk		= CLK_CPU,
	.notifier	= NOTIFIER_ID_CPU_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
};

struct timer *platform_timer = &platform_generic_queue.timer;

int platform_boot_complete(uint32_t boot_message)
{
	mailbox_dspbox_write(0, &ready, sizeof(ready));
	mailbox_dspbox_write(sizeof(ready), &sram_window,
		sram_window.ext_hdr.hdr.size);

	/* boot now complete so we can relax the CPU */
	clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ);

	/* tell host we are ready */
	ipc_write(IPC_DIPCIDD, SRAM_WINDOW_HOST_OFFSET(0) >> 12);
	ipc_write(IPC_DIPCIDR, 0x80000000 | SOF_IPC_FW_READY);

	return 0;
}

static void platform_memory_windows_init(void)
{
	/* window0, for fw status & outbox/uplink mbox */
	io_reg_write(DMWLO(0), HP_SRAM_WIN0_SIZE | 0x7);
	io_reg_write(DMWBA(0), HP_SRAM_WIN0_BASE
		| DMWBA_READONLY | DMWBA_ENABLE);

	/* window1, for inbox/downlink mbox */
	io_reg_write(DMWLO(1), HP_SRAM_WIN1_SIZE | 0x7);
	io_reg_write(DMWBA(1), HP_SRAM_WIN1_BASE
		| DMWBA_ENABLE);

	/* window2, for debug */
	io_reg_write(DMWLO(2), HP_SRAM_WIN2_SIZE | 0x7);
	io_reg_write(DMWBA(2), HP_SRAM_WIN2_BASE
		| DMWBA_READONLY | DMWBA_ENABLE);

	/* window3, for trace */
	io_reg_write(DMWLO(3), HP_SRAM_WIN3_SIZE | 0x7);
	io_reg_write(DMWBA(3), HP_SRAM_WIN3_BASE
		| DMWBA_READONLY | DMWBA_ENABLE);
}

/* init HW  */
static void platform_init_hw(void)
{
	io_reg_write(DSP_INIT_GENO,
		GENO_MDIVOSEL | GENO_DIOPTOSEL);

	io_reg_write(DSP_INIT_IOPO,
		IOPO_DMIC_FLAG |IOPO_I2S_FLAG);

	io_reg_write(DSP_INIT_ALHO,
		ALHO_ASO_FLAG | ALHO_CSO_FLAG | ALHO_CFO_FLAG);

	io_reg_write(DSP_INIT_LPGPDMA(0),
		LPGPDMA_CHOSEL_FLAG | LPGPDMA_CTLOSEL_FLAG);
	io_reg_write(DSP_INIT_LPGPDMA(1),
		LPGPDMA_CHOSEL_FLAG | LPGPDMA_CTLOSEL_FLAG);
}

static struct timer platform_ext_timer = {
	.id = TIMER3,
	.irq = IRQ_EXT_TSTAMP0_LVL2(0),
};

int platform_init(struct reef *reef)
{
	struct dma *dmac;
	struct dai *ssp;
	int i;

	trace_point(TRACE_BOOT_PLATFORM_ENTRY);
	platform_init_hw();

	platform_interrupt_init();

	trace_point(TRACE_BOOT_PLATFORM_MBOX);
	platform_memory_windows_init();

	trace_point(TRACE_BOOT_PLATFORM_SHIM);

	/* init work queues and clocks */
	trace_point(TRACE_BOOT_PLATFORM_TIMER);
	platform_timer_start(&platform_ext_timer);

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	init_platform_clocks();

	trace_point(TRACE_BOOT_SYS_WORK);
	init_system_workq(&platform_generic_queue);

	/* init the system agent */
	//sa_init(reef);

	/* Set CPU to default frequency for booting */
	trace_point(TRACE_BOOT_SYS_CPU_FREQ);
	clock_set_freq(CLK_CPU, CLK_MAX_CPU_HZ);

	/* set SSP clock to 24M */
	trace_point(TRACE_BOOT_PLATFORM_SSP_FREQ);
	clock_set_freq(CLK_SSP, 24000000);

	/* initialise the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init(reef);

	/* prevent Core0 clock gating. */
	shim_write(SHIM_CLKCTL, shim_read(SHIM_CLKCTL) |
		SHIM_CLKCTL_TCPLCG(0));

	/* prevent LP GPDMA 0&1 clock gating */
	io_reg_write(GPDMA_CLKCTL(0), GPDMA_FDCGB);
	io_reg_write(GPDMA_CLKCTL(1), GPDMA_FDCGB);

	/* prevent DSP Common power gating */
	shim_write16(SHIM_PWRCTL, SHIM_PWRCTL_TCPDSP0PG);

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	dmac = dma_get(DMA_GP_LP_DMAC0);
	if (!dmac)
		return -ENODEV;
	dma_probe(dmac);

	dmac = dma_get(DMA_HOST_OUT_DMAC);
	if (!dmac)
		return -ENODEV;
	dma_probe(dmac);

	dmac = dma_get(DMA_HOST_IN_DMAC);
	if (!dmac)
		return -ENODEV;
	dma_probe(dmac);

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	for(i = 0; i < PLATFORM_SSP_COUNT; i++) {
		ssp = dai_get(SOF_DAI_INTEL_SSP, i);
		if (ssp == NULL)
			return -ENODEV;
		dai_probe(ssp);
	}

	return 0;
}
