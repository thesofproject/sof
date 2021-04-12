// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/drivers/timer.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/platform.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

/* 8890ea76-0df9-44ae-87e6-994f4c15e9fa */
DECLARE_SOF_UUID("clock", clock_uuid, 0x8890ea76, 0x0df9, 0x44ae,
		 0x87, 0xe6, 0x99, 0x4f, 0x4c, 0x15, 0xe9, 0xfa);

DECLARE_TR_CTX(clock_tr, SOF_UUID(clock_uuid), LOG_LEVEL_INFO);

struct clock_notify_data clk_notify_data;

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
	uint32_t flags;

	clk_notify_data.old_freq =
		clk_info->freqs[clk_info->current_freq_idx].freq;
	clk_notify_data.old_ticks_per_msec =
		clk_info->freqs[clk_info->current_freq_idx].ticks_per_msec;

	/* atomic context for changing clocks */
	spin_lock_irq(&clk_info->lock, flags);

	/* get nearest frequency that is >= requested Hz */
	idx = clock_get_nearest_freq_idx(clk_info->freqs, clk_info->freqs_num,
					 hz);
	clk_notify_data.freq = clk_info->freqs[idx].freq;

	tr_info(&clock_tr, "clock %d set freq %dHz freq_idx %d",
		clock, hz, idx);

	/* tell anyone interested we are about to change freq */
	clk_notify_data.message = CLOCK_NOTIFY_PRE;
	notifier_event(clk_info, clk_info->notification_id,
		       clk_info->notification_mask, &clk_notify_data,
		       sizeof(clk_notify_data));

	if (!clk_info->set_freq ||
	    clk_info->set_freq(clock, idx) == 0)
		/* update clock frequency */
		clk_info->current_freq_idx = idx;

	/* tell anyone interested we have now changed freq */
	clk_notify_data.message = CLOCK_NOTIFY_POST;
	notifier_event(clk_info, clk_info->notification_id,
		       clk_info->notification_mask, &clk_notify_data,
		       sizeof(clk_notify_data));

	spin_unlock_irq(&clk_info->lock, flags);
}

void clock_low_power_mode(int clock, bool enable)
{
	struct clock_info *clk_info = clocks_get() + clock;

	if (clk_info->low_power_mode)
		clk_info->low_power_mode(clock, enable);
}

uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	struct clock_info *clk_info = clocks_get() + clock;
	uint64_t ticks;

	ticks = clk_info->freqs[clk_info->current_freq_idx].ticks_per_msec * ms;

	return ticks;
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

void platform_timer_set_delta(struct timer *timer, uint64_t ns)
{
	struct clock_info *clk_info = clocks_get() + PLATFORM_DEFAULT_CLOCK;
	uint32_t ticks_per_msec =
		clk_info->freqs[clk_info->current_freq_idx].ticks_per_msec;
	uint64_t ticks;

	ticks = ticks_per_msec * ns / 1000000;
	timer->delta = ticks - platform_timer_get(timer);

}
