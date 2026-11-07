/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <stdint.h>

struct sof;

#define CLK_CPU(x)	(x)

#define CPU_DEFAULT_IDX		0

#ifdef CONFIG_IMX8
#define CLK_DEFAULT_CPU_HZ	666000000
#define CLK_MAX_CPU_HZ		666000000
#else /* CONFIG_IMX8X */
#define CLK_DEFAULT_CPU_HZ	640000000
#define CLK_MAX_CPU_HZ		640000000
#endif

#define NUM_CLOCKS	1

#define NUM_CPU_FREQ	1

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
