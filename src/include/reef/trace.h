/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_TRACE__
#define __INCLUDE_TRACE__

#include <stdint.h>
#include <stdlib.h>
#include <reef/mailbox.h>
#include <reef/debug.h>
#include <reef/timer.h>

/* trace event classes - high 24 bits*/
#define TRACE_CLASS_IRQ		(1 << 8)
#define TRACE_CLASS_IPC		(1 << 9)

/* move to config.h */
#define TRACE	1

#if TRACE
extern uint32_t trace_pos;

static inline void trace_event(uint32_t event)
{
	volatile uint32_t *t =
		(volatile uint32_t*)(MAILBOX_TRACE_BASE + trace_pos);

	/* write timestamp and event to trace buffer */
	t[0] = timer_get_system();
	t[1] = event;

	trace_pos += (sizeof(uint32_t) * 2);
	if (trace_pos >= MAILBOX_TRACE_SIZE)
		trace_pos = 0;
}

#else

#define trace_event(x)

#endif

#endif

