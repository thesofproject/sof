/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2022 MediaTek. All rights reserved.
 *
 * Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
 *         Tinghan Shen <tinghan.shen@mediatek.com>
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <stdint.h>

struct sof;

#define CPU_DEFAULT_IDX				0

#define CLK_CPU(x)				(x)
#define CLK_DEFAULT_CPU_HZ			26000000
#define CLK_MAX_CPU_HZ				800000000
#define NUM_CLOCKS				1
#define NUM_CPU_FREQ				3

/* MTK_ADSPPLL_CON1 */
#define MTK_PLL_DIV_RATIO_800M			0x810F6276
#define MTK_PLL_DIV_RATIO_400M			0x831EC4ED

/* MTK_ADSPPLL_CON3 */
#define MTK_PLL_EN				BIT(9)
#define MTK_PLL_PWR_ON				BIT(0)
#define MTK_PLL_ISO_EN				BIT(1)

/* MTK_CLK_CFG_UPDATE2 */
#define MTK_CLK_UPDATE_ADSK_CLK			BIT(4)
#define MTK_CLK_UPDATE_AUDIO_LOCAL_BUS_CLK	BIT(5)

/* MTK_CLK_CFG_17[3:0] */
#define MTK_CLK_ADSP_OFFSET			0
#define MTK_CLK_ADSP_MASK			0xF
#define MTK_CLK_ADSP_26M			0
#define MTK_CLK_ADSP_ADSPPLL			8 /* 800M */
#define MTK_CLK_ADSP_ADSPPLL_D_2		9 /* 400M */

/* MTK_CLK_CFG_17[11:8] */
#define MTK_CLK_AUDIO_LOCAL_BUS_OFFSET		8
#define MTK_CLK_AUDIO_LOCAL_BUS_MASK		0xF
#define MTK_CLK_AUDIO_LOCAL_BUS_26M		0
#define MTK_CLK_AUDIO_LOCAL_BUS_MAINPLL_D_7	6 /* 312M */
#define MTK_CLK_AUDIO_LOCAL_BUS_MAINPLL_D_4	7 /* 546M */

/* List resource from low to high request */
/* 0 is the lowest request */
enum ADSP_HW_DSP_CLK {
	ADSP_CLK_26M = 0,
	ADSP_CLK_PLL_400M,
	ADSP_CLK_PLL_800M,
};

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
