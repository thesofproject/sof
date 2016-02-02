/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Baytrail external timer control.
 */

#include <platform/timer.h>
#include <platform/shim.h>
#include <reef/debug.h>
#include <stdint.h>

void platform_timer_set(int timer, uint32_t ticks)
{
	/* a tick value of 0 will not generate an IRQ */
	if (ticks == 0)
		ticks = 1;

	/* set new value and run */
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_RUN);
	shim_write(SHIM_EXT_TIMER_CNTLL, ticks);
}

void platform_timer_clear(int timer)
{
	uint32_t pisr;

	/* we dont use the timer clear bit as we only need to clear the ISR */
	pisr = shim_read(SHIM_PISR);
	pisr |= SHIM_PISR_EXT_TIMER;
	shim_write(SHIM_PISR, pisr);
}

uint32_t platform_timer_get(int timer)
{
	return shim_read(SHIM_EXT_TIMER_STAT);
}
