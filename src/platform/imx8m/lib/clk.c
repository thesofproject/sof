// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/common.h>
#include <rtos/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <rtos/sof.h>

#include <zephyr/sys/util.h>

const struct freq_table platform_cpu_freq[] = {
	{ 800000000, 800000 },
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

void platform_clock_init(struct sof *sof)
{
	int i;

	sof->clocks = platform_shared_get(platform_clocks_info, sizeof(platform_clocks_info));

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = NULL,
		};
	}
}
