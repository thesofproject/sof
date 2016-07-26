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

void platform_timer_start(struct timer *timer)
{
}

/* this seems to stop rebooting with RTD3 ???? */
void platform_timer_stop(struct timer *timer)
{
}

void platform_timer_set(struct timer *timer, uint32_t ticks)
{
}

void platform_timer_clear(struct timer *timer)
{
}

uint32_t platform_timer_get(struct timer *timer)
{
	return 0;
}
