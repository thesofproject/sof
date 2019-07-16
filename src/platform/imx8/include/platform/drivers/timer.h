/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_DRIVERS_TIMER_H__

#ifndef __PLATFORM_DRIVERS_TIMER_H__
#define __PLATFORM_DRIVERS_TIMER_H__

#include <sof/drivers/interrupt.h>

#define TIMER_COUNT	2

/* timer numbers must use associated IRQ number */
#define TIMER0		IRQ_NUM_TIMER0
#define TIMER1		IRQ_NUM_TIMER1

#endif /* __PLATFORM_DRIVERS_TIMER_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/timer.h"

#endif /* __SOF_DRIVERS_TIMER_H__ */
