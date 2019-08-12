/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_TIMER_H_
#define __ARCH_TIMER_H_

#include <kernel.h>

struct timer {
	uint32_t id;
	uint32_t irq;
	void *timer_data;	/* used by core */
	uint32_t hitime;	/* high end of 64bit timer */
	uint32_t hitimeout;
	uint32_t lowtimeout;
	uint64_t delta;
};

static inline int arch_timer_register(struct timer *timer,
	void (*handler)(void *arg), void *arg) {return 0; }
static inline void arch_timer_unregister(struct timer *timer) {}
static inline void arch_timer_enable(struct timer *timer) {}
static inline void arch_timer_disable(struct timer *timer) {}
static inline uint32_t arch_timer_get_system(struct timer *timer) {return 0; }
static inline int arch_timer_set(struct timer *timer,
	uint64_t ticks) {return 0; }
static inline void arch_timer_clear(struct timer *timer) {}

#endif
