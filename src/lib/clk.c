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

typedef int (*set_frequency)(uint32_t);

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
	set_frequency set_freq = NULL;
	const struct freq_table *freq_table = NULL;
	uint32_t freq_table_size = 0;
	uint32_t idx;
	uint32_t flags;

	notify_data.data_size = sizeof(clk_notify_data);
	notify_data.data = &clk_notify_data;

	clk_notify_data.old_freq = clk_pdata->clk[clock].freq;
	clk_notify_data.old_ticks_per_msec =
		clk_pdata->clk[clock].ticks_per_msec;

	/* atomic context for changing clocks */
	spin_lock_irq(clk_pdata->clk[clock].lock, flags);

	switch (clock) {
	case CLK_CPU(0) ... CLK_CPU(PLATFORM_CORE_COUNT - 1):
		set_freq = &clock_platform_set_cpu_freq;
		freq_table = cpu_freq;
		freq_table_size = NUM_CPU_FREQ;
		notify_data.id = NOTIFIER_ID_CPU_FREQ;
		notify_data.target_core_mask =
			NOTIFIER_TARGET_CORE_MASK(cpu_get_id());
		break;
	case CLK_SSP:
		set_freq = &clock_platform_set_ssp_freq;
		freq_table = ssp_freq;
		freq_table_size = NUM_SSP_FREQ;
		notify_data.id = NOTIFIER_ID_SSP_FREQ;
		notify_data.target_core_mask = NOTIFIER_TARGET_CORE_ALL_MASK;
		break;
	default:
		trace_clk_error("clk: invalid clock type %d", clock);
		goto out;
	}

	/* get nearest frequency that is >= requested Hz */
	idx = clock_get_nearest_freq_idx(freq_table, freq_table_size, hz);
	clk_notify_data.freq = freq_table[idx].freq;

	/* tell anyone interested we are about to change freq */
	notify_data.message = CLOCK_NOTIFY_PRE;
	notifier_event(&notify_data);

	if (set_freq(freq_table[idx].enc) == 0) {
		/* update clock frequency */
		clk_pdata->clk[clock].freq = freq_table[idx].freq;
		clk_pdata->clk[clock].ticks_per_msec =
			freq_table[idx].ticks_per_msec;
	}

	/* tell anyone interested we have now changed freq */
	notify_data.message = CLOCK_NOTIFY_POST;
	notifier_event(&notify_data);

out:
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

	clk_pdata = rmalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
			    sizeof(*clk_pdata));

	/* set defaults */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		clk_pdata->clk[i].freq = cpu_freq[CPU_DEFAULT_IDX].freq;
		clk_pdata->clk[i].ticks_per_msec =
			cpu_freq[CPU_DEFAULT_IDX].ticks_per_msec;
		spinlock_init(&clk_pdata->clk[i].lock);
	}

	clk_pdata->clk[CLK_SSP].freq = ssp_freq[SSP_DEFAULT_IDX].freq;
	clk_pdata->clk[CLK_SSP].ticks_per_msec =
			ssp_freq[SSP_DEFAULT_IDX].ticks_per_msec;
	spinlock_init(&clk_pdata->clk[CLK_SSP].lock);
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
