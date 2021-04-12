// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <errno.h>
#include <stdint.h>

void platform_timer_start(struct timer *timer)
{
	//nothing to do on BDW & HSW for cpu timer
}

void platform_timer_stop(struct timer *timer)
{
	arch_timer_disable(timer);
}

int64_t platform_timer_set(struct timer *timer, uint64_t ticks)
{
	return arch_timer_set(timer, ticks);
}

void platform_timer_clear(struct timer *timer)
{
	arch_timer_clear(timer);
}

uint64_t platform_timer_get(struct timer *timer)
{
	return arch_timer_get_system(timer);
}

/* IRQs off in arch_timer_get_system() */
uint64_t platform_timer_get_atomic(struct timer *timer)
{
	return arch_timer_get_system(timer);
}

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get host position */
	err = comp_position(host, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_HOST_VALID | SOF_TIME_HOST_64;
}

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get DAI position */
	err = comp_position(dai, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_DAI_VALID;

	/* get SSP wallclock - DAI sets this to stream start value */
	posn->wallclock = timer_get_system(timer_get()) - posn->wallclock;
	posn->wallclock_hz = clock_get_freq(PLATFORM_DEFAULT_CLOCK);
	posn->flags |= SOF_TIME_WALL_VALID | SOF_TIME_WALL_64;
}

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	/* only 1 wallclock on HSW */
	*wallclock = timer_get_system(timer_get());
}

int timer_register(struct timer *timer, void (*handler)(void *arg), void *arg)
{
	int ret;

	switch (timer->id) {
	case TIMER0:
	case TIMER1:
	case TIMER2:
		ret = arch_timer_register(timer, handler, arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void timer_unregister(struct timer *timer, void *arg)
{
	interrupt_unregister(timer->irq, arg);

}

void timer_enable(struct timer *timer, void *arg, int core)
{
	interrupt_enable(timer->irq, arg);

}

void timer_disable(struct timer *timer, void *arg, int core)
{
	interrupt_disable(timer->irq, arg);

}
