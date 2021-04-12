// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/audio/component_ext.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <stdint.h>

/** \brief Minimum number of timer recovery cycles in case of delay. */
#define TIMER_MIN_RECOVER_CYCLES	240	/* ~10us at 24.576MHz */

void platform_timer_start(struct timer *timer)
{
	/* run timer */
	shim_write64(SHIM_DSPWCT0C, 0);
	shim_write(SHIM_DSPWCTCS,
		   shim_read(SHIM_DSPWCTCS) | SHIM_DSPWCTCS_T0A);
}

void platform_timer_stop(struct timer *timer)
{
	/* stop timer */
	shim_write64(SHIM_DSPWCT0C, 0);
	shim_write(SHIM_DSPWCTCS,
		   shim_read(SHIM_DSPWCTCS) & ~SHIM_DSPWCTCS_T0A);
}

int64_t platform_timer_set(struct timer *timer, uint64_t ticks)
{
	uint64_t ticks_now;
	uint32_t flags;

	/* a tick value of 0 will not generate an IRQ */
	if (ticks == 0)
		ticks = 1;

	irq_local_disable(flags);

	ticks_now = platform_timer_get(timer);

	/* Check if requested time is not past time and include the
	 * overhead of changing the timer
	 */
	if (ticks > ticks_now + TIMER_MIN_RECOVER_CYCLES) {
		shim_write64(SHIM_DSPWCT0C, ticks);
	} else {
		ticks = ticks_now + TIMER_MIN_RECOVER_CYCLES;
		if (ticks == 0)
			ticks = 1;

		shim_write64(SHIM_DSPWCT0C, ticks);
	}

	/* Enable IRQ */
	shim_write(SHIM_DSPWCTCS, SHIM_DSPWCTCS_T0A);

	irq_local_enable(flags);

	return shim_read64(SHIM_DSPWCT0C);
}

void platform_timer_clear(struct timer *timer)
{
	/* write 1 to clear the timer interrupt */
	shim_write(SHIM_DSPWCTCS, SHIM_DSPWCTCS_T0T);
}

uint64_t platform_timer_get(struct timer *timer)
{
	uint32_t hi0;
	uint32_t hi1;
	uint32_t lo;
	uint64_t ticks_now;

	/* 64bit reads are non atomic on xtensa so we must
	 * read a stable value where there is no bit 32 flipping.
	 * A large delta between reads[0..1] means we have flipped
	 * and that the value read back in either 0..1] is invalid.
	 */
	do {
		hi0 = shim_read(SHIM_DSPWCH);
		lo = shim_read(SHIM_DSPWCL);
		hi1 = shim_read(SHIM_DSPWCH);

		/* worst case is we perform this twice so 6 * 32b clock reads */
	} while (hi0 != hi1);

	ticks_now = (((uint64_t)hi0) << 32) | lo;

	return ticks_now;
}

uint64_t platform_timer_get_atomic(struct timer *timer)
{
	uint32_t flags;
	uint64_t ticks_now;

	irq_local_disable(flags);
	ticks_now = platform_timer_get(timer);
	irq_local_enable(flags);

	return ticks_now;
}

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get host position */
	err = comp_position(host, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_HOST_VALID;
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
	posn->wallclock = shim_read64(SHIM_DSPWC) - posn->wallclock;
	posn->wallclock_hz = clock_get_freq(PLATFORM_DEFAULT_CLOCK);
	posn->flags |= SOF_TIME_WALL_VALID;
}

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	*wallclock = shim_read64(SHIM_DSPWC);
}

static int platform_timer_register(struct timer *timer,
				   void (*handler)(void *arg), void *arg)
{
	int err;

	/* register timer interrupt */
	timer->logical_irq = interrupt_get_irq(timer->irq, timer->irq_name);
	if (timer->logical_irq < 0)
		return timer->logical_irq;

	err = interrupt_register(timer->logical_irq, handler, arg);
	if (err < 0)
		return err;

	/* enable timer interrupt */
	interrupt_enable(timer->logical_irq, arg);

	/* disable timer interrupt on core level */
	timer_disable(timer, arg, cpu_get_id());

	return err;
}

int timer_register(struct timer *timer, void (*handler)(void *arg), void *arg)
{
	int ret;

	switch (timer->id) {
	case TIMER0:
	case TIMER1:
	case TIMER2:
		/* arch timers have no children, so HW IRQ is logical IRQ */
		timer->logical_irq = timer->irq;
		ret = arch_timer_register(timer, handler, arg);
		break;
	case TIMER3:
	case TIMER4:
		ret = platform_timer_register(timer, handler, arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void platform_timer_unregister(struct timer *timer, void *arg)
{
	/* disable timer interrupt */
	interrupt_disable(timer->logical_irq, arg);

	/* unregister timer interrupt */
	interrupt_unregister(timer->logical_irq, arg);
}

void timer_unregister(struct timer *timer, void *arg)
{
	switch (timer->id) {
	case TIMER0:
	case TIMER1:
	case TIMER2:
		interrupt_unregister(timer->logical_irq, arg);
		break;
	case TIMER3:
	case TIMER4:
		platform_timer_unregister(timer, arg);
		break;
	}

}

void timer_enable(struct timer *timer, void *arg, int core)
{
	switch (timer->id) {
	case TIMER0:
	case TIMER1:
	case TIMER2:
		interrupt_enable(timer->logical_irq, arg);
		break;
	case TIMER3:
	case TIMER4:
		interrupt_unmask(timer->logical_irq, core);
		break;
	}

}

void timer_disable(struct timer *timer, void *arg, int core)
{
	switch (timer->id) {
	case TIMER0:
	case TIMER1:
	case TIMER2:
		interrupt_disable(timer->logical_irq, arg);
		break;
	case TIMER3:
	case TIMER4:
		interrupt_mask(timer->logical_irq, core);
		break;
	}

}
