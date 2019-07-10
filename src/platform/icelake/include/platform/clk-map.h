/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __PLATFORM_CLK_MAP_H__
#define __PLATFORM_CLK_MAP_H__

#include <sof/clk.h>

#define CLK_MAX_CPU_HZ		400000000

static const struct freq_table cpu_freq[] = {
	{120000000, 120000, 0x0},
	{CLK_MAX_CPU_HZ, 400000, 0x4}, /* default */
};

#define CPU_DEFAULT_IDX		1

/* IMPORTANT: array should be filled in increasing order
 * (regarding to .freq field)
 */
static const struct freq_table ssp_freq[] = {
	{ 24576000, 24576, CLOCK_SSP_AUDIO_CARDINAL },
	{ 38400000, 38400, CLOCK_SSP_XTAL_OSCILLATOR }, /* default */
	{ 96000000, 96000, CLOCK_SSP_PLL_FIXED },
};

#define SSP_DEFAULT_IDX		1

#endif /* __PLATFORM_CLK_MAP_H__ */
