/*
 * Copyright (c) 2018, Intel Corporation
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
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#include <platform/memory.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <platform/dai.h>
#include <platform/clk.h>
#if defined(CONFIG_ICELAKE) || defined(CONFIG_SUECREEK)
#include <platform/clk-map.h>
#endif
#include <platform/timer.h>
#include <platform/interrupt.h>
#include <platform/idc.h>
#include <uapi/ipc/info.h>
#include <sof/mailbox.h>
#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/sof.h>
#include <sof/agent.h>
#include <sof/clk.h>
#include <sof/ipc.h>
#include <sof/io.h>
#include <sof/trace.h>
#include <sof/audio/component.h>
#include <sof/drivers/timer.h>
#include <sof/cpu.h>
#include <sof/notifier.h>
#include <sof/spi.h>
#include <config.h>
#include <sof/string.h>
#include <version.h>
#include <cavs/version.h>

static const struct sof_ipc_fw_ready ready
	__attribute__((section(".fw_ready"))) = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
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
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

#if defined(CONFIG_MEM_WND)
#define SRAM_WINDOW_HOST_OFFSET(x) (0x80000 + x * 0x20000)

#define NUM_WINDOWS 7

static const struct sof_ipc_window sram_window = {
	.ext_hdr	= {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_window) +
			sizeof(struct sof_ipc_window_elem) * NUM_WINDOWS,
		.type	= SOF_IPC_EXT_WINDOW,
	},
	.num_windows	= NUM_WINDOWS,
	.window	= {
		{
			.type	= SOF_IPC_REGION_REGS,
			.id	= 0,	/* map to host window 0 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_SW_REG_SIZE,
			.offset	= 0,
		},
		{
			.type	= SOF_IPC_REGION_UPBOX,
			.id	= 0,	/* map to host window 0 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_DSPBOX_SIZE,
			.offset	= MAILBOX_SW_REG_SIZE,
		},
		{
			.type	= SOF_IPC_REGION_DOWNBOX,
			.id	= 1,	/* map to host window 1 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_HOSTBOX_SIZE,
			.offset	= 0,
		},
		{
			.type	= SOF_IPC_REGION_DEBUG,
			.id	= 2,	/* map to host window 2 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_EXCEPTION_SIZE + MAILBOX_DEBUG_SIZE,
			.offset	= 0,
		},
		{
			.type	= SOF_IPC_REGION_EXCEPTION,
			.id	= 2,	/* map to host window 2 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_EXCEPTION_SIZE,
			.offset	= MAILBOX_EXCEPTION_OFFSET,
		},
		{
			.type	= SOF_IPC_REGION_STREAM,
			.id	= 2,	/* map to host window 2 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_STREAM_SIZE,
			.offset	= MAILBOX_STREAM_OFFSET,
		},
		{
			.type	= SOF_IPC_REGION_TRACE,
			.id	= 3,	/* map to host window 3 */
			.flags	= 0, // TODO: set later
			.size	= MAILBOX_TRACE_SIZE,
			.offset	= 0,
		},
	},
};
#endif

struct timesource_data platform_generic_queue[] = {
{
	.timer	 = {
		.id = TIMER3, /* external timer */
		.irq = IRQ_EXT_TSTAMP0_LVL2(0),
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
},
{
	.timer	 = {
		.id = TIMER3, /* external timer */
		.irq = IRQ_EXT_TSTAMP0_LVL2(1),
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
},
#if CAVS_VERSION >= CAVS_VERSION_1_8
{
	.timer	 = {
		.id = TIMER3, /* external timer */
		.irq = IRQ_EXT_TSTAMP0_LVL2(2),
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
},
{
	.timer	 = {
		.id = TIMER3, /* external timer */
		.irq = IRQ_EXT_TSTAMP0_LVL2(3),
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
},
#endif
};

#if defined(CONFIG_IOMUX)
#include <sof/iomux.h>
#endif

#if defined(CONFIG_DW_GPIO)
#include <sof/gpio.h>

const struct gpio_pin_config gpio_data[] = {
	{	/* GPIO0 */
		.mux_id = 1,
		.mux_config = {.bit = 0, .mask = 3, .fn = 1},
	}, {	/* GPIO1 */
		.mux_id = 1,
		.mux_config = {.bit = 2, .mask = 3, .fn = 1},
	}, {	/* GPIO2 */
		.mux_id = 1,
		.mux_config = {.bit = 4, .mask = 3, .fn = 1},
	}, {	/* GPIO3 */
		.mux_id = 1,
		.mux_config = {.bit = 6, .mask = 3, .fn = 1},
	}, {	/* GPIO4 */
		.mux_id = 1,
		.mux_config = {.bit = 8, .mask = 3, .fn = 1},
	}, {	/* GPIO5 */
		.mux_id = 1,
		.mux_config = {.bit = 10, .mask = 3, .fn = 1},
	}, {	/* GPIO6 */
		.mux_id = 1,
		.mux_config = {.bit = 12, .mask = 3, .fn = 1},
	}, {	/* GPIO7 */
		.mux_id = 1,
		.mux_config = {.bit = 14, .mask = 3, .fn = 1},
	}, {	/* GPIO8 */
		.mux_id = 1,
		.mux_config = {.bit = 16, .mask = 1, .fn = 1},
	}, {	/* GPIO9 */
		.mux_id = 0,
		.mux_config = {.bit = 11, .mask = 1, .fn = 1},
	}, {	/* GPIO10 */
		.mux_id = 0,
		.mux_config = {.bit = 11, .mask = 1, .fn = 1},
	}, {	/* GPIO11 */
		.mux_id = 0,
		.mux_config = {.bit = 11, .mask = 1, .fn = 1},
	}, {	/* GPIO12 */
		.mux_id = 0,
		.mux_config = {.bit = 11, .mask = 1, .fn = 1},
	}, {	/* GPIO13 */
		.mux_id = 0,
		.mux_config = {.bit = 0, .mask = 1, .fn = 1},
	}, {	/* GPIO14 */
		.mux_id = 0,
		.mux_config = {.bit = 1, .mask = 1, .fn = 1},
	}, {	/* GPIO15 */
		.mux_id = 0,
		.mux_config = {.bit = 9, .mask = 1, .fn = 1},
	}, {	/* GPIO16 */
		.mux_id = 0,
		.mux_config = {.bit = 9, .mask = 1, .fn = 1},
	}, {	/* GPIO17 */
		.mux_id = 0,
		.mux_config = {.bit = 9, .mask = 1, .fn = 1},
	}, {	/* GPIO18 */
		.mux_id = 0,
		.mux_config = {.bit = 9, .mask = 1, .fn = 1},
	}, {	/* GPIO19 */
		.mux_id = 0,
		.mux_config = {.bit = 10, .mask = 1, .fn = 1},
	}, {	/* GPIO20 */
		.mux_id = 0,
		.mux_config = {.bit = 10, .mask = 1, .fn = 1},
	}, {	/* GPIO21 */
		.mux_id = 0,
		.mux_config = {.bit = 10, .mask = 1, .fn = 1},
	}, {	/* GPIO22 */
		.mux_id = 0,
		.mux_config = {.bit = 10, .mask = 1, .fn = 1},
	}, {	/* GPIO23 */
		.mux_id = 0,
		.mux_config = {.bit = 16, .mask = 1, .fn = 1},
	}, {	/* GPIO24 */
		.mux_id = 0,
		.mux_config = {.bit = 16, .mask = 1, .fn = 1},
	}, {	/* GPIO25 */
		.mux_id = 0,
		.mux_config = {.bit = 26, .mask = 1, .fn = 1},
	},
};

const int n_gpios = ARRAY_SIZE(gpio_data);

struct iomux iomux_data[] = {
	{.base = EXT_CTRL_BASE + 0x30,},
	{.base = EXT_CTRL_BASE + 0x34,},
	{.base = EXT_CTRL_BASE + 0x38,},
};

const int n_iomux = ARRAY_SIZE(iomux_data);

static struct spi_platform_data spi = {
	.base		= DW_SPI_SLAVE_BASE,
	.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0, 0),
	.type		= SOF_SPI_INTEL_SLAVE,
	.fifo[SPI_DIR_RX] = {
		.handshake	= DMA_HANDSHAKE_SSI_RX,
	},
	.fifo[SPI_DIR_TX] = {
		.handshake	= DMA_HANDSHAKE_SSI_TX,
	}
};
#endif

struct timer *platform_timer =
	&platform_generic_queue[PLATFORM_MASTER_CORE_ID].timer;

#if defined(CONFIG_DW_SPI)
int platform_boot_complete(uint32_t boot_message)
{
	return spi_push(spi_get(SOF_SPI_INTEL_SLAVE), &ready, sizeof(ready));
}
#else
int platform_boot_complete(uint32_t boot_message)
{
	mailbox_dspbox_write(0, &ready, sizeof(ready));
#if defined(CONFIG_MEM_WND)
	mailbox_dspbox_write(sizeof(ready), &sram_window,
		sram_window.ext_hdr.hdr.size);
#endif // defined(CONFIG_MEM_WND)

	/* tell host we are ready */
#if CAVS_VERSION == CAVS_VERSION_1_5
	ipc_write(IPC_DIPCIE, SRAM_WINDOW_HOST_OFFSET(0) >> 12);
	ipc_write(IPC_DIPCI, 0x80000000 | SOF_IPC_FW_READY);
#else
	ipc_write(IPC_DIPCIDD, SRAM_WINDOW_HOST_OFFSET(0) >> 12);
	ipc_write(IPC_DIPCIDR, 0x80000000 | SOF_IPC_FW_READY);
#endif
	return 0;
}
#endif

#if defined(CONFIG_MEM_WND)
static void platform_memory_windows_init(void)
{
	/* window0, for fw status & outbox/uplink mbox */
	io_reg_write(DMWLO(0), HP_SRAM_WIN0_SIZE | 0x7);
	io_reg_write(DMWBA(0), HP_SRAM_WIN0_BASE
		| DMWBA_READONLY | DMWBA_ENABLE);
	bzero((void *)(HP_SRAM_WIN0_BASE + SRAM_REG_FW_END),
	      HP_SRAM_WIN0_SIZE - SRAM_REG_FW_END);
	dcache_writeback_region((void *)(HP_SRAM_WIN0_BASE + SRAM_REG_FW_END),
				HP_SRAM_WIN0_SIZE - SRAM_REG_FW_END);

	/* window1, for inbox/downlink mbox */
	io_reg_write(DMWLO(1), HP_SRAM_WIN1_SIZE | 0x7);
	io_reg_write(DMWBA(1), HP_SRAM_WIN1_BASE
		| DMWBA_ENABLE);
	bzero((void *)HP_SRAM_WIN1_BASE, HP_SRAM_WIN1_SIZE);
	dcache_writeback_region((void *)HP_SRAM_WIN1_BASE, HP_SRAM_WIN1_SIZE);

	/* window2, for debug */
	io_reg_write(DMWLO(2), HP_SRAM_WIN2_SIZE | 0x7);
	io_reg_write(DMWBA(2), HP_SRAM_WIN2_BASE
		| DMWBA_ENABLE);
	bzero((void *)HP_SRAM_WIN2_BASE, HP_SRAM_WIN2_SIZE);
	dcache_writeback_region((void *)HP_SRAM_WIN2_BASE, HP_SRAM_WIN2_SIZE);

	/* window3, for trace
	 * zeroed by trace initialization
	 */
	io_reg_write(DMWLO(3), HP_SRAM_WIN3_SIZE | 0x7);
	io_reg_write(DMWBA(3), HP_SRAM_WIN3_BASE
		| DMWBA_READONLY | DMWBA_ENABLE);
}
#endif

#if CAVS_VERSION >= CAVS_VERSION_1_8
/* init HW  */
static void platform_init_hw(void)
{
	io_reg_write(DSP_INIT_GENO,
		GENO_MDIVOSEL | GENO_DIOPTOSEL);

	io_reg_write(DSP_INIT_IOPO,
		IOPO_DMIC_FLAG | IOPO_I2S_FLAG);

	io_reg_write(DSP_INIT_ALHO,
		ALHO_ASO_FLAG | ALHO_CSO_FLAG | ALHO_CFO_FLAG);

	io_reg_write(DSP_INIT_LPGPDMA(0),
		LPGPDMA_CHOSEL_FLAG | LPGPDMA_CTLOSEL_FLAG);
	io_reg_write(DSP_INIT_LPGPDMA(1),
		LPGPDMA_CHOSEL_FLAG | LPGPDMA_CTLOSEL_FLAG);
}
#endif

int platform_init(struct sof *sof)
{
#if defined(CONFIG_DW_SPI)
	struct spi *spi_dev;
#endif
	int ret;

#if defined(CONFIG_CANNONLAKE) || defined(CONFIG_ICELAKE) \
	|| defined(CONFIG_SUECREEK)
	trace_point(TRACE_BOOT_PLATFORM_ENTRY);
	platform_init_hw();
#endif

	platform_interrupt_init();

	trace_point(TRACE_BOOT_PLATFORM_MBOX);

#if defined(CONFIG_MEM_WND)
	platform_memory_windows_init();
#endif
	trace_point(TRACE_BOOT_PLATFORM_SHIM);

	/* init work queues and clocks */
	trace_point(TRACE_BOOT_PLATFORM_TIMER);
	platform_timer_start(platform_timer);

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	clock_init();

	trace_point(TRACE_BOOT_SYS_SCHED);
	scheduler_init();

	/* init the system agent */
	sa_init(sof);

	/* Set CPU to max frequency for booting (single shim_write below) */
	trace_point(TRACE_BOOT_SYS_CPU_FREQ);
#if defined(CONFIG_APOLLOLAKE)
	/* initialize PM for boot */

	/* TODO: there are two clk freqs CRO & CRO/4
	 * Running on CRO all the time atm
	 */

	shim_write(SHIM_CLKCTL,
		   SHIM_CLKCTL_HDCS_PLL | /* HP domain clocked by PLL */
		   SHIM_CLKCTL_LDCS_PLL | /* LP domain clocked by PLL */
		   SHIM_CLKCTL_DPCS_DIV1(0) | /* Core 0 clk not divided */
		   SHIM_CLKCTL_DPCS_DIV1(1) | /* Core 1 clk not divided */
		   SHIM_CLKCTL_HPMPCS_DIV2 | /* HP mem clock div by 2 */
		   SHIM_CLKCTL_LPMPCS_DIV4 | /* LP mem clock div by 4 */
		   SHIM_CLKCTL_TCPAPLLS_DIS |
		   SHIM_CLKCTL_TCPLCG_DIS(0) | SHIM_CLKCTL_TCPLCG_DIS(1));

	shim_write(SHIM_LPSCTL, shim_read(SHIM_LPSCTL));

#elif defined(CONFIG_CANNONLAKE)

	/* initialize PM for boot */
	shim_write(SHIM_CLKCTL,
		   SHIM_CLKCTL_RHROSCC | /* Request High Performance RING Osc */
		   SHIM_CLTCTL_RLROSCC | /* Request Low Performance RING Osc */
		   SHIM_CLKCTL_OCS_HP_RING | /* Select HP RING Oscillator Clk
					      * for memory
					      */
		   SHIM_CLKCTL_HMCS_DIV2 | /* HP mem clock div by 2 */
		   SHIM_CLKCTL_LMCS_DIV4 | /* LP mem clock div by 4 */
		   SHIM_CLKCTL_TCPLCG_DIS(0) | /* Allow Local Clk Gating */
		   SHIM_CLKCTL_TCPLCG_DIS(1) |
		   SHIM_CLKCTL_TCPLCG_DIS(2) |
		   SHIM_CLKCTL_TCPLCG_DIS(3));

	/* prevent LP GPDMA 0&1 clock gating */
	shim_write(SHIM_GPDMA_CLKCTL(0), SHIM_CLKCTL_LPGPDMAFDCGB);
	shim_write(SHIM_GPDMA_CLKCTL(1), SHIM_CLKCTL_LPGPDMAFDCGB);

	/* prevent DSP Common power gating */
	shim_write16(SHIM_PWRCTL, SHIM_PWRCTL_TCPDSPPG(0) |
		     SHIM_PWRCTL_TCPDSPPG(1) | SHIM_PWRCTL_TCPDSPPG(2) |
		     SHIM_PWRCTL_TCPDSPPG(3));

#elif defined(CONFIG_ICELAKE) || defined(CONFIG_SUECREEK)
	/* TODO: need to merge as for APL */
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);

	/* prevent Core0 clock gating. */
	shim_write(SHIM_CLKCTL, shim_read(SHIM_CLKCTL) |
		SHIM_CLKCTL_TCPLCG(0));

	/* prevent LP GPDMA 0&1 clock gating */
	io_reg_write(GPDMA_CLKCTL(0), GPDMA_FDCGB);
	io_reg_write(GPDMA_CLKCTL(1), GPDMA_FDCGB);

	/* prevent DSP Common power gating */
	shim_write16(SHIM_PWRCTL, SHIM_PWRCTL_TCPDSPPG(0) |
		     SHIM_PWRCTL_TCPDSPPG(1) | SHIM_PWRCTL_TCPDSPPG(2) |
		     SHIM_PWRCTL_TCPDSPPG(3));
#endif

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	ret = dmac_init();
	if (ret < 0)
		return ret;

	/* initialize the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init(sof);

	/* init DAIs */
	ret = dai_init();
	if (ret < 0)
		return ret;

	/* initialize IDC mechanism */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	ret = idc_init();
	if (ret < 0)
		return ret;

#if defined(CONFIG_DW_SPI)
	/* initialize the SPI slave */
	spi_init();
	ret = spi_install(&spi, 1);
	if (ret < 0)
		return ret;

	spi_dev = spi_get(SOF_SPI_INTEL_SLAVE);
	if (!spi_dev)
		return -ENODEV;

	/* initialize the SPI-SLave module */
	ret = spi_probe(spi_dev);
	if (ret < 0)
		return ret;
#elif CONFIG_TRACE
	/* Initialize DMA for Trace*/
	dma_trace_init_complete(sof->dmat);
#endif

	/* show heap status */
	heap_trace_all(1);

	return 0;
}
