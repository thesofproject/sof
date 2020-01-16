/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_LIB_CLK_H__
#define __SOF_LIB_CLK_H__

#include <platform/lib/clk.h>
#include <stdint.h>

struct timer;

#define CLOCK_NOTIFY_PRE	0
#define CLOCK_NOTIFY_POST	1

#define CLOCK_SSP_XTAL_OSCILLATOR		0x0
#define CLOCK_SSP_AUDIO_CARDINAL		0x1
#define CLOCK_SSP_PLL_FIXED			0x2

struct clock_notify_data {
	uint32_t old_freq;
	uint32_t old_ticks_per_msec;
	uint32_t freq;
	uint32_t ticks_per_msec;
};

struct freq_table {
	uint32_t freq;
	uint32_t ticks_per_msec;
	uint32_t enc;
};

extern struct freq_table *cpu_freq;

extern struct freq_table *ssp_freq;

uint32_t clock_get_freq(int clock);

void clock_set_freq(int clock, uint32_t hz);

uint64_t clock_ms_to_ticks(int clock, uint64_t ms);

void platform_timer_set_delta(struct timer *timer, uint64_t ns);

void clock_init(void);

void clock_set_high_freq(void);

void clock_set_low_freq(void);

#endif /* __SOF_LIB_CLK_H__ */
