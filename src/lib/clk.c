// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <rtos/timer.h>
#include <rtos/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/platform.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

LOG_MODULE_REGISTER(clock, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(clock);

DECLARE_TR_CTX(clock_tr, SOF_UUID(clock_uuid), LOG_LEVEL_INFO);

SHARED_DATA struct k_spinlock clk_lock;

static inline uint32_t clock_get_nearest_freq_idx(const struct freq_table *tab,
						  uint32_t size, uint32_t hz)
{
	uint32_t i;

	/* find lowest available frequency that is >= requested hz */
	for (i = 0; i < size; i++) {
		if (hz <= tab[i].freq)
			return i;
	}

	/* not found, so return max frequency */
	return size - 1;
}

uint32_t clock_get_freq(int clock)
{
	struct clock_info *clk_info = clocks_get() + clock;
	uint32_t freq = clk_info->freqs[clk_info->current_freq_idx].freq;

	return freq;
}

void clock_set_freq(int clock, uint32_t hz)
{
	struct clock_info *clk_info = clocks_get() + clock;
	uint32_t idx;
	k_spinlock_key_t key;

	/* atomic context for changing clocks */
	key = clock_lock();

	/* get nearest frequency that is >= requested Hz */
	idx = clock_get_nearest_freq_idx(clk_info->freqs, clk_info->freqs_num,
					 hz);

	if (clk_info->current_freq_idx != idx &&
	    (!clk_info->set_freq ||
	     clk_info->set_freq(clock, idx) == 0)) {
		tr_info(&clock_tr, "clock %d set freq %dHz freq_idx %d old %d",
			clock, hz, idx, clk_info->current_freq_idx);

		/* update clock frequency */
		clk_info->current_freq_idx = idx;
	}

	clock_unlock(key);
}

void clock_low_power_mode(int clock, bool enable)
{
	struct clock_info *clk_info = clocks_get() + clock;

	if (clk_info->low_power_mode)
		clk_info->low_power_mode(clock, enable);
}

uint64_t clock_ticks_per_sample(int clock, uint32_t sample_rate)
{
	struct clock_info *clk_info = clocks_get() + clock;
	uint32_t ticks_per_msec;
	uint64_t ticks_per_sample;

	platform_shared_get(clk_info, sizeof(*clk_info));
	ticks_per_msec = clk_info->freqs[clk_info->current_freq_idx].ticks_per_msec;
	ticks_per_sample = sample_rate ? ticks_per_msec * 1000 / sample_rate : 0;

	return ticks_per_sample;
}
