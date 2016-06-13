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


#ifndef __PLATFORM_TIMER_H__
#define __PLATFORM_TIMER_H__

#include <stdint.h>
#include <reef/timer.h>
#include <platform/interrupt.h>

#define TIMER_COUNT	4

/* timer numbers must use associated IRQ number */
#define TIMER0		IRQ_NUM_TIMER1
#define TIMER1		IRQ_NUM_TIMER2
#define TIMER2		IRQ_NUM_TIMER3
#define TIMER3		IRQ_NUM_EXT_TIMER

#define TIMER_AUDIO	TIMER3

void platform_timer_set(struct timer *timer, uint32_t ticks);
void platform_timer_clear(struct timer *timer);
uint32_t platform_timer_get(struct timer *timer);
void platform_timer_start(struct timer *timer);
void platform_timer_stop(struct timer *timer);

#endif

