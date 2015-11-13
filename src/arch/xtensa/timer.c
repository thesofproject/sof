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
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/alloc.h>
#include <reef/init.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <stdint.h>
#include <errno.h>

static uint32_t _mask = 0, _enable = 0;

static int get_irq_from_timer(int timer)
{
	switch (timer) {
	case 0:
		return IRQ_NUM_TIMER1;
	case 1:
		return IRQ_NUM_TIMER2;
	case 2:
		return IRQ_NUM_TIMER3;
	default:
		return -ENODEV;
	}
}

int timer_register(int timer, void(*handler)(void *arg), void *arg)
{
	int irq;

	irq = get_irq_from_timer(timer);
	if (irq < 0)
		return irq;

	return interrupt_register(irq, handler, arg);
}

void timer_unregister(int timer)
{
	int irq;

	irq = get_irq_from_timer(timer);
	if (irq < 0)
		return;
	interrupt_unregister(irq);
}

void timer_enable(int timer)
{
	int irq;

	irq = get_irq_from_timer(timer);
	if (irq < 0)
		return;

	interrupt_enable(irq);
}

void timer_disable(int timer)
{
	int irq;

	irq = get_irq_from_timer(timer);
	if (irq < 0)
		return;

	interrupt_disable(irq);
}

void timer_set(int timer, unsigned int ticks)
{
	uint32_t count;

	count = xthal_get_ccount();
	xthal_set_ccompare(timer, count + ticks);
}

uint32_t timer_get_system(void)
{
	return xthal_get_ccount();
}
