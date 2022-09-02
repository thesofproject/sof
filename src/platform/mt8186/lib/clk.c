// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek. All rights reserved.
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
//         Tinghan Shen <tinghan.shen@mediatek.com>

#include <platform/drivers/mt_reg_base.h>
#include <sof/common.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>

/* 53863428-9a72-44df-af0f-fe45ea2348ba */
DECLARE_SOF_UUID("clkdrv", clkdrv_uuid, 0x53863428, 0x9a72, 0x44df,
		 0xaf, 0x0f, 0xfe, 0x45, 0xea, 0x23, 0x48, 0xba);

DECLARE_TR_CTX(clkdrv_tr, SOF_UUID(clkdrv_uuid), LOG_LEVEL_INFO);

/* default voltage is 0.8V */
const struct freq_table platform_cpu_freq[] = {
	{  26000000, 26000},
	{ 100000000, 26000},
	{ 200000000, 26000},
	{ 400000000, 26000},
	{ 800000000, 26000},
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

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

	tr_dbg(&clkdrv_tr, "adsp_bus_mux=%x, MTK_ADSP_BUS_SRC=0x%08x\n",
	       value, io_reg_read(MTK_ADSP_BUS_SRC));
}

static int clock_platform_set_dsp_freq(int clock, int freq_idx)
{
	switch (freq_idx) {
	case ADSP_CLK_26M:
		set_mux_adsp_sel(MTK_CLK_ADSP_26M);
		break;
	case ADSP_CLK_PLL_800M_D_8:
		set_mux_adsp_sel(MTK_CLK_ADSP_DSPPLL_8);
		break;
	case ADSP_CLK_PLL_800M_D_4:
		set_mux_adsp_sel(MTK_CLK_ADSP_DSPPLL_4);
		break;
	case ADSP_CLK_PLL_800M_D_2:
		set_mux_adsp_sel(MTK_CLK_ADSP_DSPPLL_2);
		break;
	case ADSP_CLK_PLL_800M:
		set_mux_adsp_sel(MTK_CLK_ADSP_DSPPLL);
		break;
	default:
		set_mux_adsp_sel(MTK_CLK_ADSP_26M);
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

		k_spinlock_init(&sof->clocks[i].lock);
	}

	/* DSP bus clock */
	set_mux_adsp_bus_src_sel(MTK_ADSP_CLK_BUS_SRC_EMI);
}
