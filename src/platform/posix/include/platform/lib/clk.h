/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */

#define CLK_MAX_CPU_HZ CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
#define CPU_LPRO_FREQ_IDX 1
#define CPU_LOWEST_FREQ_IDX CPU_LPRO_FREQ_IDX

/* This is not a platform function, it's defined in src/lib/clk.c.
 * But the declaration has historically been in the platform layer, so
 * it's repeated here.
 */
uint32_t clock_get_freq(int clock);
