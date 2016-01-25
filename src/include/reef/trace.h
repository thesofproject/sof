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
#include <platform/platform.h>

/* trace event classes - high 8 bits*/
#define TRACE_CLASS_IRQ		(1 << 24)
#define TRACE_CLASS_IPC		(2 << 24)
#define TRACE_CLASS_PIPE	(3 << 24)

/* move to config.h */
#define TRACE	1

#if TRACE
extern uint32_t trace_pos;

static inline void _trace_event(uint32_t event)
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

#define trace_event(__c, __e) \
	_trace_event(__c | (__e[0] << 16) | (__e[1] <<8) | __e[2])

#define trace_value(x)	_trace_event(x)

#define trace_point(x) platform_trace_point(x)

#else

#define trace_event(x)
#define trace_value(x)
#define trace_point(x)

#endif

#endif

