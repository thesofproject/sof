/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#include <sof/clk.h>
#include <sof/list.h>
#include <sof/alloc.h>
#include <sof/lock.h>
#include <sof/notifier.h>
#include <sof/cpu.h>
#include <platform/clk.h>
#include <platform/clk-map.h>
#include <platform/platcfg.h>
#include <config.h>
#include <stdint.h>
#include <limits.h>

typedef int (*set_frequency)(uint32_t);

struct clk_data {
	uint32_t freq;
	uint32_t ticks_per_msec;

	/* for synchronizing freq set for each clock */
	spinlock_t lock;
};

struct clk_pdata {
	struct clk_data clk[NUM_CLOCKS];
};

static struct clk_pdata *clk_pdata;

static inline uint32_t clock_get_freq(const struct freq_table *table,
				      uint32_t size, uint32_t hz)
{
	uint32_t i;

	/* find lowest available frequency that is >= requested hz */
	for (i = 0; i < size; i++) {
		if (hz <= table[i].freq)
			return i;
	}

	/* not found, so return max frequency */
	return size - 1;
}

uint32_t clock_set_freq(int clock, uint32_t hz)
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
	spin_lock_irq(&clk_pdata->clk[clock].lock, flags);

	switch (clock) {
	case CLK_CPU(0) ... CLK_CPU(PLATFORM_CORE_COUNT - 1):
		set_freq = &clock_platform_set_cpu_freq;
		freq_table = cpu_freq;
		freq_table_size = ARRAY_SIZE(cpu_freq);
		notify_data.id = NOTIFIER_ID_CPU_FREQ;
		notify_data.target_core_mask =
			NOTIFIER_TARGET_CORE_MASK(cpu_get_id());
		break;
	case CLK_SSP:
		set_freq = &clock_platform_set_ssp_freq;
		freq_table = ssp_freq;
		freq_table_size = ARRAY_SIZE(ssp_freq);
		notify_data.id = NOTIFIER_ID_SSP_FREQ;
		notify_data.target_core_mask = NOTIFIER_TARGET_CORE_ALL_MASK;
		break;
	default:
		break;
	}

	/* get nearest frequency that is >= requested Hz */
	idx = clock_get_freq(freq_table, freq_table_size, hz);
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

	spin_unlock_irq(&clk_pdata->clk[clock].lock, flags);

	return clk_pdata->clk[clock].freq;
}

uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	return clk_pdata->clk[clock].ticks_per_msec * ms;
}

void clock_init(void)
{
	int i = 0;

	clk_pdata = rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*clk_pdata));

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
