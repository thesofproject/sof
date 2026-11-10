/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <stdint.h>

#define CPU_DEFAULT_IDX 0
#define NUM_CPU_FREQ 1
#define NUM_CLOCKS 1

struct sof;
void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */
