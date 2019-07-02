/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_TIMER__
#define __INCLUDE_TIMER__

#include <arch/timer.h>
#include <stdint.h>

struct timesource_data {
	struct timer timer;
	int clk;
	int notifier;
	int (*timer_set)(struct timer *t, uint64_t ticks);
	void (*timer_clear)(struct timer *t);
	uint64_t (*timer_get)(struct timer *t);
};

int timer_register(struct timer *timer,
	void (*handler)(void *arg), void *arg);
void timer_unregister(struct timer *timer);
void timer_enable(struct timer *timer);
void timer_disable(struct timer *timer);

static inline int timer_set(struct timer *timer, uint64_t ticks)
{
	return arch_timer_set(timer, ticks);
}

void timer_set_ms(struct timer *timer, unsigned int ms);

static inline void timer_clear(struct timer *timer)
{
	arch_timer_clear(timer);
}

unsigned int timer_get_count(struct timer *timer);

unsigned int timer_get_count_delta(struct timer *timer);

static inline uint64_t timer_get_system(struct timer *timer)
{
	return arch_timer_get_system(timer);
}

#endif
