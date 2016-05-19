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

#include <xtensa/xtruntime.h>
#include <xtensa/hal.h>
#include <arch/timer.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/timer.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <stdint.h>
#include <errno.h>

void arch_timer_set(struct timer *timer, unsigned int ticks)
{
	switch (timer->id) {
	case TIMER0:
		xthal_set_ccompare(0, ticks);
		break;
	case TIMER1:
		xthal_set_ccompare(1, ticks);
		break;
	case TIMER2:
		xthal_set_ccompare(2, ticks);
		break;
	default:
		break;
	}
}

