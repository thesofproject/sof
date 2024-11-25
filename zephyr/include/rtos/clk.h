/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_CLK_H__
#define __ZEPHYR_RTOS_CLK_H__

#ifndef CONFIG_SOF_ZEPHYR_NO_SOF_CLOCK

#include <zephyr/kernel.h>

#define __SOF_LIB_CLK_H__
#include <platform/lib/clk.h>

/* TODO: most of the below is tied to the CLK drivers that need wrapped */
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <stdbool.h>
#include <stdint.h>

struct timer;

#define CLOCK_NOTIFY_PRE	0
#define CLOCK_NOTIFY_POST	1

struct clock_notify_data {
	uint32_t old_freq;
	uint32_t old_ticks_per_msec;
	uint32_t freq;
	uint32_t ticks_per_msec;
	uint32_t message;
};

struct freq_table {
	uint32_t freq;
	uint32_t ticks_per_msec;
};

struct clock_info {
	uint32_t freqs_num;
	const struct freq_table *freqs;
	uint32_t default_freq_idx;
	uint32_t current_freq_idx;
	uint32_t lowest_freq_idx;	/* lowest possible clock */
	uint32_t notification_id;
	uint32_t notification_mask;

	/* persistent change clock value in active state, caller must hold clk_lock */
	int (*set_freq)(int clock, int freq_idx);

	/* temporary change clock - don't modify default clock settings */
	void (*low_power_mode)(int clock, bool enable);
};

uint32_t clock_get_freq(int clock);

void clock_set_freq(int clock, uint32_t hz);

void clock_low_power_mode(int clock, bool enable);

uint64_t clock_ticks_per_sample(int clock, uint32_t sample_rate);

extern struct k_spinlock clk_lock;

static inline k_spinlock_key_t clock_lock(void)
{
	return k_spin_lock(&clk_lock);
}

static inline void clock_unlock(k_spinlock_key_t key)
{
	k_spin_unlock(&clk_lock, key);
}

static inline struct clock_info *clocks_get(void)
{
	return sof_get()->clocks;
}

#endif /* CONFIG_SOF_ZEPHYR_NO_SOF_CLOCK */

#endif /* __ZEPHYR_RTOS_CLK_H__ */
