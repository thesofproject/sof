/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_TIMER__
#define __INCLUDE_TIMER__

#include <arch/timer.h>
#include <stdint.h>

static inline int timer_register(struct timer *timer,
	void(*handler)(void *arg), void *arg)
{
	return arch_timer_register(timer, handler, arg);
}

static inline void timer_unregister(struct timer *timer)
{
	arch_timer_unregister(timer);
}

static inline void timer_enable(struct timer *timer)
{
	arch_timer_enable(timer);
}

static inline void timer_disable(struct timer *timer)
{
	arch_timer_disable(timer);
}

static inline void timer_set(struct timer *timer, unsigned int ticks)
{
	arch_timer_set(timer, ticks);
}

void timer_set_ms(struct timer *timer, unsigned int ms);

static inline void timer_clear(struct timer *timer)
{
	arch_timer_clear(timer);
}

unsigned int timer_get_count(struct timer *timer);

unsigned int timer_get_count_delta(struct timer *timer);

static inline uint32_t timer_get_system(void)
{
	return arch_timer_get_system();
}

#endif
