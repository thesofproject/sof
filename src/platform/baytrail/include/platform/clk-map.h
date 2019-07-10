/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __PLATFORM_CLK_MAP_H__
#define __PLATFORM_CLK_MAP_H__

#include <sof/clk.h>

#if defined CONFIG_BAYTRAIL
static const struct freq_table cpu_freq[] = {
	{25000000, 25000, 0x0},
	{25000000, 25000, 0x1},
	{50000000, 50000, 0x2},
	{50000000, 50000, 0x3}, /* default */
	{100000000, 100000, 0x4},
	{200000000, 200000, 0x5},
	{267000000, 267000, 0x6},
	{343000000, 343000, 0x7},
};
#elif defined CONFIG_CHERRYTRAIL
static const struct freq_table cpu_freq[] = {
	{19200000, 19200, 0x0},
	{19200000, 19200, 0x1},
	{38400000, 38400, 0x2},
	{50000000, 50000, 0x3}, /* default */
	{100000000, 100000, 0x4},
	{200000000, 200000, 0x5},
	{267000000, 267000, 0x6},
	{343000000, 343000, 0x7},
};
#endif

static const struct freq_table ssp_freq[] = {
	{19200000, 19200, PMC_SET_SSP_19M2}, /* default */
	{25000000, 25000, PMC_SET_SSP_25M},
};

#endif /* __PLATFORM_CLK_MAP_H__ */
