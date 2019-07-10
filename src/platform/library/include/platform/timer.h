/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __PLATFORM_TIMER_H__
#define __PLATFORM_TIMER_H__

#include <stdint.h>
#include <sof/timer.h>
#include <platform/interrupt.h>

struct comp_dev;
struct sof_ipc_stream_posn;

int platform_timer_set(struct timer *timer, uint64_t ticks);
void platform_timer_clear(struct timer *timer);
uint64_t platform_timer_get(struct timer *timer);
void platform_timer_start(struct timer *timer);
void platform_timer_stop(struct timer *timer);
int platform_timer_register(struct timer *timer,
			    void (*handler)(void *arg), void *arg);

/* get timestamp for host stream DMA position */
static inline void platform_host_timestamp(struct comp_dev *host,
	struct sof_ipc_stream_posn *posn) {}

/* get timestamp for DAI stream DMA position */
static inline void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn) {}

/* get current wallclock for componnent */
static inline void platform_dai_wallclock(struct comp_dev *dai,
	uint64_t *wallclock) {}

#endif /* __PLATFORM_TIMER_H__ */
