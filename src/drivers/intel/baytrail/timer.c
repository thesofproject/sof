// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Baytrail external timer control.
 */

#include <sof/audio/component_ext.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <errno.h>
#include <stdint.h>

static void platform_timer_64_handler(void *arg)
{
	struct timer *timer = arg;
	uint32_t timeout;

	/* get timeout value - will tell us timeout reason */
	timeout = shim_read(SHIM_EXT_TIMER_CNTLL);

	/* we don't use the timer clear bit as we only need to clear the ISR */
	shim_write(SHIM_PISR, SHIM_PISR_EXT_TIMER);

	/* is this a 32 bit rollover ? */
	if (timeout == 1) {
		/* roll over the timer */
		timer->hitime++;
	} else {
		/* no roll over, run the handler */
		timer->handler(timer->data);
	}

	/* get next timeout value */
	if (timer->hitimeout == timer->hitime) {
		/* timeout is in this 32 bit period */
		timeout = timer->lowtimeout;
	} else {
		/* timeout is in another 32 bit period */
		timeout = 1;
	}

	/* set new value and run */
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_RUN);
	shim_write(SHIM_EXT_TIMER_CNTLL, timeout);

}

void platform_timer_start(struct timer *timer)
{
	/* run timer */
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_RUN);
	shim_write(SHIM_EXT_TIMER_CNTLL, 1);
}

/* this seems to stop rebooting with RTD3 ???? */
void platform_timer_stop(struct timer *timer)
{
	/* run timer */
	shim_write(SHIM_EXT_TIMER_CNTLL, 0);
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_CLEAR);
}

int64_t platform_timer_set(struct timer *timer, uint64_t ticks)
{
	uint32_t time = 1;
	uint32_t hitimeout = ticks >> 32;
	uint32_t flags;

	/* a tick value of 0 will not generate an IRQ */
	/* value of 1 represents rollover */
	if ((ticks & 0xffffffff) < 0x2)
		ticks += 2;

	flags = arch_interrupt_global_disable();

	/* same hi 64 bit context as ticks ? */
	if (hitimeout < timer->hitime) {
		/* can't be in the past */
		arch_interrupt_global_enable(flags);
		return -EINVAL;
	}

	/* set for checking at next timeout */
	time = ticks;
	timer->hitimeout = hitimeout;
	timer->lowtimeout = ticks;

	/* set new value and run */
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_RUN);
	shim_write(SHIM_EXT_TIMER_CNTLL, time);

	arch_interrupt_global_enable(flags);

	return ticks;
}

void platform_timer_clear(struct timer *timer)
{
	/* we don't use the timer clear bit as we only need to clear the ISR */
	shim_write(SHIM_PISR, SHIM_PISR_EXT_TIMER);
}

uint64_t platform_timer_get(struct timer *timer)
{
	uint64_t time;
	uint32_t flags;
	uint32_t low;
	uint32_t high;

	flags = arch_interrupt_global_disable();

	/* read low 32 bits */
	low = shim_read(SHIM_EXT_TIMER_STAT);

	/* check and see whether 32bit IRQ is pending for timer */
	if (arch_interrupt_get_status() & IRQ_MASK_EXT_TIMER &&
	    shim_read(SHIM_EXT_TIMER_CNTLL) == 1) {
		/* yes, overflow has occurred but handler has not run */
		high = timer->hitime + 1;
	} else {
		/* no overflow */
		high = timer->hitime;
	}

	time = ((uint64_t)high << 32) | low;

	arch_interrupt_global_enable(flags);

	return time;
}

uint64_t platform_timer_get_atomic(struct timer *timer)
{
	return platform_timer_get(timer);
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
	posn->wallclock = platform_timer_get(timer_get()) - posn->wallclock;
	posn->wallclock_hz = clock_get_freq(PLATFORM_DEFAULT_CLOCK);
	posn->flags |= SOF_TIME_WALL_VALID | SOF_TIME_WALL_64;
}

/* get current wallclock for component */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	/* only 1 wallclock on BYT */
	*wallclock = platform_timer_get(timer_get());
}

static int platform_timer_register(struct timer *timer,
				   void (*handler)(void *arg), void *arg)
{
	uint32_t flags;
	int ret;

	flags = arch_interrupt_global_disable();
	timer->handler = handler;
	timer->data = arg;
	timer->hitime = 0;
	timer->hitimeout = 0;
	ret = arch_interrupt_register(timer->irq,
				      platform_timer_64_handler, timer);
	arch_interrupt_global_enable(flags);

	return ret;
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
	case TIMER3:
		ret = platform_timer_register(timer, handler, arg);
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
