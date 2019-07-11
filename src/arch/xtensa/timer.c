// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <sof/memory.h>
#include <sof/mailbox.h>
#include <sof/debug.h>
#include <sof/drivers/timer.h>
#include <sof/drivers/interrupt.h>
#include <stdint.h>
#include <errno.h>

struct timer_data {
	void (*handler2)(void *arg);
	void *arg2;
};

static struct timer_data xtimer[3] = {};

void timer_64_handler(void *arg)
{
	struct timer *timer = arg;
	struct timer_data *tdata = timer->timer_data;
	uint32_t ccompare;

	/* get comparator value - will tell us timeout reason */
	switch (timer->id) {
	case TIMER0:
		ccompare = xthal_get_ccompare(0);
		break;
#if XCHAL_NUM_TIMERS > 1
	case TIMER1:
		ccompare = xthal_get_ccompare(1);
		break;
#endif
#if XCHAL_NUM_TIMERS > 2
	case TIMER2:
		ccompare = xthal_get_ccompare(2);
		break;
#endif
	default:
		return;
	}

	/* is this a 32 bit rollover ? */
	if (ccompare == 1) {
		/* roll over the timer */
		timer->hitime++;
		arch_timer_clear(timer);
	} else {
		/* no roll over, run the handler */
		tdata->handler2(tdata->arg2);
	}

	/* get next timeout value */
	if (timer->hitimeout == timer->hitime) {
		/* timeout is in this 32 bit period */
		ccompare = timer->lowtimeout;
	} else {
		/* timeout is in another 32 bit period */
		ccompare = 1;
	}

	switch (timer->id) {
	case TIMER0:
		xthal_set_ccompare(0, ccompare);
		break;
#if XCHAL_NUM_TIMERS > 1
	case TIMER1:
		xthal_set_ccompare(1, ccompare);
		break;
#endif
#if XCHAL_NUM_TIMERS > 2
	case TIMER2:
		xthal_set_ccompare(2, ccompare);
		break;
#endif
	default:
		return;
	}
}

int timer64_register(struct timer *timer, void(*handler)(void *arg), void *arg)
{
	struct timer_data *tdata;

	switch (timer->id) {
	case TIMER0:
		tdata = &xtimer[0];
		break;
#if XCHAL_NUM_TIMERS > 1
	case TIMER1:
		tdata = &xtimer[1];
		break;
#endif
#if XCHAL_NUM_TIMERS > 2
	case TIMER2:
		tdata = &xtimer[2];
		break;
#endif
	default:
		return -EINVAL;
	}

	tdata->handler2 = handler;
	tdata->arg2 = arg;
	timer->timer_data = tdata;
	timer->hitime = 0;
	timer->hitimeout = 0;
	return 0;
}

uint64_t arch_timer_get_system(struct timer *timer)
{
	uint64_t time;
	uint32_t flags;
	uint32_t low;
	uint32_t high;
	uint32_t ccompare;

	switch (timer->id) {
	case TIMER0:
		ccompare = xthal_get_ccompare(0);
		break;
#if XCHAL_NUM_TIMERS > 1
	case TIMER1:
		ccompare = xthal_get_ccompare(1);
		break;
#endif
#if XCHAL_NUM_TIMERS > 2
	case TIMER2:
		ccompare = xthal_get_ccompare(2);
		break;
#endif
	default:
		return 0;
	}

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

	return time;
}

int arch_timer_set(struct timer *timer, uint64_t ticks)
{
	uint32_t time = 1;
	uint32_t hitimeout = ticks >> 32;
	uint32_t flags;

	/* value of 1 represents rollover */
	if ((ticks & 0xffffffff) == 0x1)
		ticks++;

	flags = arch_interrupt_global_disable();

	/* same hi 64 bit context as ticks ? */
	if (hitimeout < timer->hitime) {
		/* cant be in the past */
		arch_interrupt_global_enable(flags);
		return -EINVAL;
	}

	/* set for checking at next timeout */
	time = ticks;
	timer->hitimeout = hitimeout;
	timer->lowtimeout = ticks;

	switch (timer->id) {
	case TIMER0:
		xthal_set_ccompare(0, time);
		break;
#if XCHAL_NUM_TIMERS > 1
	case TIMER1:
		xthal_set_ccompare(1, time);
		break;
#endif
#if XCHAL_NUM_TIMERS > 2
	case TIMER2:
		xthal_set_ccompare(2, time);
		break;
#endif
	default:
		return -EINVAL;
	}

	arch_interrupt_global_enable(flags);
	return 0;
}
