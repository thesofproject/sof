/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
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
#define NUM_CPU_FREQ				2

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
