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

/* 19d4e680-4479-48cc-af86-9f63d8b0098b */
SOF_DEFINE_UUID("clkdrv_mt8188", clkdrv_mt8188_uuid, 0x19d4e680, 0x4479, 0x48cc,
		 0xaf, 0x86, 0x9f, 0x63, 0xd8, 0xb0, 0x09, 0x8b);

DECLARE_TR_CTX(clkdrv_tr, SOF_UUID(clkdrv_mt8188_uuid), LOG_LEVEL_INFO);

/* default voltage is 0.75V */
const struct freq_table platform_cpu_freq[] = {
	{  26000000, 26000},
	{ 400000000, 26000},
	{ 800000000, 26000},
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

static void clk_dsppll_enable(uint32_t value)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_enable %d\n", value);

	switch (value) {
	case ADSP_CLK_PLL_400M:
		io_reg_write(MTK_ADSPPLL_CON1, MTK_PLL_DIV_RATIO_400M);
		break;
	case ADSP_CLK_PLL_800M:
		io_reg_write(MTK_ADSPPLL_CON1, MTK_PLL_DIV_RATIO_800M);
		break;
	default:
		tr_err(&clkdrv_tr, "invalid dsppll: %d\n", value);
		return;
	}

	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_PWR_ON, MTK_PLL_PWR_ON);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_ISO_EN, 0);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON0, MTK_PLL_EN, MTK_PLL_EN);
	wait_delay_us(20);
}

static void clk_dsppll_disable(void)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_disable\n");

	io_reg_update_bits(MTK_ADSPPLL_CON0, MTK_PLL_EN, 0);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_ISO_EN, MTK_PLL_ISO_EN);
	wait_delay_us(1);
	io_reg_update_bits(MTK_ADSPPLL_CON3, MTK_PLL_PWR_ON, 0);
}

static void set_mux_adsp_sel(uint32_t value)
{
	io_reg_write(MTK_CLK_CFG_17_CLR, MTK_CLK_ADSP_MASK << MTK_CLK_ADSP_OFFSET);
	io_reg_write(MTK_CLK_CFG_17_SET, value << MTK_CLK_ADSP_OFFSET);
	io_reg_write(MTK_CLK_CFG_UPDATE2, MTK_CLK_UPDATE_ADSK_CLK);

	tr_dbg(&clkdrv_tr, "adsp_clk_mux=%x, CLK_CFG_17=0x%08x\n",
	       value, io_reg_read(MTK_CLK_CFG_17));
}

static void set_mux_adsp_bus_sel(uint32_t value)
{
	io_reg_write(MTK_CLK_CFG_17_CLR,
		     MTK_CLK_AUDIO_LOCAL_BUS_MASK << MTK_CLK_AUDIO_LOCAL_BUS_OFFSET);
	io_reg_write(MTK_CLK_CFG_17_SET, value << MTK_CLK_AUDIO_LOCAL_BUS_OFFSET);
	io_reg_write(MTK_CLK_CFG_UPDATE2, MTK_CLK_UPDATE_AUDIO_LOCAL_BUS_CLK);

	tr_dbg(&clkdrv_tr, "audio_local_bus_mux=%x, CLK_CFG_17=0x%08x\n",
	       value, io_reg_read(MTK_CLK_CFG_17));
}

static int clock_platform_set_dsp_freq(int clock, int freq_idx)
{
	int freq = platform_cpu_freq[freq_idx].freq;

	tr_info(&clkdrv_tr, "clock_platform_set_cpu_freq %d\n", freq);

	switch (freq_idx) {
	case ADSP_CLK_26M:
		set_mux_adsp_sel(MTK_CLK_ADSP_26M);
		set_mux_adsp_bus_sel(MTK_CLK_AUDIO_LOCAL_BUS_26M);
		clk_dsppll_disable();
		break;
	case ADSP_CLK_PLL_400M:
		clock_platform_set_dsp_freq(clock, ADSP_CLK_26M);
		clk_dsppll_enable(ADSP_CLK_PLL_400M);
		set_mux_adsp_sel(MTK_CLK_ADSP_ADSPPLL);
		set_mux_adsp_bus_sel(MTK_CLK_AUDIO_LOCAL_BUS_MAINPLL_D_7);
		break;
	case ADSP_CLK_PLL_800M:
		clock_platform_set_dsp_freq(clock, ADSP_CLK_26M);
		clk_dsppll_enable(ADSP_CLK_PLL_800M);
		set_mux_adsp_sel(MTK_CLK_ADSP_ADSPPLL);
		set_mux_adsp_bus_sel(MTK_CLK_AUDIO_LOCAL_BUS_MAINPLL_D_4);
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
