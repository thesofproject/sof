// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author:Fengquan Chen <fengquan.chen@mediatek.com>
//        Allen-KH Cheng <allen-kh.cheng@mediatek.com>

#include <sof/audio/component_ext.h>
#include <rtos/interrupt.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <platform/drivers/timer.h>
#include <platform/drivers/interrupt.h>
#include <ipc/stream.h>
#include <errno.h>
#include <stdint.h>

void platform_timer_start(struct timer *timer)
{
	/*set 13M clksrc*/
	io_reg_update_bits(CNTCR, CLKSRC_BIT, 0x0);
	io_reg_update_bits(CNTCR, CLKSRC_13M_BIT, CLKSRC_13M_BIT);

	/*enable timer*/
	io_reg_update_bits(CNTCR, CNT_EN_BIT, CNT_EN_BIT);
}

void platform_timer_stop(struct timer *timer)
{
	if (timer->id > NR_TMRS)
		return;

	io_reg_update_bits(TIMER_CON(timer->id), TIMER_ENABLE_BIT, 0x0);
	io_reg_update_bits(TIMER_IRQ_ACK(timer->id), TIMER_IRQ_ENABLE, 0x0);
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
	uint32_t low, high;
	uint32_t ticks_set;

	if (timer->id > NR_TMRS)
		return -EINVAL;

	flags = arch_interrupt_global_disable();

	low = io_reg_read(OSTIMER_CUR_L);
	high = io_reg_read(OSTIMER_CUR_H);

	/*ostimer 13M counter to 26M interrupt */
	time = (((uint64_t)high << 32) | low) << 1;
	ticks_set = (ticks > time) ? ticks - time : UINT64_MAX - time +  ticks;
	timer->hitimeout = ticks >> 32;
	timer->lowtimeout = ticks_set;

	/*selsect 26M clksrc*/
	io_reg_update_bits(TIMER_CON(timer->id), TIMER_CLKSRC_BIT, 0x0);
	io_reg_update_bits(TIMER_CON(timer->id),
			   TIMER_CLK_SRC_CLK_26M << TIMER_CLK_SRC_SHIFT,
			   TIMER_CLK_SRC_CLK_26M << TIMER_CLK_SRC_SHIFT);

	io_reg_write(TIMER_RST_VAL(timer->id), ticks_set);
	io_reg_update_bits(TIMER_IRQ_ACK(timer->id), TIMER_IRQ_CLEAR, TIMER_IRQ_CLEAR);
	io_reg_update_bits(TIMER_IRQ_ACK(timer->id), TIMER_IRQ_ENABLE, TIMER_IRQ_ENABLE);
	io_reg_update_bits(TIMER_CON(timer->id), TIMER_ENABLE_BIT, TIMER_ENABLE_BIT);

	arch_interrupt_global_enable(flags);

	return ticks;
}

void platform_timer_clear(struct timer *timer)
{
	if (timer->id > NR_TMRS)
		return;

	io_reg_update_bits(TIMER_IRQ_ACK(timer->id), TIMER_IRQ_CLEAR, TIMER_IRQ_CLEAR);
}

uint64_t platform_timer_get(struct timer *timer)
{
	uint64_t time;
	uint32_t low;
	uint32_t high0;
	uint32_t high1;

	if (timer->id > NR_TMRS)
		return -EINVAL;

	/* 64bit reads are non atomic on xtensa so we must
	 * read a stable value where there is no bit 32 flipping.
	 */
	do {
		high0 = io_reg_read(OSTIMER_CUR_H);
		low = io_reg_read(OSTIMER_CUR_L);
		high1 = io_reg_read(OSTIMER_CUR_H);

		/* worst case is we perform this twice so 6 * 32b clock reads */
	} while (high0 != high1);

	/*ostimer 13M counter to 26M interrupt */
	time = (((uint64_t)high0 << 32) | low) << 1;

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

	io_reg_update_bits(TIMER_IRQ_ACK(timer->id), TIMER_IRQ_CLEAR, TIMER_IRQ_CLEAR);
	io_reg_update_bits(TIMER_CON(timer->id), TIMER_ENABLE_BIT, 0x0);
	io_reg_update_bits(TIMER_IRQ_ACK(timer->id), TIMER_IRQ_ENABLE, 0x0);
	timer->handler(timer->data);
}

static int platform_timer_register(struct timer *timer, void (*handler)(void *arg), void *arg)
{
	uint32_t timer_irq0;
	int err;

	timer->handler = handler;
	timer->data = arg;
	timer->hitime = 0;
	timer->hitimeout = 0;

	timer_irq0 = mtk_get_irq_domain_id(timer->irq);
	err = interrupt_register(timer_irq0, platform_timer_handler, timer);
	if (err < 0)
		return err;

	/* enable timer interrupt */
	interrupt_enable(timer_irq0, timer);

	return err;
}

int timer_register(struct timer *timer, void(*handler)(void *arg), void *arg)
{
	switch (timer->id) {
	case OSTIMER0:
	case OSTIMER1:
	case OSTIMER2:
	case OSTIMER3:
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
	interrupt_enable(timer->irq, arg);
}

void timer_disable(struct timer *timer, void *arg, int core)
{
	interrupt_disable(timer->irq, arg);
}
