// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#include <sof/audio/component_ext.h>
#include <rtos/interrupt.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <platform/drivers/timer.h>
#include <ipc/stream.h>
#include <errno.h>
#include <stdint.h>

void platform_timer_start(struct timer *timer)
{
	if (timer->id >= NR_TMRS)
		return;

	/*timer use 13M clksrc*/
	/*clear irq, if pending and enable timer*/
	io_reg_write(TIMER_CON(timer->id), TIMER_IRQ_CLEAR | TIMER_ENABLE_BIT);
}

void platform_timer_stop(struct timer *timer)
{
	if (timer->id >= NR_TMRS)
		return;

	io_reg_write(TIMER_CON(timer->id), TIMER_IRQ_CLEAR | TIMER_ENABLE_BIT);
	io_reg_write(TIMER_CON(timer->id), 0);

}

/* IRQs off in arch_timer_get_system() */
uint64_t platform_timer_get_atomic(struct timer *timer)
{
	return platform_timer_get(timer);
}

int64_t platform_timer_set(struct timer *timer, uint64_t ticks)
{
	uint64_t time;
	uint32_t flags;
	uint32_t ticks_set;

	if (timer->id >= NR_TMRS)
		return -EINVAL;

	flags = arch_interrupt_global_disable();

	time = platform_timer_get(timer);
	ticks_set = (ticks > time) ? ticks - time : UINT64_MAX - time +  ticks;
	timer->hitimeout = ticks >> 32;
	timer->lowtimeout = ticks_set;

	io_reg_write(TIMER_CNT_VAL(timer->id), ticks_set);
	io_reg_write(TIMER_CON(timer->id), TIMER_IRQ_ENABLE | TIMER_ENABLE_BIT);

	arch_interrupt_global_enable(flags);

	return ticks;
}

void platform_timer_clear(struct timer *timer)
{
	if (timer->id >= NR_TMRS)
		return;

	io_reg_write(TIMER_CON(timer->id), TIMER_IRQ_CLEAR | TIMER_ENABLE_BIT);
}

uint64_t platform_timer_get(struct timer *timer)
{
	uint64_t time;
	uint32_t low, high;

	if (timer->id >= NR_TMRS)
		return -EINVAL;

	/* must read 'low' first, and 'high' next */
	low = io_reg_read(CNTCV_L);
	high = io_reg_read(CNTCV_H);
	time = ((uint64_t)high << 32) | low;

	return time;
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

	posn->wallclock = timer_get_system(timer_get()) - posn->wallclock;
	posn->flags |= SOF_TIME_WALL_VALID | SOF_TIME_WALL_64;
}

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	*wallclock = platform_timer_get(timer_get());
}

static void platform_timer_handler(void *arg)
{
	struct timer *timer = arg;

	timer->handler(timer->data);
}

static int platform_timer_register(struct timer *timer, void (*handler)(void *arg), void *arg)
{
	timer->handler = handler;
	timer->data = arg;
	timer->hitime = 0;
	timer->hitimeout = 0;

	return interrupt_register(timer->irq, platform_timer_handler, timer);
}

int timer_register(struct timer *timer, void(*handler)(void *arg), void *arg)
{
	switch (timer->id) {
	case OSTIMER0:
		return platform_timer_register(timer, handler, arg);
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
	interrupt_unmask(timer->irq, cpu_get_id());
	interrupt_enable(timer->irq, arg);
}

void timer_disable(struct timer *timer, void *arg, int core)
{
	interrupt_disable(timer->irq, arg);
	interrupt_mask(timer->irq, cpu_get_id());
}
