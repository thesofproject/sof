// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <cavs/version.h>
#if (CONFIG_CAVS_LPS)
#include <cavs/lps_wait.h>
#endif
#include <cavs/mem_window.h>
#include <sof/common.h>
#include <sof/debug/debug.h>
#include <sof/drivers/dw-dma.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/dma-trace.h>
#include <sof/trace/trace.h>
#include <ipc/header.h>
#include <ipc/info.h>
#include <kernel/abi.h>
#include <config.h>
#include <version.h>
#include <errno.h>
#include <stdint.h>

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

#if CONFIG_MEM_WND
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

#if CONFIG_CANNONLAKE
#if CONFIG_CAVS_LPRO
#define CNL_DEFAULT_RO		SHIM_CLKCTL_RLROSCC
#define CNL_DEFAULT_RO_FOR_MEM	SHIM_CLKCTL_OCS_LP_RING
#else
#define CNL_DEFAULT_RO		(SHIM_CLKCTL_RLROSCC | SHIM_CLKCTL_RHROSCC)
#define CNL_DEFAULT_RO_FOR_MEM	SHIM_CLKCTL_OCS_HP_RING
#endif
#endif

#if CONFIG_DW_GPIO

#include <sof/drivers/gpio.h>

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

#if CONFIG_INTEL_IOMUX

#include <sof/drivers/iomux.h>

struct iomux iomux_data[] = {
	{.base = EXT_CTRL_BASE + 0x30,},
	{.base = EXT_CTRL_BASE + 0x34,},
	{.base = EXT_CTRL_BASE + 0x38,},
};

const int n_iomux = ARRAY_SIZE(iomux_data);

#endif

#endif

struct timer timer = {
	.id = TIMER3, /* external timer */
	.irq = IRQ_EXT_TSTAMP0_LVL2,
	.irq_name = irq_name_level2,
};

struct timer *platform_timer = &timer;

struct ll_schedule_domain *platform_timer_domain;
struct ll_schedule_domain *platform_dma_domain;

#if CONFIG_DW_SPI

#include <sof/drivers/spi.h>

static struct spi_platform_data spi = {
	.base		= DW_SPI_SLAVE_BASE,
	.type		= SOF_SPI_INTEL_SLAVE,
	.fifo[SPI_DIR_RX] = {
		.handshake	= DMA_HANDSHAKE_SSI_RX,
	},
	.fifo[SPI_DIR_TX] = {
		.handshake	= DMA_HANDSHAKE_SSI_TX,
	}
};

int platform_boot_complete(uint32_t boot_message)
{
	return spi_push(spi_get(SOF_SPI_INTEL_SLAVE), &ready, sizeof(ready));
}

#else

int platform_boot_complete(uint32_t boot_message)
{
	mailbox_dspbox_write(0, &ready, sizeof(ready));
#if CONFIG_MEM_WND
	mailbox_dspbox_write(sizeof(ready), &sram_window,
		sram_window.ext_hdr.hdr.size);
#endif

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

#if CAVS_VERSION >= CAVS_VERSION_1_8
/* init HW  */
static void platform_init_hw(void)
{
	io_reg_write(DSP_INIT_GENO,
		GENO_MDIVOSEL | GENO_DIOPTOSEL);

	io_reg_write(DSP_INIT_IOPO,
		IOPO_DMIC_FLAG | IOPO_I2S_FLAG);

	io_reg_write(DSP_INIT_ALHO,
		ALHO_ASO_FLAG | ALHO_CSO_FLAG);

	io_reg_write(DSP_INIT_LPGPDMA(0),
		LPGPDMA_CHOSEL_FLAG | LPGPDMA_CTLOSEL_FLAG);
	io_reg_write(DSP_INIT_LPGPDMA(1),
		LPGPDMA_CHOSEL_FLAG | LPGPDMA_CTLOSEL_FLAG);
}
#endif

int platform_init(struct sof *sof)
{
#if CONFIG_DW_SPI
	struct spi *spi_dev;
#endif
	int ret;

	/* pm runtime already initialized, request the DSP to stay in D0
	 * until we are allowed to do full power gating (by the IPC req).
	 */
	pm_runtime_disable(PM_RUNTIME_DSP, 0);

#if CONFIG_CANNONLAKE || CONFIG_ICELAKE || CONFIG_SUECREEK || CONFIG_TIGERLAKE
	trace_point(TRACE_BOOT_PLATFORM_ENTRY);
	platform_init_hw();
#endif

	trace_point(TRACE_BOOT_PLATFORM_IRQ);
	platform_interrupt_init();

#if CONFIG_MEM_WND
	trace_point(TRACE_BOOT_PLATFORM_MBOX);
	platform_memory_windows_init(MEM_WND_INIT_CLEAR);
#endif

	/* init timers, clocks and schedulers */
	trace_point(TRACE_BOOT_PLATFORM_TIMER);
	platform_timer_start(platform_timer);

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	platform_clock_init();
	clock_init();

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init_edf();

	/* init low latency timer domain and scheduler */
	platform_timer_domain =
		timer_domain_init(platform_timer, PLATFORM_DEFAULT_CLOCK,
				  CONFIG_SYSTICK_PERIOD);
	scheduler_init_ll(platform_timer_domain);

	/* init low latency single channel DW-DMA domain and scheduler */
	platform_dma_domain =
		dma_single_chan_domain_init
			(&dma[PLATFORM_DW_DMA_INDEX],
			 PLATFORM_NUM_DW_DMACS,
			 PLATFORM_DEFAULT_CLOCK);
	scheduler_init_ll(platform_dma_domain);

	/* init the system agent */
	trace_point(TRACE_BOOT_PLATFORM_AGENT);
	sa_init(sof, CONFIG_SYSTICK_PERIOD);

	/* Set CPU to max frequency for booting (single shim_write below) */
	trace_point(TRACE_BOOT_PLATFORM_CPU_FREQ);
#if CONFIG_APOLLOLAKE
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

#elif CONFIG_CANNONLAKE

	/* initialize PM for boot */

	/* request configured ring oscillator and wait for status ready */
	shim_write(SHIM_CLKCTL, shim_read(SHIM_CLKCTL) | CNL_DEFAULT_RO);
	while (!(shim_read(SHIM_CLKSTS) & CNL_DEFAULT_RO))
		idelay(16);

	shim_write(SHIM_CLKCTL,
		   CNL_DEFAULT_RO | /* Request configured RING Osc */
		   CNL_DEFAULT_RO_FOR_MEM | /* Select configured
					     * RING Oscillator Clk for memory
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
	pm_runtime_get(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID);

#elif CONFIG_ICELAKE || CONFIG_SUECREEK || CONFIG_TIGERLAKE
	/* TODO: need to merge as for APL */
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);

	/* prevent Core0 clock gating. */
	shim_write(SHIM_CLKCTL, shim_read(SHIM_CLKCTL) |
		SHIM_CLKCTL_TCPLCG(0));

	/* prevent LP GPDMA 0&1 clock gating */
	shim_write(SHIM_GPDMA_CLKCTL(0), SHIM_CLKCTL_LPGPDMAFDCGB);
	shim_write(SHIM_GPDMA_CLKCTL(1), SHIM_CLKCTL_LPGPDMAFDCGB);

	/* prevent DSP Common power gating */
	pm_runtime_get(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID);
#endif

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	ret = dmac_init();
	if (ret < 0)
		return ret;

	/* initialize the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init(sof);

	/* initialize IDC mechanism */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	ret = idc_init();
	if (ret < 0)
		return ret;

	/* init DAIs */
	trace_point(TRACE_BOOT_PLATFORM_DAI);
	ret = dai_init();
	if (ret < 0)
		return ret;

#if CONFIG_DW_SPI
	/* initialize the SPI slave */
	trace_point(TRACE_BOOT_PLATFORM_SPI);
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
	trace_point(TRACE_BOOT_PLATFORM_DMA_TRACE);
	dma_trace_init_complete(sof->dmat);
#endif

	/* show heap status */
	heap_trace_all(1);

	return 0;
}

void platform_wait_for_interrupt(int level)
{
#if (CONFIG_CAVS_LPS)
	if (pm_runtime_is_active(PM_RUNTIME_DSP, PLATFORM_MASTER_CORE_ID))
		arch_wait_for_interrupt(level);
	else
		lps_wait_for_interrupt(level);
#else
	arch_wait_for_interrupt(level);
#endif
}
