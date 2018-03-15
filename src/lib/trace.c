/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <reef/trace.h>
#include <reef/reef.h>
#include <reef/alloc.h>
#include <arch/cache.h>
#include <platform/timer.h>
#include <reef/lock.h>
#include <reef/dma-trace.h>
#include <stdint.h>

struct trace {
	uint32_t pos ;	/* trace position */
	uint32_t enable;
	spinlock_t lock;
};

static struct trace trace;

void _trace_error(uint32_t event)
{
	unsigned long flags;
	volatile uint64_t *t;
	uint64_t dt[2];
	uint64_t time;

	if (!trace.enable)
		return;

	time = platform_timer_get(platform_timer);

	/* save event to DMA tracing buffer */
	dt[0] = time;
	dt[1] = event;
	dtrace_event((const char*)dt, sizeof(uint64_t) * 2);

	/* send event by mail box too. */
	spin_lock_irq(&trace.lock, flags);

	/* write timestamp and event to trace buffer */
	t = (volatile uint64_t*)(MAILBOX_TRACE_BASE + trace.pos);
	trace.pos += (sizeof(uint64_t) << 1);

	if (trace.pos > MAILBOX_TRACE_SIZE - sizeof(uint64_t) * 2)
		trace.pos = 0;

	spin_unlock_irq(&trace.lock, flags);

	t[0] = time;
	t[1] = event;

	/* writeback trace data */
	dcache_writeback_region((void*)t, sizeof(uint64_t) * 2);
}

void _trace_error_atomic(uint32_t event)
{
	volatile uint64_t *t;
	uint64_t dt[2];

	uint64_t time;

	if (!trace.enable)
		return;

	time = platform_timer_get(platform_timer);

	/* save event to DMA tracing buffer */
	dt[0] = time;
	dt[1] = event;
	dtrace_event_atomic((const char*)dt, sizeof(uint64_t) * 2);

	/* write timestamp and event to trace buffer */
	t = (volatile uint64_t*)(MAILBOX_TRACE_BASE + trace.pos);
	trace.pos += (sizeof(uint64_t) << 1);

	if (trace.pos > MAILBOX_TRACE_SIZE - sizeof(uint64_t) * 2)
		trace.pos = 0;

	t[0] = time;
	t[1] = event;

	/* writeback trace data */
	dcache_writeback_region((void*)t, sizeof(uint64_t) * 2);
}

void _trace_event(uint32_t event)
{
	uint64_t dt[2];

	if (!trace.enable)
		return;

	dt[0] = platform_timer_get(platform_timer);
	dt[1] = event;
	dtrace_event((const char*)dt, sizeof(uint64_t) * 2);
}

void _trace_event_atomic(uint32_t event)
{
	uint64_t dt[2];

	if (!trace.enable)
		return;

	dt[0] = platform_timer_get(platform_timer);
	dt[1] = event;
	dtrace_event_atomic((const char*)dt, sizeof(uint64_t) * 2);
}

void trace_off(void)
{
	trace.enable = 0;
}

void trace_init(struct reef *reef)
{
	dma_trace_init_early(reef);
	trace.enable = 1;
	trace.pos = 0;
	spinlock_init(&trace.lock);
}
