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
#include <reef/audio/dma-trace.h>
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

	if (!trace.enable)
		return;

	/* save event to DMA tracing buffer */
	_trace_event(event);

	/* send event by mail box too. */
	spin_lock_irq(&trace.lock, flags);

	/* write timestamp and event to trace buffer */
	t =(volatile uint64_t*)(MAILBOX_TRACE_BASE + trace.pos);
	t[0] = platform_timer_get(platform_timer);
	t[1] = event;

	/* writeback trace data */
	dcache_writeback_region((void*)t, sizeof(uint64_t) * 2);

	trace.pos += (sizeof(uint64_t) << 1);

	if (trace.pos >= MAILBOX_TRACE_SIZE)
		trace.pos = 0;

	spin_unlock_irq(&trace.lock, flags);
}

void _trace_event(uint32_t event)
{
	unsigned long flags;
	volatile uint64_t dt[2];
	uint32_t et = (event & 0xff000000);

	if (!trace.enable)
		return;

	if (et == TRACE_CLASS_DMA)
		return;

	spin_lock_irq(&trace.lock, flags);

	dt[0] = platform_timer_get(platform_timer);
	dt[1] = event;
	dtrace_event((const char*)dt, sizeof(uint64_t) * 2);

	spin_unlock_irq(&trace.lock, flags);
}

void trace_off(void)
{
	trace.enable = 0;
};

void trace_init(struct reef *reef)
{
	trace.enable = 1;
	trace.pos = 0;
	spinlock_init(&trace.lock);
}
