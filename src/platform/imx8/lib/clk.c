// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/clk.h>
#include <sof/lib/notifier.h>

static struct freq_table platform_cpu_freq[] = {
	{ 666000000, 666000 },
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

/* TODO: abstract SSP clk based on platform and remove this from here! */
static struct freq_table platform_ssp_freq[] = {
	{ 19200000, 19200 },
	{ 25000000, 25000 },
};

uint32_t platform_ssp_freq_sources[] = {
	19,
	12,
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

struct freq_table *ssp_freq = platform_ssp_freq;
uint32_t *ssp_freq_sources = platform_ssp_freq_sources;

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
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = NULL,
		};
	}

	platform_clocks_info[CLK_SSP] = (struct clock_info) {
		.freqs_num = NUM_SSP_FREQ,
		.freqs = platform_ssp_freq,
		.default_freq_idx = SSP_DEFAULT_IDX,
		.notification_id = NOTIFIER_ID_SSP_FREQ,
		.notification_mask = NOTIFIER_TARGET_CORE_ALL_MASK,
		.set_freq = NULL,
	};
}
