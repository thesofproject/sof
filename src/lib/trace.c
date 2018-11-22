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
#include <sof/preproc.h>
#include <stdint.h>

struct trace {
	uint32_t pos ;	/* trace position */
	uint32_t enable;
	spinlock_t lock;
};

static struct trace *trace;

/* calculates total message size, both header and payload in bytes */
#define MESSAGE_SIZE(args_num)	\
	(sizeof(struct log_entry_header) + args_num * sizeof(uint32_t))

/* calculated total message size in dwords */
#define MESSAGE_SIZE_DWORDS(args_num)	\
	(MESSAGE_SIZE(args_num) / sizeof(uint32_t))

/* calculates offset to param_idx of payload */
#define PAYLOAD_OFFSET(param_idx) \
	(MESSAGE_SIZE_DWORDS(0) + param_idx)

#define TRACE_ID_MASK ((1 << TRACE_ID_LENGTH) - 1)

static void put_header(uint32_t *dst, uint32_t id_0, uint32_t id_1,
		       uint32_t entry, uint64_t timestamp)
{
	struct log_entry_header header;

	header.id_0 = id_0 & TRACE_ID_MASK;
	header.id_1 = id_1 & TRACE_ID_MASK;
	header.core_id = cpu_get_id();
	header.timestamp = timestamp;
	header.log_entry_address = entry;

	memcpy(dst, &header, sizeof(header));
}

static void mtrace_event(const char *data, uint32_t length)
{
	volatile char *t;
	uint32_t i, available;

	available = MAILBOX_TRACE_SIZE - trace->pos;

	t = (volatile char *)(MAILBOX_TRACE_BASE);

	/* write until we run out of space */
	for (i = 0; i < available && i < length; i++)
		t[trace->pos + i] = data[i];

	dcache_writeback_region((void *)&t[trace->pos], i);
	trace->pos += length;

	/* if there was more data than space available, wrap back */
	if (length > available) {
		for (i = 0; i < length - available; i++)
			t[i] = data[available + i];

		dcache_writeback_region((void *)t, i);
		trace->pos = i;
	}
}

#define _TRACE_EVENT_NTH_IMPL_PAYLOAD_STEP(i, _)	\
	dt[PAYLOAD_OFFSET(i)] = META_CONCAT(param, i);

#define _TRACE_EVENT_NTH_PAYLOAD_IMPL(arg_count)			\
	META_SEQ_FROM_0_TO(arg_count, _TRACE_EVENT_NTH_IMPL_PAYLOAD_STEP)

 /* _trace_event function with poor people's version of constexpr if */
#define _TRACE_EVENT_NTH_IMPL(is_mbox, is_atomic, arg_count)		\
_TRACE_EVENT_NTH(META_CONCAT(						\
/*if is_mbox  , append _mbox   to function name*/			\
META_IF_ELSE(is_mbox)(_mbox)(),						\
/*if is_atomic, append _atomic to function name*/			\
META_IF_ELSE(is_atomic)(_atomic)()					\
), arg_count)								\
{									\
	uint32_t dt[MESSAGE_SIZE_DWORDS(arg_count)];			\
	META_IF_ELSE(is_mbox)						\
	(								\
		META_IF_ELSE(is_atomic)()(unsigned long flags;)		\
	)()								\
									\
	if (!trace->enable)						\
		return;							\
									\
	put_header(dt, id_0, id_1, log_entry,				\
		   platform_timer_get(platform_timer));			\
									\
	_TRACE_EVENT_NTH_PAYLOAD_IMPL(arg_count)			\
	META_IF_ELSE(is_atomic)						\
		(dtrace_event_atomic)					\
		(dtrace_event)						\
			((const char *)dt, MESSAGE_SIZE(arg_count));	\
	/* send event by mail box too. */				\
	META_IF_ELSE(is_mbox)						\
	(								\
		META_IF_ELSE(is_atomic)()(				\
			spin_lock_irq(&trace->lock, flags);		\
		)							\
		mtrace_event((const char *)dt, MESSAGE_SIZE(arg_count));\
									\
		META_IF_ELSE(is_atomic)()(spin_unlock_irq(&trace->lock,	\
							  flags);)	\
	)()								\
}

#define _TRACE_EVENT_NTH_IMPL_GROUP(arg_count)				\
	/* send trace events only to the local trace buffer */		\
	_TRACE_EVENT_NTH_IMPL(0, 0, arg_count)				\
	_TRACE_EVENT_NTH_IMPL(0, 1, arg_count)				\
	/* send trace events to the local trace buffer and mailbox */	\
	_TRACE_EVENT_NTH_IMPL(1, 0, arg_count)				\
	_TRACE_EVENT_NTH_IMPL(1, 1, arg_count)


/* Implementation of
 * void _trace_event1(            uintptr_t log_entry, uint32_t ids...) {...}
 * void _trace_event_mbox1(       uintptr_t log_entry, uint32_t ids...) {...}
 * void _trace_event_atomic1(     uintptr_t log_entry, uint32_t ids...) {...}
 * void _trace_event_mbox_atomic1(uintptr_t log_entry, uint32_t ids...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(0)

/* Implementation of
 * void _trace_event1(            uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox1(       uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_atomic1(     uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox_atomic1(uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(1)

/* Implementation of
 * void _trace_event2(            uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox2(       uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_atomic2(     uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox_atomic2(uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(2)

/* Implementation of
 * void _trace_event3(            uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox3(       uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_atomic3(     uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox_atomic3(uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(3)

/* Implementation of
 * void _trace_event4(            uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox4(       uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_atomic4(     uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 * void _trace_event_mbox_atomic4(uintptr_t log_entry, uint32_t ids...,
 *                                uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(4)

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
