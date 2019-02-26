/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_TIMER_H__
#define __SOF_DRIVERS_TIMER_H__

#include <arch/drivers/timer.h>
#include <platform/drivers/timer.h>
#include <stdint.h>

struct comp_dev;
struct sof_ipc_stream_posn;

struct timesource_data {
	struct timer timer;
	int clk;
	int notifier;
	int (*timer_set)(struct timer *t, uint64_t ticks);
	void (*timer_clear)(struct timer *t);
	uint64_t (*timer_get)(struct timer *t);
};

extern struct timesource_data platform_generic_queue[];

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

int platform_timer_set(struct timer *timer, uint64_t ticks);
void platform_timer_clear(struct timer *timer);
uint64_t platform_timer_get(struct timer *timer);
void platform_timer_start(struct timer *timer);
void platform_timer_stop(struct timer *timer);

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn);

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn);

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock);

#endif /* __SOF_DRIVERS_TIMER_H__ */
