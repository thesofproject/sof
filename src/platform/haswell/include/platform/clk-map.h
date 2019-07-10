/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __PLATFORM_CLK_MAP_H__
#define __PLATFORM_CLK_MAP_H__

#include <sof/clk.h>

static const struct freq_table cpu_freq[] = {
	{32000000, 32000, 0x6},
	{80000000, 80000, 0x2},
	{160000000, 160000, 0x1},
	{320000000, 320000, 0x4}, /* default */
	{320000000, 320000, 0x0},
	{160000000, 160000, 0x5},
};

static const struct freq_table ssp_freq[] = {
	{24000000, 24000, 0}, /* default */
};

#endif /* __PLATFORM_CLK_MAP_H__ */
