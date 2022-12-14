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
/* check vcore voltage before select higher frequency than 300M */
#define CLK_MAX_CPU_HZ				300000000
#define NUM_CLOCKS				1
#define NUM_CPU_FREQ				3

/* MTK_ADSP_CLK_BUS_UPDATE */
#define MTK_ADSP_CLK_BUS_UPDATE_BIT		BIT(31)

/* MTK_ADSP_BUS_SRC */
#define MTK_ADSP_CLK_BUS_SRC_EMI		0
#define MTK_ADSP_CLK_BUS_SRC_LOCAL		1

/* MTK_CLK_CFG_11 */
#define MTK_CLK_CFG_ADSP_UPDATE			BIT(16)
#define MTK_CLK_ADSP_OFFSET			24
#define MTK_CLK_ADSP_MASK			0x7
#define MTK_CLK_ADSP_26M			0
#define MTK_CLK_ADSP_ULPOSC_D_10		1 /* 25M, Not used */
#define MTK_CLK_ADSP_DSPPLL			2
#define MTK_CLK_ADSP_DSPPLL_2			3
#define MTK_CLK_ADSP_DSPPLL_4			4
#define MTK_CLK_ADSP_DSPPLL_8			5

/* MTK_CLK_CFG_15 */
#define MTK_CLK_CFG_ADSP_BUS_UPDATE		BIT(31)
#define MTK_CLK_ADSP_BUS_OFFSET			17
#define MTK_CLK_ADSP_BUS_MASK			0x7
#define MTK_CLK_ADSP_BUS_26M			0
#define MTK_CLK_ADSP_BUS_ULPOSC_D_2		1
#define MTK_CLK_ADSP_BUS_MAINPPLL_D_5		2
#define MTK_CLK_ADSP_BUS_MAINPPLL_D_2_D_2	3
#define MTK_CLK_ADSP_BUS_MAINPPLL_D_3		4
#define MTK_CLK_ADSP_BUS_RESERVED		5
#define MTK_CLK_ADSP_BUS_UNIVPLL_D_3		6

#define MTK_PLL_BASE_EN				BIT(0)
#define MTK_PLL_PWR_ON				BIT(0)
#define MTK_PLL_ISO_EN				BIT(1)

#define MTK_PLL_DIV_RATIO_300M			0x831713B2
#define MTK_PLL_DIV_RATIO_400M			0x831EC4ED

/* List resource from low to high request */
/* 0 is the lowest request */
enum ADSP_HW_DSP_CLK {
	ADSP_CLK_26M = 0,
	ADSP_CLK_PLL_300M,
	ADSP_CLK_PLL_400M,
};

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
