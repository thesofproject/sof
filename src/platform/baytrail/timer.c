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
#include <stdint.h>

void platform_timer_set(int timer, uint32_t ticks)
{
	/* clear the clear bit */
	//shim_write(SHIM_EXT_TIMER_CNTLH, 0);

	/* set new value and run */
	shim_write(SHIM_EXT_TIMER_CNTLL, ticks);
	shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_RUN);
}

void platform_timer_clear(int timer)
{
	uint32_t pisr;

	//shim_write(SHIM_EXT_TIMER_CNTLL, 0);
	//shim_write(SHIM_EXT_TIMER_CNTLH, SHIM_EXT_TIMER_CLEAR);

	pisr = shim_read(SHIM_PISR);
	pisr |= SHIM_PISR_EXT_TIMER;
	shim_write(SHIM_PISR, pisr);
}

uint32_t platform_timer_get(int timer)
{
	return shim_read(SHIM_EXT_TIMER_STAT);
}
