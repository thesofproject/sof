/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H___

#define CLK_CPU(x) (x)
#define CPU_DEFAULT_IDX 0
#define NUM_CLOCKS 1
#define NUM_CPU_FREQ 1
#define CLK_MAX_CPU_HZ CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC

struct sof;

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
