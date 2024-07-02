// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 MediaTek. All rights reserved.
 *
 * Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
 *         Tinghan Shen <tinghan.shen@mediatek.com>
 */

#include <platform/drivers/mt_reg_base.h>
#include <rtos/clk.h>
#include <rtos/wait.h>
#include <sof/common.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <rtos/sof.h>
#include <sof/trace/trace.h>

SOF_DEFINE_REG_UUID(clkdrv_mt8186);

DECLARE_TR_CTX(clkdrv_tr, SOF_UUID(clkdrv_mt8186_uuid), LOG_LEVEL_INFO);

/* default voltage is 0.8V */
const struct freq_table platform_cpu_freq[] = {
	{  26000000, 26000},
	{ 300000000, 26000},
	{ 400000000, 26000},
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

static void clk_dsppll_enable(uint32_t value)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_enable: %d\n", value);

	switch (value) {
	case ADSP_CLK_PLL_300M:
		io_reg_write(MTK_ADSPPLL_CON1, MTK_PLL_DIV_RATIO_300M);
		break;
	case ADSP_CLK_PLL_400M:
		io_reg_write(MTK_ADSPPLL_CON1, MTK_PLL_DIV_RATIO_400M);
		break;
	default:
		tr_err(&clkdrv_tr, "invalid dsppll: %d\n", value);
		return;
	}

	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_PWR_ON, MTK_PLL_PWR_ON);
	wait_delay_us(20);
	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_ISO_EN, 0);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON0, MTK_PLL_BASE_EN, MTK_PLL_BASE_EN);
	wait_delay_us(20);
}

static void clk_dsppll_disable(void)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_disable\n");

	io_reg_update_bits(MTK_ADSPPLL_CON0, MTK_PLL_BASE_EN, 0);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_ISO_EN, MTK_PLL_ISO_EN);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_PWR_ON, 0);
}

static void set_mux_adsp_sel(uint32_t value)
{
	io_reg_write(MTK_CLK_CFG_11_CLR, MTK_CLK_ADSP_MASK << MTK_CLK_ADSP_OFFSET);
	io_reg_write(MTK_CLK_CFG_11_SET, value << MTK_CLK_ADSP_OFFSET);
	io_reg_write(MTK_CLK_CFG_UPDATE, MTK_CLK_CFG_ADSP_UPDATE);

	tr_dbg(&clkdrv_tr, "adsp_clk_mux=%x, CLK_CFG_11=0x%08x\n",
	       value, io_reg_read(MTK_CLK_CFG_11));
}

static void set_mux_adsp_bus_src_sel(uint32_t value)
{
	io_reg_write(MTK_ADSP_BUS_SRC, value);
	io_reg_write(MTK_ADSP_CLK_BUS_UPDATE, MTK_ADSP_CLK_BUS_UPDATE_BIT);
	wait_delay_us(1);

	tr_dbg(&clkdrv_tr, "adsp_bus_mux=%x, MTK_ADSP_BUS_SRC=0x%08x\n",
	       value, io_reg_read(MTK_ADSP_BUS_SRC));
}

static void set_mux_adsp_bus_sel(uint32_t value)
{
	io_reg_write(MTK_CLK_CFG_15_CLR, MTK_CLK_ADSP_BUS_MASK << MTK_CLK_ADSP_BUS_OFFSET);
	io_reg_write(MTK_CLK_CFG_15_SET, value << MTK_CLK_ADSP_BUS_OFFSET);
	io_reg_write(MTK_CLK_CFG_UPDATE, MTK_CLK_CFG_ADSP_BUS_UPDATE);

	tr_dbg(&clkdrv_tr, "adsp_bus_clk_mux=%x, CLK_CFG_15=0x%08x\n",
	       value, io_reg_read(MTK_CLK_CFG_15));
}

static int clock_platform_set_dsp_freq(int clock, int freq_idx)
{
	switch (freq_idx) {
	case ADSP_CLK_26M:
		set_mux_adsp_bus_sel(MTK_CLK_ADSP_BUS_26M);
		set_mux_adsp_bus_src_sel(MTK_ADSP_CLK_BUS_SRC_LOCAL);
		set_mux_adsp_sel(MTK_CLK_ADSP_26M);
		clk_dsppll_disable();
		break;
	case ADSP_CLK_PLL_300M:
		clock_platform_set_dsp_freq(clock, ADSP_CLK_26M);
		clk_dsppll_enable(ADSP_CLK_PLL_300M);
		set_mux_adsp_sel(MTK_CLK_ADSP_DSPPLL);
		set_mux_adsp_bus_src_sel(MTK_ADSP_CLK_BUS_SRC_EMI);
		set_mux_adsp_bus_sel(MTK_CLK_ADSP_BUS_26M);
		break;
	case ADSP_CLK_PLL_400M:
		clock_platform_set_dsp_freq(clock, ADSP_CLK_26M);
		clk_dsppll_enable(ADSP_CLK_PLL_400M);
		set_mux_adsp_sel(MTK_CLK_ADSP_DSPPLL);
		set_mux_adsp_bus_src_sel(MTK_ADSP_CLK_BUS_SRC_EMI);
		set_mux_adsp_bus_sel(MTK_CLK_ADSP_BUS_26M);
		break;
	default:
		clock_platform_set_dsp_freq(clock, ADSP_CLK_26M);
		tr_err(&clkdrv_tr, "unknown freq index %x\n", freq_idx);
		break;
	}

	return 0;
}

void platform_clock_init(struct sof *sof)
{
	int i;

	sof->clocks = platform_shared_get(platform_clocks_info, sizeof(platform_clocks_info));

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info){
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = clock_platform_set_dsp_freq,
		};
	}

	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);
}
