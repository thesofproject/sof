/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#include <reef/trace.h>
#include <stdint.h>

/* trace position */
static uint32_t trace_pos = 0;

void _trace_event(uint32_t event)
{
	volatile uint32_t *t =
		(volatile uint32_t*)(MAILBOX_TRACE_BASE + trace_pos);

	/* write timestamp and event to trace buffer */
	t[0] = platform_timer_get(0);
	t[1] = event;

	trace_pos += (sizeof(uint32_t) << 1);
	if (trace_pos >= MAILBOX_TRACE_SIZE)
		trace_pos = 0;
}

