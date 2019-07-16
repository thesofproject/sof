/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_TIMER_H__

#ifndef __PLATFORM_DRIVERS_TIMER_H__
#define __PLATFORM_DRIVERS_TIMER_H__

#include <sof/drivers/interrupt.h>

#define TIMER_COUNT	4

/* timer numbers must use associated IRQ number */
#define TIMER0		IRQ_NUM_TIMER1
#define TIMER1		IRQ_NUM_TIMER2
#define TIMER2		IRQ_NUM_TIMER3
#define TIMER3		IRQ_NUM_EXT_TIMER

#endif /* __PLATFORM_DRIVERS_TIMER_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/timer.h"

#endif /* __SOF_DRIVERS_TIMER_H__ */
