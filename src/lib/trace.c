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
 *         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#include <sof/trace.h>
#include <sof/sof.h>
#include <sof/alloc.h>
#include <arch/cache.h>
#include <platform/timer.h>
#include <sof/lock.h>
#include <sof/dma-trace.h>
#include <sof/cpu.h>
#include <stdint.h>

struct trace {
	uint32_t pos ;	/* trace position */
	uint32_t enable;
	spinlock_t lock;
};

static struct trace *trace;

/* calculates total message size, both header and payload */
#define MESSAGE_SIZE(args_num) \
	((sizeof(struct log_entry_header) + (1 + args_num) * sizeof(uint32_t)) \
	/ sizeof(uint32_t))

static void put_header(volatile uint32_t *dst, uint64_t timestamp)
{
	struct log_entry_header header;

	header.rsvd = 0;
	header.core_id = cpu_get_id();
	header.timestamp = timestamp;

	int i = 0;

	for (; i < sizeof(struct log_entry_header) / sizeof(uint32_t); i++) {
		uint32_t *header_ptr = (uint32_t *)&header;

		dst[i] = header_ptr[i];
	}
}

/* send trace events only to the local trace buffer */
void _trace_event0(uint32_t log_entry)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(0);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(0)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_atomic0(uint32_t log_entry)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(0);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(0)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events to the local trace buffer and the mailbox */
void _trace_event_mbox0(uint32_t log_entry)
{
	unsigned long flags;
	uint32_t message_size_dwords = MESSAGE_SIZE(0);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(0)];
	uint64_t time;

	volatile uint32_t *t;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);

	dt[payload_offset] = log_entry;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);

	/* send event by mail box too. */
	spin_lock_irq(&trace->lock, flags);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	spin_unlock_irq(&trace->lock, flags);

	put_header(t, time);
	t[payload_offset] = log_entry;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_mbox_atomic0(uint32_t log_entry)
{
	volatile uint32_t *t;
	uint32_t message_size_dwords = MESSAGE_SIZE(0);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(0)];
	uint64_t time;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);
	dt[payload_offset] = log_entry;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	put_header(t, time);
	t[payload_offset] = log_entry;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events only to the local trace buffer */
void _trace_event1(uint32_t log_entry, uint32_t param)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(1);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(1)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_atomic1(uint32_t log_entry, uint32_t param)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(1);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(1)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events to the local trace buffer and the mailbox */
void _trace_event_mbox1(uint32_t log_entry, uint32_t param)
{
	unsigned long flags;
	uint32_t message_size_dwords = MESSAGE_SIZE(1);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(1)];
	uint64_t time;

	volatile uint32_t *t;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);

	/* send event by mail box too. */
	spin_lock_irq(&trace->lock, flags);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	spin_unlock_irq(&trace->lock, flags);

	put_header(t, time);
	t[payload_offset] = log_entry;
	t[payload_offset + 1] = param;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_mbox_atomic1(uint32_t log_entry, uint32_t param)
{
	volatile uint32_t *t;
	uint32_t message_size_dwords = MESSAGE_SIZE(1);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(1)];
	uint64_t time;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);
	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	put_header(t, time);
	t[payload_offset] = log_entry;
	t[payload_offset + 1] = param;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events only to the local trace buffer */
void _trace_event2(uint32_t log_entry, uint32_t param1, uint32_t param2)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(2);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(2)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_atomic2(uint32_t log_entry, uint32_t param1, uint32_t param2)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(2);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(2)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events to the local trace buffer and the mailbox */
void _trace_event_mbox2(uint32_t log_entry, uint32_t param1, uint32_t param2)
{
	unsigned long flags;
	uint32_t message_size_dwords = MESSAGE_SIZE(2);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(2)];
	uint64_t time;

	volatile uint32_t *t;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);

	/* send event by mail box too. */
	spin_lock_irq(&trace->lock, flags);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	spin_unlock_irq(&trace->lock, flags);

	put_header(t, time);
	t[payload_offset] = log_entry;
	t[payload_offset + 1] = param1;
	t[payload_offset + 2] = param2;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_mbox_atomic2(uint32_t log_entry, uint32_t param1,
	uint32_t param2)
{
	volatile uint32_t *t;
	uint32_t message_size_dwords = MESSAGE_SIZE(2);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(2)];
	uint64_t time;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);
	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	put_header(t, time);
	t[payload_offset] = log_entry;
	t[payload_offset + 1] = param1;
	t[payload_offset + 2] = param2;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events only to the local trace buffer */
void _trace_event3(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(3);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(3)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dt[payload_offset + 3] = param3;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_atomic3(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3)
{
	uint32_t message_size_dwords = MESSAGE_SIZE(3);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(3)];

	if (!trace->enable)
		return;

	put_header(dt, platform_timer_get(platform_timer));

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dt[payload_offset + 3] = param3;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);
}

/* send trace events to the local trace buffer and the mailbox */
void _trace_event_mbox3(uint32_t log_entry, uint32_t param1, uint32_t param2,
	uint32_t param3)
{
	unsigned long flags;
	uint32_t message_size_dwords = MESSAGE_SIZE(3);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(3)];
	uint64_t time;

	volatile uint32_t *t;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);

	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dt[payload_offset + 3] = param3;
	dtrace_event((const char *)dt, sizeof(uint32_t) * message_size_dwords);

	/* send event by mail box too. */
	spin_lock_irq(&trace->lock, flags);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	spin_unlock_irq(&trace->lock, flags);

	put_header(t, time);
	t[payload_offset] = log_entry;
	t[payload_offset + 1] = param1;
	t[payload_offset + 2] = param2;
	t[payload_offset + 3] = param3;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

void _trace_event_mbox_atomic3(uint32_t log_entry, uint32_t param1,
	uint32_t param2, uint32_t param3)
{
	volatile uint32_t *t;
	uint32_t message_size_dwords = MESSAGE_SIZE(3);
	uint32_t payload_offset = sizeof(struct log_entry_header)
		/ sizeof(uint32_t);
	uint32_t dt[MESSAGE_SIZE(3)];
	uint64_t time;

	if (!trace->enable)
		return;

	time = platform_timer_get(platform_timer);

	put_header(dt, time);
	dt[payload_offset] = log_entry;
	dt[payload_offset + 1] = param1;
	dt[payload_offset + 2] = param2;
	dt[payload_offset + 3] = param3;
	dtrace_event_atomic((const char *)dt,
		sizeof(uint32_t) * message_size_dwords);

	/* write timestamp and event to trace buffer */
	t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);
	trace->pos += sizeof(uint32_t) * message_size_dwords;

	if (trace->pos > MAILBOX_TRACE_SIZE
		- sizeof(uint32_t) * message_size_dwords)
		trace->pos = 0;

	put_header(t, time);
	t[payload_offset] = log_entry;
	t[payload_offset + 1] = param1;
	t[payload_offset + 2] = param2;
	t[payload_offset + 3] = param3;

	/* writeback trace data */
	dcache_writeback_region((void *)t,
		sizeof(uint32_t) * message_size_dwords);
}

void trace_flush(void)
{
	volatile uint64_t *t;

	/* get mailbox position */
	t = (volatile uint64_t *)(MAILBOX_TRACE_BASE + trace->pos);

	/* flush dma trace messages */
	dma_trace_flush((void *)t);
}

void trace_off(void)
{
	trace->enable = 0;
}

void trace_init(struct sof *sof)
{
	dma_trace_init_early(sof);

	trace = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
			sizeof(*trace));
	trace->enable = 1;
	trace->pos = 0;
	spinlock_init(&trace->lock);

	bzero((void *)MAILBOX_TRACE_BASE, MAILBOX_TRACE_SIZE);
	dcache_writeback_invalidate_region((void *)MAILBOX_TRACE_BASE,
					   MAILBOX_TRACE_SIZE);
}
