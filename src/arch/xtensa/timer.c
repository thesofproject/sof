/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <arch/timer.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/timer.h>
#include <sof/mailbox.h>
#include <sof/debug.h>
#include <sof/timer.h>
#include <sof/interrupt.h>
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
	case TIMER1:
		ccompare = xthal_get_ccompare(1);
		break;
	case TIMER2:
		ccompare = xthal_get_ccompare(2);
		break;
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
	case TIMER1:
		xthal_set_ccompare(1, ccompare);
		break;
	case TIMER2:
		xthal_set_ccompare(2, ccompare);
		break;
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
	case TIMER1:
		tdata = &xtimer[1];
		break;
	case TIMER2:
		tdata = &xtimer[2];
		break;
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
	case TIMER1:
		ccompare = xthal_get_ccompare(1);
		break;
	case TIMER2:
		ccompare = xthal_get_ccompare(2);
		break;
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
	} else {
		/* set for checking at next timeout */
		time = ticks;
		timer->hitimeout = hitimeout;
		timer->lowtimeout = ticks;
	}

	switch (timer->id) {
	case TIMER0:
		xthal_set_ccompare(0, time);
		break;
	case TIMER1:
		xthal_set_ccompare(1, time);
		break;
	case TIMER2:
		xthal_set_ccompare(2, time);
		break;
	default:
		return -EINVAL;
	}

	arch_interrupt_global_enable(flags);
	return 0;
}

void timer_unregister(struct timer *timer)
{
	interrupt_unregister(timer->irq);
}

void timer_enable(struct timer *timer)
{
	interrupt_enable(timer->irq);
}

void timer_disable(struct timer *timer)
{
	interrupt_disable(timer->irq);
}

