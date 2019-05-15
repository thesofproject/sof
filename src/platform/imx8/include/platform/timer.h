/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __PLATFORM_TIMER_H__
#define __PLATFORM_TIMER_H__

#include <stdint.h>
#include <sof/timer.h>
#include <platform/interrupt.h>

#define TIMER_COUNT	2

/* timer numbers must use associated IRQ number */
#define TIMER0		IRQ_NUM_TIMER0
#define TIMER1		IRQ_NUM_TIMER1

#endif
