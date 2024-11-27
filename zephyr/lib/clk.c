// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/lib/notifier.h>
#include <rtos/clk.h>
#include <sof/drivers/ssp.h>
#include <rtos/sof.h>
#include <sof/common.h>
#include <sof/lib/memory.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

static int select_cpu_freq(int clock, int hz)
{
	return clock_control_set_rate(DEVICE_DT_GET(DT_NODELABEL(clkctl)), NULL,
				      (clock_control_subsys_rate_t)hz);
}

void platform_clock_init(struct sof *sof)
{
	uint32_t platform_lowest_clock = CPU_LOWEST_FREQ_IDX;
	int i;

	sof->clocks = platform_clocks_info;

	/*
	 * plafforms using this clk.c implementation must
	 * declare the CPU clocks first, and there must be at least
	 * one clock per core. If this model doesn't fit, platform
	 * needs a custom platform_clock_init.
	 */
	BUILD_ASSERT(NUM_CLOCKS >= CONFIG_CORE_COUNT, "Invalid NUM_CLOCKS");

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.lowest_freq_idx = platform_lowest_clock,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = select_cpu_freq,
		};
	}
}
