/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_LIB_CLK_H__
#define __SOF_LIB_CLK_H__

#include <xtensa/config/core-isa.h>
#include <platform/lib/clk.h>
#include <stdint.h>

struct timer;

#define CLOCK_NOTIFY_PRE	0
#define CLOCK_NOTIFY_POST	1

struct clock_notify_data {
	uint32_t old_freq;
	uint32_t old_ticks_per_msec;
	uint32_t freq;
	uint32_t ticks_per_msec;
};

struct freq_table {
	uint32_t freq;
	uint32_t ticks_per_msec;
};

struct clock_info {
	uint32_t freqs_num;
	struct freq_table *freqs;
	uint32_t default_freq_idx;
	uint32_t notification_id;
	uint32_t notification_mask;
	int (*set_freq)(int clock, int freq_idx);
};

extern struct clock_info *clocks;

uint32_t clock_get_freq(int clock);

void clock_set_freq(int clock, uint32_t hz);

uint64_t clock_ms_to_ticks(int clock, uint64_t ms);

void platform_timer_set_delta(struct timer *timer, uint64_t ns);

void clock_init(void);

void clock_set_high_freq(void);

void clock_set_low_freq(void);

#endif /* __SOF_LIB_CLK_H__ */
