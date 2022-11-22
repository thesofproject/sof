// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Krzysztof Frydryk <frydryk.krzysztof@intel.com>

#include <rtos/spinlock.h>
#include <sof/sof.h>
#include <stdint.h>
#include <sof/lib/kcps-budget-manager.h>
#include <rtos/clk.h>
#include <sof/sof.h>
#include <sof/math/numbers.h>
#ifdef __ZEPHYR__
#include <zephyr/sys/util.h>
#endif /* __ZEPHYR__ */

static struct kcps_budget_data kcps_data;

static int request_freq_change(int core, int freq)
{
	int current_freq;
	int selected_freq_id;
	struct clock_info *clk;

	clk = clocks_get() + core;

	for (selected_freq_id = 0; selected_freq_id < clk->freqs_num; selected_freq_id++) {
		if (freq < clk->freqs[selected_freq_id].freq)
			break;
	}

	/* don't change clock frequency if already using proper clock */
	current_freq = clock_get_freq(core);
	if (clk->freqs[selected_freq_id].freq != current_freq)
		clock_set_freq(core, freq);

	return 0;
}

static int max_core_consumption(void)
{
	int result = 0;
	int core;

	for (core = 0; core < CONFIG_CORE_COUNT; core++)
		result = MAX(result, kcps_data.kcps_consumption[core]);

	return result;
}

int core_kcps_adjust(int core, int kcps_delta)
{
	k_spinlock_key_t key;
	int freq;
	int core_id;

	key = k_spin_lock(&kcps_data.lock);
	kcps_data.kcps_consumption[core] += kcps_delta;

	/* set clock according to maximum requested mcps budget */
	freq = max_core_consumption();

	for (core_id = 0; core_id < CONFIG_CORE_COUNT; core_id++)
		request_freq_change(core_id, freq);

	k_spin_unlock(&kcps_data.lock, key);
	return 0;
}

int core_kcps_get(int core)
{
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&kcps_data.lock);
	ret = kcps_data.kcps_consumption[core];
	k_spin_unlock(&kcps_data.lock, key);
	return ret;
}

int kcps_budget_init(void)
{
	k_spinlock_init(&kcps_data.lock);

	return 0;
}
