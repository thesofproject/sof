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
#include <platform/timer.h>

/* trace event classes - high 8 bits*/
#define TRACE_CLASS_IRQ		(1 << 24)
#define TRACE_CLASS_IPC		(2 << 24)
#define TRACE_CLASS_PIPE	(3 << 24)
#define TRACE_CLASS_HOST	(4 << 24)
#define TRACE_CLASS_DAI		(5 << 24)
#define TRACE_CLASS_DMA		(6 << 24)
#define TRACE_CLASS_SSP		(7 << 24)
#define TRACE_CLASS_VOLUME	(8 << 24)

/* move to config.h */
#define TRACE	1
#define TRACEV	0
#define TRACEE	1

#if TRACE
extern uint32_t trace_pos;

static inline void _trace_event(uint32_t event)
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

#define trace_event(__c, __e) \
	_trace_event(__c | (__e[0] << 16) | (__e[1] <<8) | __e[2])

#define trace_value(x)	_trace_event(x)

#define trace_point(x) platform_trace_point(x)

/* verbose tracing */
#if TRACEV
#define tracev_event(__c, __e) trace_event(__c, __e)
#else
#define tracev_event(__c, __e)
#endif

/* error tracing */
#if TRACEE
#define trace_error(__c, __e) trace_event(__c, __e)
#else
#define trace_error(__c, __e)
#endif

#else

#define trace_event(x)
#define trace_value(x)
#define trace_point(x)

#endif

#endif

