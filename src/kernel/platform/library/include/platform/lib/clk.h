/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#define CLK_CPU(x)	x
#define CLK_SSP		1

#define CLK_MAX_CPU_HZ		343000000

#define CPU_WOVCRO_FREQ_IDX	0

#define CPU_LOWEST_FREQ_IDX	CPU_WOVCRO_FREQ_IDX

#define NUM_SSP_FREQ		3

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
