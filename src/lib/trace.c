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

/* TODO: it should be possible to do this for arbitrary array, passed as an arg
 * something like:
 * #define FOO(array, i, _) array[payload_offset + 1 + i] =\
 *     META_CONCAT(param, i);
 * #define BAR(array, arg_count) META_SEQ_FROM_0_TO(arg_count, FOO(array))
 */
#define _TRACE_EVENT_NTH_IMPL_DT_STEP(i, _)\
	dt[payload_offset + 1 + i] = META_CONCAT(param, i);
#define _TRACE_EVENT_NTH_IMPL_T_STEP(i, _)\
	t[payload_offset + 1 + i] = META_CONCAT(param, i);
#define _TRACE_EVENT_NTH_IMPL_DT(arg_count)\
	META_SEQ_FROM_0_TO(arg_count, _TRACE_EVENT_NTH_IMPL_DT_STEP)
#define _TRACE_EVENT_NTH_IMPL_T(arg_count)\
	META_SEQ_FROM_0_TO(arg_count, _TRACE_EVENT_NTH_IMPL_T_STEP)

/* _trace_event function with poor people's version of constexpr if */
#define _TRACE_EVENT_NTH_IMPL(is_mbox, is_atomic, arg_count)\
_TRACE_EVENT_NTH(META_CONCAT(\
/*if is_mbox  , append _mbox   to function name*/\
META_IF_ELSE(is_mbox)(_mbox)(),\
/*if is_atomic, append _atomic to function name*/\
META_IF_ELSE(is_atomic)(_atomic)()\
), arg_count)\
{\
	const uint32_t msg_size_dwords = MESSAGE_SIZE(arg_count);\
	const uint32_t payload_offset = sizeof(struct log_entry_header)\
		/ sizeof(uint32_t);\
	uint32_t dt[MESSAGE_SIZE(arg_count)];\
	META_IF_ELSE(is_mbox)\
	(\
		volatile uint32_t *t;\
		META_IF_ELSE(is_atomic)()(unsigned long flags;)\
	)()\
\
	if (!trace->enable)\
		return;\
\
	META_IF_ELSE(is_mbox)\
	(\
		uint64_t time = platform_timer_get(platform_timer);\
		put_header(dt, time);\
	)\
	(\
		put_header(dt, platform_timer_get(platform_timer));\
	)\
\
	dt[payload_offset] = log_entry;\
	_TRACE_EVENT_NTH_IMPL_DT(arg_count)\
	META_IF_ELSE(is_atomic)\
		(dtrace_event_atomic)\
		(dtrace_event)\
			((const char *)dt, sizeof(uint32_t) * msg_size_dwords);\
	META_IF_ELSE(is_mbox)\
	(\
		META_IF_ELSE(is_atomic)()(\
			/* send event by mail box too. */\
			spin_lock_irq(&trace->lock, flags);\
		)\
		/* write timestamp and event to trace buffer */\
		t = (volatile uint32_t *)(MAILBOX_TRACE_BASE + trace->pos);\
		trace->pos += sizeof(uint32_t) * msg_size_dwords;\
\
		if (trace->pos > MAILBOX_TRACE_SIZE\
			- sizeof(uint32_t) * msg_size_dwords)\
		trace->pos = 0;\
\
		META_IF_ELSE(is_atomic)()(spin_unlock_irq(&trace->lock, flags);)\
\
		put_header(t, time);\
		t[payload_offset] = log_entry;\
		_TRACE_EVENT_NTH_IMPL_T(arg_count)\
\
		/* writeback trace data */\
		dcache_writeback_region\
			((void *)t, sizeof(uint32_t) * msg_size_dwords);\
	)()\
}

#define _TRACE_EVENT_NTH_IMPL_GROUP(arg_count)\
	/* send trace events only to the local trace buffer */\
	_TRACE_EVENT_NTH_IMPL(0, 0, arg_count)\
	_TRACE_EVENT_NTH_IMPL(0, 1, arg_count)\
	/* send trace events to the local trace buffer and the mailbox */\
	_TRACE_EVENT_NTH_IMPL(1, 0, arg_count)\
	_TRACE_EVENT_NTH_IMPL(1, 1, arg_count)


/* Implementation of
 * void _trace_event1(            uint32_t log_entry) {...}
 * void _trace_event_mbox1(       uint32_t log_entry) {...}
 * void _trace_event_atomic1(     uint32_t log_entry) {...}
 * void _trace_event_mbox_atomic1(uint32_t log_entry) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(0)

/* Implementation of
 * void _trace_event1(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox1(       uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_atomic1(     uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic1(uint32_t log_entry, uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(1)

/* Implementation of
 * void _trace_event2(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox2(       uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_atomic2(     uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic2(uint32_t log_entry, uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(2)

/* Implementation of
 * void _trace_event3(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox3(       uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_atomic3(     uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic3(uint32_t log_entry, uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(3)

/* Implementation of
 * void _trace_event4(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox4(       uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_atomic4(     uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic4(uint32_t log_entry, uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(4)

/* Implementation of
 * void _trace_event5(            uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox5(       uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_atomic5(     uint32_t log_entry, uint32_t params...) {...}
 * void _trace_event_mbox_atomic5(uint32_t log_entry, uint32_t params...) {...}
 */
_TRACE_EVENT_NTH_IMPL_GROUP(5)

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
