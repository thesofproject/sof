/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <ace/lib/clk.h>

#define CLK_MAX_CPU_HZ		400000000

#define CPU_WOVCRO_FREQ_IDX	0

#define CPU_LPRO_FREQ_IDX	1

#define CPU_HPRO_FREQ_IDX	2

#define CPU_LOWEST_FREQ_IDX	CPU_WOVCRO_FREQ_IDX

#define CPU_DEFAULT_IDX		CPU_LOWEST_FREQ_IDX

#define SSP_DEFAULT_IDX		1

#define NUM_CPU_FREQ		3

#define NUM_SSP_FREQ		3

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
