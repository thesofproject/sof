// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

/* clock tracing */
#define trace_clk(__e, ...) \
	trace_event(TRACE_CLASS_CLK, __e, ##__VA_ARGS__)
#define tracev_clk(__e, ...) \
	tracev_event(TRACE_CLASS_CLK, __e, ##__VA_ARGS__)
#define trace_clk_error(__e, ...) \
	trace_error(TRACE_CLASS_CLK, __e, ##__VA_ARGS__)

struct clk_data {
	uint32_t freq;
	uint32_t ticks_per_msec;

	spinlock_t *lock;	/* for synchronizing freq set for each clock */
};

struct clk_pdata {
	struct clk_data clk[NUM_CLOCKS];
};

static struct clk_pdata *clk_pdata;

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
	return clk_pdata->clk[clock].freq;
}

void clock_set_freq(int clock, uint32_t hz)
{
	struct notify_data notify_data;
	struct clock_notify_data clk_notify_data;
	struct clock_info *clk_info = &clocks[clock];
	uint32_t idx;
	uint32_t flags;

	notify_data.data_size = sizeof(clk_notify_data);
	notify_data.data = &clk_notify_data;

	clk_notify_data.old_freq = clk_pdata->clk[clock].freq;
	clk_notify_data.old_ticks_per_msec =
		clk_pdata->clk[clock].ticks_per_msec;

	/* atomic context for changing clocks */
	spin_lock_irq(clk_pdata->clk[clock].lock, flags);

	notify_data.id = clk_info->notification_id;
	notify_data.target_core_mask = clk_info->notification_mask;

	/* get nearest frequency that is >= requested Hz */
	idx = clock_get_nearest_freq_idx(clk_info->freqs, clk_info->freqs_num,
					 hz);
	clk_notify_data.freq = clk_info->freqs[idx].freq;

	/* tell anyone interested we are about to change freq */
	notify_data.message = CLOCK_NOTIFY_PRE;
	notifier_event(&notify_data);

	if (!clk_info->set_freq ||
	    clk_info->set_freq(clock, idx) == 0) {
		/* update clock frequency */
		clk_pdata->clk[clock].freq = clk_info->freqs[idx].freq;
		clk_pdata->clk[clock].ticks_per_msec =
			clk_info->freqs[idx].ticks_per_msec;
	}

	/* tell anyone interested we have now changed freq */
	notify_data.message = CLOCK_NOTIFY_POST;
	notifier_event(&notify_data);

	spin_unlock_irq(clk_pdata->clk[clock].lock, flags);
}

uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	return clk_pdata->clk[clock].ticks_per_msec * ms;
}

void platform_timer_set_delta(struct timer *timer, uint64_t ns)
{
	uint64_t ticks;

	ticks = clk_pdata->clk[PLATFORM_DEFAULT_CLOCK].ticks_per_msec /
		1000 * ns / 1000;
	timer->delta = ticks - platform_timer_get(timer);
}

void clock_init(void)
{
	int i = 0;
	uint32_t def;

	clk_pdata = rmalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
			    sizeof(*clk_pdata));

	/* set defaults */
	for (i = 0; i < NUM_CLOCKS; i++) {
		def = clocks[i].default_freq_idx;
		clk_pdata->clk[i].freq = clocks[i].freqs[def].freq;
		clk_pdata->clk[i].ticks_per_msec =
			clocks[i].freqs[def].ticks_per_msec;
		spinlock_init(&clk_pdata->clk[i].lock);
	}
}

void clock_set_high_freq(void)
{
	uint32_t reg = io_reg_read(SHIM_BASE + SHIM_CLKCTL);

	reg = reg & ~(SHIM_CLKCTL_OCS_LP_RING);
	reg = reg | SHIM_CLKCTL_OCS_HP_RING;

	io_reg_write(SHIM_BASE + SHIM_CLKCTL, reg);
}

void clock_set_low_freq(void)
{
	uint32_t reg = io_reg_read(SHIM_BASE + SHIM_CLKCTL);

	reg = reg & ~(SHIM_CLKCTL_OCS_HP_RING);
	reg = reg | SHIM_CLKCTL_OCS_LP_RING;

	io_reg_write(SHIM_BASE + SHIM_CLKCTL, reg);
}
