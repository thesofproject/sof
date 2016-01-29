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

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/timer.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/alloc.h>
#include <reef/init.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <stdint.h>
#include <errno.h>

int timer_register(int timer, void(*handler)(void *arg), void *arg)
{
	return interrupt_register(timer, handler, arg);
}

void timer_unregister(int timer)
{
	interrupt_unregister(timer);
}

void timer_enable(int timer)
{
	interrupt_enable(timer);
}

void timer_disable(int timer)
{
	interrupt_disable(timer);
}

void timer_clear(int timer)
{
	interrupt_clear(timer);
}

void timer_set(int timer, unsigned int ticks)
{
	switch (timer) {
	case TIMER0:
		xthal_set_ccompare(0, ticks);
		break;
	case TIMER1:
		xthal_set_ccompare(1, ticks);
		break;
	case TIMER2:
		xthal_set_ccompare(2, ticks);
		break;
	default:
		break;
	}
}

uint32_t timer_get_system(void)
{
	return xthal_get_ccount();
}
