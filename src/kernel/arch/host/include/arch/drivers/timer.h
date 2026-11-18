/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_TIMER_H__

#ifndef __ARCH_DRIVERS_TIMER_H__
#define __ARCH_DRIVERS_TIMER_H__

#include <stdint.h>

struct timer {
	uint32_t id;
	uint64_t delta;
};

static inline int arch_timer_register(struct timer *timer,
	void (*handler)(void *arg), void *arg) {return 0; }
static inline void arch_timer_unregister(struct timer *timer) {}
static inline void arch_timer_enable(struct timer *timer) {}
static inline void arch_timer_disable(struct timer *timer) {}
static inline uint64_t arch_timer_get_system(struct timer *timer) {return 0; }
static inline int64_t arch_timer_set(struct timer *timer,
				     uint64_t ticks) {return 0; }
static inline void arch_timer_clear(struct timer *timer) {}

#endif /* __ARCH_DRIVERS_TIMER_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/timer.h"

#endif /* __SOF_DRIVERS_TIMER_H__ */
