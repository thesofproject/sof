// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/lib/clk.h>
#include <sof/lib/notifier.h>
#include <sof/spinlock.h>

const struct freq_table platform_cpu_freq[] = {
	{ 666000000, 666000 },
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

static struct clock_info platform_clocks_info[NUM_CLOCKS];

struct clock_info *clocks = platform_clocks_info;

void platform_clock_init(void)
{
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		platform_clocks_info[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = NULL,
		};

		spinlock_init(&platform_clocks_info[i].lock);
	}
}
