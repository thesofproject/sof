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

void platform_timer_start(int timer)
{
	/* run timer */
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_RUN);
	shim_write(SHIM_EXT_TIMER_CNTLL, 0);
}

/* this seems to stop rebooting with RTD3 ???? */
void platform_timer_stop(int timer)
{
	/* run timer */
	shim_write(SHIM_EXT_TIMER_CNTLL, 0);
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_CLEAR);
}

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
	/* we dont use the timer clear bit as we only need to clear the ISR */
	shim_write(SHIM_PISR, SHIM_PISR_EXT_TIMER);
}

uint32_t platform_timer_get(int timer)
{
	return shim_read(SHIM_EXT_TIMER_STAT);
}
