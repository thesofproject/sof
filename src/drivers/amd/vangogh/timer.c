// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2022 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>

#include <xtensa/config/core-isa.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/component.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <errno.h>
#include <stdint.h>

void platform_timer_start(struct timer *timer)
{
	/* Nothing to do here */
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

uint64_t platform_timer_get_atomic(struct timer *timer)
{
	return arch_timer_get_system(timer);
}

void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn)
{
	int err;

	err = comp_position(host, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_HOST_VALID | SOF_TIME_HOST_64;
}

void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn)
{
	int err;

	err = comp_position(dai, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_DAI_VALID;

	posn->wallclock = timer_get_system(timer_get()) - posn->wallclock;
	posn->flags |= SOF_TIME_WALL_VALID | SOF_TIME_WALL_64;
}

void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	*wallclock = timer_get_system(timer_get());
}

int timer_register(struct timer *timer, void(*handler)(void *arg), void *arg)
{
	switch (timer->id) {
	case TIMER0:
	case TIMER1:
		return arch_timer_register(timer, handler, arg);
	default:
		return -EINVAL;
	}
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
