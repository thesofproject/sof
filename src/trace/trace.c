// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <sof/debug/panic.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/dma-trace.h>
#include <sof/trace/preproc.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stdint.h>

struct trace {
	uint32_t pos ;	/* trace position */
	uint32_t enable;
	spinlock_t lock; /* locking mechanism */
};

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

static void put_header(uint32_t *dst, uint32_t id_0,
		       uint32_t id_1, uint32_t id_2,
		       uint32_t entry, uint64_t timestamp)
{
	struct timer *timer = timer_get();
	struct log_entry_header header;
	int ret;

	header.uid = id_0;
	header.id_0 = id_1 & TRACE_ID_MASK;
	header.id_1 = id_2 & TRACE_ID_MASK;
	header.core_id = cpu_get_id();
	header.timestamp = timestamp + timer->delta;
	header.log_entry_address = entry;

	ret = memcpy_s(dst, sizeof(header), &header, sizeof(header));
	assert(!ret);

	platform_shared_commit(timer, sizeof(*timer));
}

static void mtrace_event(const char *data, uint32_t length)
{
	struct trace *trace = trace_get();
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

	platform_shared_commit(trace, sizeof(*trace));
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
	struct trace *trace = trace_get();				\
	META_IF_ELSE(is_mbox)						\
	(								\
		META_IF_ELSE(is_atomic)()(unsigned long flags;)		\
	)()								\
									\
	if (!trace->enable) {						\
		platform_shared_commit(trace, sizeof(*trace));		\
		return;							\
	}								\
									\
	put_header(dt, id_0, id_1, id_2, log_entry,			\
		   platform_timer_get(timer_get()));			\
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
	struct trace *trace = trace_get();
	volatile uint64_t *t;
	uint32_t flags;

	spin_lock_irq(&trace->lock, flags);

	/* get mailbox position */
	t = (volatile uint64_t *)(MAILBOX_TRACE_BASE + trace->pos);

	/* flush dma trace messages */
	dma_trace_flush((void *)t);

	platform_shared_commit(trace, sizeof(*trace));

	spin_unlock_irq(&trace->lock, flags);
}

void trace_on(void)
{
	struct trace *trace = trace_get();
	uint32_t flags;

	spin_lock_irq(&trace->lock, flags);

	trace->enable = 1;
	dma_trace_on();

	platform_shared_commit(trace, sizeof(*trace));

	spin_unlock_irq(&trace->lock, flags);
}

void trace_off(void)
{
	struct trace *trace = trace_get();
	uint32_t flags;

	spin_lock_irq(&trace->lock, flags);

	trace->enable = 0;
	dma_trace_off();

	platform_shared_commit(trace, sizeof(*trace));

	spin_unlock_irq(&trace->lock, flags);
}

void trace_init(struct sof *sof)
{
	dma_trace_init_early(sof);

	sof->trace = rzalloc(SOF_MEM_ZONE_SYS, SOF_MEM_FLAG_SHARED,
			     SOF_MEM_CAPS_RAM, sizeof(*sof->trace));
	sof->trace->enable = 1;
	sof->trace->pos = 0;
	spinlock_init(&sof->trace->lock);

	platform_shared_commit(sof->trace, sizeof(*sof->trace));

	bzero((void *)MAILBOX_TRACE_BASE, MAILBOX_TRACE_SIZE);
	dcache_writeback_invalidate_region((void *)MAILBOX_TRACE_BASE,
					   MAILBOX_TRACE_SIZE);
}
