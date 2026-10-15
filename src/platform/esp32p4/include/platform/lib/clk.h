/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <rtos/clk.h>

#define CPU_DEFAULT_IDX 0
#define CLK_CPU(x) (x)
#define NUM_CLOCKS 1
#define NUM_CPU_FREQ 1

#define CLK_MAX_CPU_HZ 400000000
#define CLK_DEFAULT_CPU_HZ 400000000

struct sof;
void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */
