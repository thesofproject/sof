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
#include <platform/pmc.h>
#include <platform/interrupt.h>
#include <uapi/ipc.h>
#include <reef/mailbox.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/reef.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/ipc.h>
#include <reef/trace.h>
#include <reef/audio/component.h>
#include <string.h>
#include <version.h>

static const struct sof_ipc_fw_ready ready = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},

	/* for host, we need exchange the naming of inxxx and outxxx */
	.hostbox_offset = 0,
	.dspbox_offset = 0,
	.hostbox_size = 0x1000,
	.dspbox_size = 0x1000,
	.version = {
		.build = REEF_BUILD,
		.minor = REEF_MINOR,
		.major = REEF_MAJOR,
		.date = __DATE__,
		.time = __TIME__,
		.tag = REEF_TAG,
	},
};

#define SRAM_WINDOW_HOST_OFFSET(x)		(0x80000 + x * 0x20000)

#define NUM_BXT_WINDOWS		5

static const struct sof_ipc_window sram_window = {
	.ext_hdr	= {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_window) +
			sizeof(struct sof_ipc_window_elem) * NUM_BXT_WINDOWS,
		.type	= SOF_IPC_EXT_WINDOW,
	},
	.num_windows 	= NUM_BXT_WINDOWS,
	.window[0] 	= {
		.type 	= SOF_IPC_REGION_REGS,
		.id	= 0,	/* map to host window 0 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_SW_REG_SIZE,
		.offset	= 0,
	},
	.window[1] 	= {
		.type 	= SOF_IPC_REGION_UPBOX,
		.id	= 0,	/* map to host window 0 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_DSPBOX_SIZE,
		.offset	= MAILBOX_SW_REG_SIZE,
	},
	.window[2] 	= {
		.type 	= SOF_IPC_REGION_DOWNBOX,
		.id	= 1,	/* map to host window 1 */
		.flags	= 0, // TODO: set later
		.size	= MAILBOX_HOSTBOX_SIZE,
		.offset	= 0,
	},
	.window[3] 	= {
		.type 	= SOF_IPC_REGION_DEBUG,
		.id	= 2,	/* map to host window 2 */
		.flags	= 0, // TODO: set later
		.size	= SRAM_DEBUG_SIZE,
		.offset	= 0,
	},
	.window[4] 	= {
		.type 	= SOF_IPC_REGION_TRACE,
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
	mailbox_dspbox_write(sizeof(ready), &sram_window, sram_window.ext_hdr.hdr.size);

	/* boot now complete so we can relax the CPU */
	clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ);

	ipc_write(IPC_DIPCIDD, SRAM_WINDOW_HOST_OFFSET(0) >> 12);
	ipc_write(IPC_DIPCIDR, 0x80000000 | SOF_IPC_FW_READY);

	return 0;
}

struct ssp_mn {
	uint32_t source;
	uint32_t bclk_fs;
	uint32_t rate;
	uint32_t m;
	uint32_t n;
};

#if 0
static const struct ssp_mn ssp_mn_conf[] = {
	{25000000, 24, 48000, 1152, 25000},     /* 1.152MHz */
	{25000000, 32, 48000, 1536, 25000},	/* 1.536MHz */
	{25000000, 64, 48000, 3072, 25000},	/* 3.072MHz */
	{25000000, 400, 48000, 96, 125},	/* 19.2MHz */
	{25000000, 400, 44100, 441, 625},	/* 17.64MHz */
};
#endif

/* set the SSP M/N clock dividers */
int platform_ssp_set_mn(uint32_t ssp_port, uint32_t source, uint32_t rate,
	uint32_t bclk_fs)
{
#if 0
	int i;

	/* check for matching config in the table */
	for (i = 0; i < ARRAY_SIZE(ssp_mn_conf); i++) {

		if (ssp_mn_conf[i].source != source)
			continue;

		if (ssp_mn_conf[i].rate != rate)
			continue;

		if (ssp_mn_conf[i].bclk_fs != bclk_fs)
			continue;

		/* match */
		switch (ssp_port) {
		case 0:
			shim_write(SHIM_SSP0_DIVL, ssp_mn_conf[i].n);
			shim_write(SHIM_SSP0_DIVH, SHIM_SSP_DIV_ENA |
				SHIM_SSP_DIV_UPD | ssp_mn_conf[i].m);
			break;
		case 1:
			shim_write(SHIM_SSP1_DIVL, ssp_mn_conf[i].n);
			shim_write(SHIM_SSP1_DIVH, SHIM_SSP_DIV_ENA |
				SHIM_SSP_DIV_UPD | ssp_mn_conf[i].m);
			break;
		case 2:
			shim_write(SHIM_SSP2_DIVL, ssp_mn_conf[i].n);
			shim_write(SHIM_SSP2_DIVH, SHIM_SSP_DIV_ENA |
				SHIM_SSP_DIV_UPD | ssp_mn_conf[i].m);
			break;
		default:
			return -ENODEV;
		}

		return 0;
	}
#endif
	return -EINVAL;
}

void platform_ssp_disable_mn(uint32_t ssp_port)
{
#if 0
	switch (ssp_port) {
	case 0:
		shim_write(SHIM_SSP0_DIVH, SHIM_SSP_DIV_BYP |
			SHIM_SSP_DIV_UPD);
		break;
	case 1:
		shim_write(SHIM_SSP1_DIVH, SHIM_SSP_DIV_BYP |
			SHIM_SSP_DIV_UPD);
		break;
	case 2:
		shim_write(SHIM_SSP2_DIVH, SHIM_SSP_DIV_BYP |
			SHIM_SSP_DIV_UPD);
		break;
	}
#endif
}

static void platform_memory_windows_init(void)
{
	/* window0, for fw status & outbox/uplink mbox */
	*((volatile uint32_t*)(HOST_WIN_BASE(0) + 0)) =
		HP_SRAM_WIN0_BASE |0x3; /* read only for host */
	*((volatile uint32_t*)(HOST_WIN_BASE(0) + 4)) =
		HP_SRAM_WIN0_SIZE |0x7; /* limit offset, 8 KB */

	/* window1, for inbox/downlink mbox */
	*((volatile uint32_t*)(HOST_WIN_BASE(1) + 0)) =
		HP_SRAM_WIN1_BASE |0x1; /* writable for host */
	*((volatile uint32_t*)(HOST_WIN_BASE(1) + 4)) =
		HP_SRAM_WIN1_SIZE | 0x7; /* limit offset, 8 KB */

	/* window2, for debug */
	*((volatile uint32_t*)(HOST_WIN_BASE(2) + 0)) =
		HP_SRAM_WIN2_BASE |0x3; /* read only for host */
	*((volatile uint32_t*)(HOST_WIN_BASE(2) + 4)) =
		HP_SRAM_WIN2_SIZE | 0x7; /* limit offset, 4 KB */

	/* window3, for trace */
	*((volatile uint32_t*)(HOST_WIN_BASE(3) + 0)) =
		HP_SRAM_WIN3_BASE |0x3; /* read only for host */
	*((volatile uint32_t*)(HOST_WIN_BASE(3) + 4)) =
		HP_SRAM_WIN3_SIZE | 0x7; /* limit offset, 8 KB */
}

/* set resource owner to dsp, not host cpu */
static void platform_set_resource_owner(void)
{
	/* set general owner to dsp */
	io_reg_write(DSP_RESOURCE_GENO, GENO_MDIVOSEL |
					GENO_DIOPTOSEL);

	/* set IO owner to dsp */
	io_reg_write(DSP_RESOURCE_IOPO, IOPO_DMIC_FLAG |
					IOPO_I2S_FLAG);

	/* set audio link hub owner to dsp */
	io_reg_write(DSP_RESOURCE_ALHO, ALHO_ASO_FLAG |
					ALHO_CSO_FLAG |
					ALHO_CFO_FLAG);

	/* set GPDMA owner to dsp */
	io_reg_write(DSP_RESOURCE_LPGPDMA(0), LPGPDMA_CHOSEL_FLAG |
						LPGPDMA_CTLOSEL_FLAG);
	io_reg_write(DSP_RESOURCE_LPGPDMA(1), LPGPDMA_CHOSEL_FLAG |
						LPGPDMA_CTLOSEL_FLAG);
}

static struct timer platform_ext_timer = {
	.id = TIMER3,
	.irq = IRQ_EXT_TSTAMP0_LVL2(0),
};

int platform_init(struct reef *reef)
{
	struct dma *dmac0;
	struct dai *ssp;
	int i;

	trace_point(TRACE_BOOT_PLATFORM_RESOURCE);
	platform_set_resource_owner();

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

	/* Set CPU to default frequency for booting */
	trace_point(TRACE_BOOT_SYS_CPU_FREQ);
	clock_set_freq(CLK_CPU, CLK_MAX_CPU_HZ);

	/* set SSP clock to 25M */
	trace_point(TRACE_BOOT_PLATFORM_SSP_FREQ);
	clock_set_freq(CLK_SSP, 25000000);

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
	dmac0 = dma_get(DMA_GP_LP_DMAC0);
	if (dmac0 == NULL)
		return -ENODEV;
	dma_probe(dmac0);

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	for(i = 0; i < MAX_SSP_COUNT; i++) {
		ssp = dai_get(SOF_DAI_INTEL_SSP, i);
		if (ssp == NULL)
			return -ENODEV;
		dai_probe(ssp);
	}

	return 0;
}
