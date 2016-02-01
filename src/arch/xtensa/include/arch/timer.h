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

static inline int arch_timer_register(int timer,
	void(*handler)(void *arg), void *arg)
{
	return arch_interrupt_register(timer, handler, arg);
}

static inline void arch_timer_unregister(int timer)
{
	arch_interrupt_unregister(timer);
}

static inline void arch_timer_enable(int timer)
{
	arch_interrupt_enable(timer);
}

static inline void arch_timer_disable(int timer)
{
	arch_interrupt_disable(timer);
}

static inline uint32_t arch_timer_get_system(void)
{
	return xthal_get_ccount();
}

void arch_timer_set(int timer, unsigned int ticks);

static inline void arch_timer_clear(int timer)
{
	arch_interrupt_clear(timer);
}

#endif
