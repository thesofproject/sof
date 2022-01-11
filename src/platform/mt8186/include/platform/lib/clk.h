// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek. All rights reserved.
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
//         Tinghan Shen <tinghan.shen@mediatek.com>

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
#define NUM_CPU_FREQ				5

/* MTK_ADSP_CLK_BUS_UPDATE */
#define MTK_ADSP_CLK_BUS_UPDATE_BIT		BIT(31)

/* MTK_ADSP_BUS_SRC */
#define MTK_ADSP_CLK_BUS_SRC_EMI		0
#define MTK_ADSP_CLK_BUS_SRC_LOCAL		1

/* MTK_CLK_CFG_UPDATE */
#define MTK_CLK_CFG_ADSP_UPDATE			BIT(16)

/* MTK_CLK_CFG_11 */
#define MTK_CLK_ADSP_OFFSET			24
#define MTK_CLK_ADSP_MASK			0x7
#define MTK_CLK_ADSP_26M			0
#define MTK_CLK_ADSP_ULPOSC_D_10		1 /* 25M, Not used */
#define MTK_CLK_ADSP_DSPPLL			2
#define MTK_CLK_ADSP_DSPPLL_2			3
#define MTK_CLK_ADSP_DSPPLL_4			4
#define MTK_CLK_ADSP_DSPPLL_8			5

/* List resource from low to high request */
/* 0 is the lowest request */
enum ADSP_HW_DSP_CLK {
	ADSP_CLK_26M = 0,
	ADSP_CLK_PLL_800M_D_8,
	ADSP_CLK_PLL_800M_D_4,
	ADSP_CLK_PLL_800M_D_2,
	ADSP_CLK_PLL_800M,
};

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
