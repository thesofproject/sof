/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __PLATFORM_CLK_MAP_H__
#define __PLATFORM_CLK_MAP_H__

#include <sof/clk.h>
static const struct freq_table cpu_freq[] = {
	{666000000, 666000, 0x0}, /* default */
};

/* TODO: abstract SSP clk based on platform and remove this from here! */
static const struct freq_table ssp_freq[] = {
	{19200000, 19200, 19}, /* default */
	{25000000, 25000, 12},
};

#endif /* __PLATFORM_CLK_MAP_H__ */
