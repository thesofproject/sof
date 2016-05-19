/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Xtensa architecture initialisation.
 */

#ifndef __ARCH_TIMER_H_
#define __ARCH_TIMER_H_

#include <arch/interrupt.h>
#include <stdint.h>
#include <errno.h>

struct timer {
	uint32_t id;
	uint32_t irq;
};

static inline int arch_timer_register(struct timer *timer,
	void(*handler)(void *arg), void *arg)
{
	return arch_interrupt_register(timer->id, handler, arg);
}

static inline void arch_timer_unregister(struct timer *timer)
{
	arch_interrupt_unregister(timer->id);
}

static inline void arch_timer_enable(struct timer *timer)
{
	arch_interrupt_enable_mask(1 << timer->irq);
}

static inline void arch_timer_disable(struct timer *timer)
{
	arch_interrupt_disable_mask(1 << timer->irq);
}

static inline uint32_t arch_timer_get_system(void)
{
	return xthal_get_ccount();
}

void arch_timer_set(struct timer *timer, unsigned int ticks);

static inline void arch_timer_clear(struct timer *timer)
{
	arch_interrupt_clear(timer->irq);
}

#endif
