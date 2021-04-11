// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/memory.h>
#include <xtensa/config/core-isa.h>
#include <xtensa/hal.h>
#include <errno.h>
#include <stdint.h>

void timer_64_handler(void *arg)
{
	struct timer *timer = arg;
	uint32_t ccompare;

	if (timer->id >= ARCH_TIMER_COUNT)
		return;

	/* get comparator value - will tell us timeout reason */
	ccompare = xthal_get_ccompare(timer->id);

	/* is this a 32 bit rollover ? */
	if (ccompare == 1) {
		/* roll over the timer */
		timer->hitime++;
		arch_timer_clear(timer);
	} else {
		/* no roll over, run the handler */
		if (timer->handler)
			timer->handler(timer->data);
	}

	/* get next timeout value */
	if (timer->hitimeout == timer->hitime) {
		/* timeout is in this 32 bit period */
		ccompare = timer->lowtimeout;
	} else {
		/* timeout is in another 32 bit period */
		ccompare = 1;
	}

	xthal_set_ccompare(timer->id, ccompare);
}

int timer64_register(struct timer *timer, void(*handler)(void *arg), void *arg)
{
	if (timer->id >= ARCH_TIMER_COUNT)
		return -EINVAL;

	timer->handler = handler;
	timer->data = arg;
	timer->hitime = 0;
	timer->hitimeout = 0;

	return 0;
}

uint64_t arch_timer_get_system(struct timer *timer)
{
	uint64_t time = 0;
	uint32_t flags;
	uint32_t low;
	uint32_t high;
	uint32_t ccompare;

	if (timer->id >= ARCH_TIMER_COUNT)
		goto out;

	ccompare = xthal_get_ccompare(timer->id);

	flags = arch_interrupt_global_disable();

	/* read low 32 bits */
	low = xthal_get_ccount();

	/* check and see whether 32bit IRQ is pending for timer */
	if (arch_interrupt_get_status() & (1 << timer->irq) && ccompare == 1) {
		/* yes, overflow has occured but handler has not run */
		high = timer->hitime + 1;
	} else {
		/* no overflow */
		high = timer->hitime;
	}

	time = ((uint64_t)high << 32) | low;

	arch_interrupt_global_enable(flags);

out:

	return time;
}

int64_t arch_timer_set(struct timer *timer, uint64_t ticks)
{
	uint32_t time = 1;
	uint32_t hitimeout = ticks >> 32;
	uint32_t flags;
	int64_t ret;

	if (timer->id >= ARCH_TIMER_COUNT) {
		ret = -EINVAL;
		goto out;
	}

	/* value of 1 represents rollover */
	if ((ticks & 0xffffffff) == 0x1)
		ticks++;

	flags = arch_interrupt_global_disable();

	/* same hi 64 bit context as ticks ? */
	if (hitimeout < timer->hitime) {
		/* cant be in the past */
		arch_interrupt_global_enable(flags);
		ret = -EINVAL;
		goto out;
	}

	/* check whether new timeout requires timer
	 * rollover. If so, ccompare value should
	 * be set to 1 in order to increment timer->hitime
	 * properly in timer_64_handler().
	 */
	if (timer->hitime < hitimeout)
		time = 1;
	else
		time = ticks;

	timer->hitimeout = hitimeout;
	timer->lowtimeout = ticks;

	xthal_set_ccompare(timer->id, time);

	arch_interrupt_global_enable(flags);

	ret = ticks;

out:

	return ret;
}
