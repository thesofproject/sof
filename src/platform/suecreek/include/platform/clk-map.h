/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __INCLUDE_CLOCK_MAP__
#define __INCLUDE_CLOCK_MAP__

#include <sof/clk.h>

static const struct freq_table cpu_freq[] = {
	{120000000, 120000, 0x0},
	{400000000, 400000, 0x4}, /* default */
};

/* IMPORTANT: array should be filled in increasing order
 * (regarding to .freq field)
 */
static const struct freq_table ssp_freq[] = {
	{ 38400000, 38400, CLOCK_SSP_XTAL_OSCILLATOR },
	{ 96000000, 96000, CLOCK_SSP_PLL_FIXED },
};

#endif
