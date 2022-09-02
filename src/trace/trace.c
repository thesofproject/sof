// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>
//         Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/dma-trace.h>
#include <sof/trace/preproc.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stdarg.h>
#include <stdint.h>

/* every trace calls uses IPC context in this file */
LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx dt_tr;

#if CONFIG_TRACE_FILTERING_ADAPTIVE
struct recent_log_entry {
	uint32_t entry_id;
	uint64_t message_ts;
	uint64_t first_suppression_ts;
	uint32_t trigger_count;
};

struct recent_trace_context {
	struct recent_log_entry recent_entries[CONFIG_TRACE_RECENT_ENTRIES_COUNT];
};
#endif /* CONFIG_TRACE_FILTERING_ADAPTIVE */

/** MAILBOX_TRACE_BASE ring buffer */
struct trace {
	uintptr_t pos ; /**< offset of the next byte to write */
	uint32_t enable;
#if CONFIG_TRACE_FILTERING_ADAPTIVE
	bool user_filter_override;	/* whether filtering was overridden by user or not */
#endif /* CONFIG_TRACE_FILTERING_ADAPTIVE */
	struct k_spinlock lock; /* locking mechanism */

#if CONFIG_TRACE_FILTERING_ADAPTIVE
	struct recent_trace_context trace_core_context[CONFIG_CORE_COUNT];
#endif
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

static void put_header(void *dst, const struct sof_uuid_entry *uid,
		       uint32_t id_1, uint32_t id_2,
		       uint32_t entry, uint64_t timestamp)
{
	struct dma_trace_data *trace_data = dma_trace_data_get();
	/* Support very early tracing */
	uint64_t delta = dma_trace_initialized(trace_data) ? trace_data->time_delta : 0;
	struct log_entry_header header;
	int ret;

	header.uid = (uintptr_t)uid;
	header.id_0 = id_1 & TRACE_ID_MASK;
	header.id_1 = id_2 & TRACE_ID_MASK;
	header.core_id = cpu_get_id();
	header.timestamp = timestamp + delta;
	header.log_entry_address = entry;

	ret = memcpy_s(dst, sizeof(header), &header, sizeof(header));
	assert(!ret);
}

#ifndef __ZEPHYR__
/** Ring buffer for the mailbox trace */
void mtrace_event(const char *data, uint32_t length)
{
	struct trace *trace = trace_get();
	char *t = (char *)MAILBOX_TRACE_BASE;
	const int32_t available = MAILBOX_TRACE_SIZE - trace->pos;

	if (available < length) { /* wrap */
		memset(t + trace->pos, 0xff, available);
		dcache_writeback_region((__sparse_force char __sparse_cache *)t + trace->pos,
					available);
		trace->pos = 0;
	}

	memcpy_s(t + trace->pos, MAILBOX_TRACE_SIZE - trace->pos,
		 data, length);
	dcache_writeback_region((__sparse_force char __sparse_cache *)t + trace->pos, length);
	trace->pos += length;
}
#endif /* __ZEPHYR__ */

#if CONFIG_TRACE_FILTERING_VERBOSITY
/**
 * \brief Runtime trace filtering based on verbosity level
 * \param lvl log level (LOG_LEVEL_ ERROR, INFO, DEBUG ...)
 * \param uuid uuid address
 * \return false when trace is filtered out, otherwise true
 */
static inline bool trace_filter_verbosity(uint32_t lvl, const struct tr_ctx *ctx)
{
	STATIC_ASSERT(LOG_LEVEL_CRITICAL < LOG_LEVEL_VERBOSE,
		      LOG_LEVEL_CRITICAL_MUST_HAVE_LOWEST_VALUE);
	return lvl <= ctx->level;
}
#endif /* CONFIG_TRACE_FILTERING_VERBOSITY */

#if CONFIG_TRACE_FILTERING_ADAPTIVE
/** Report how many times an entry was suppressed and clear it. */
static void emit_suppressed_entry(struct recent_log_entry *entry)
{
	_log_message(trace_log_unfiltered, false, LOG_LEVEL_INFO, _TRACE_INV_CLASS, &dt_tr,
		     _TRACE_INV_ID, _TRACE_INV_ID, "Suppressed %u similar messages: %pQ",
		     entry->trigger_count - CONFIG_TRACE_BURST_COUNT,
		     (void *)entry->entry_id);

	memset(entry, 0, sizeof(*entry));
}

/** Flush entries that have not been seen again in the last
 * CONFIG_TRACE_RECENT_TIME_THRESHOLD microseconds before current_ts.
 */
static void emit_recent_entries(uint64_t current_ts)
{
	struct trace *trace = trace_get();
	struct recent_log_entry *recent_entries =
		trace->trace_core_context[cpu_get_id()].recent_entries;
	int i;

	/* Check if any tracked entries were dormant long enough to unsuppress them */
	for (i = 0; i < CONFIG_TRACE_RECENT_ENTRIES_COUNT; i++) {
		if (recent_entries[i].entry_id) {
			if (current_ts - recent_entries[i].message_ts >
			    CONFIG_TRACE_RECENT_TIME_THRESHOLD) {
				if (recent_entries[i].trigger_count > CONFIG_TRACE_BURST_COUNT)
					emit_suppressed_entry(&recent_entries[i]);
				else
					memset(&recent_entries[i], 0, sizeof(recent_entries[i]));
			}
		}
	}

}

/**
 * \brief Runtime trace flood suppression
 * \param log_level log verbosity level
 * \param entry ID of log entry
 * \param message_ts message timestamp
 * \return true when the message must be printed by the caller because it was not filtered
 */
static bool trace_filter_flood(uint32_t log_level, uint32_t entry, uint64_t message_ts)
{
	struct trace *trace = trace_get();
	struct recent_log_entry *recent_entries =
		trace->trace_core_context[cpu_get_id()].recent_entries;
	struct recent_log_entry *oldest_entry = recent_entries;
	int i;

	/* don't attempt to suppress debug messages using this method, it would be uneffective */
	if (log_level >= LOG_LEVEL_DEBUG)
		return true;

	/* check if same log entry was sent recently */
	for (i = 0; i < CONFIG_TRACE_RECENT_ENTRIES_COUNT; i++) {
		/* Identify the oldest (or empty) entry to evict if we have no match  */
		if (oldest_entry->first_suppression_ts > recent_entries[i].first_suppression_ts)
			oldest_entry = &recent_entries[i];

		if (recent_entries[i].entry_id == entry) {
			/* We have a match but include this message in this burst only if the
			 * burst:
			   - 1. hasn't lasted for too long;
			   - 2. hasn't been quiet for too long.
			 */
			if (message_ts - recent_entries[i].first_suppression_ts <
			    CONFIG_TRACE_RECENT_MAX_TIME &&
			    message_ts - recent_entries[i].message_ts <
			    CONFIG_TRACE_RECENT_TIME_THRESHOLD) {
				recent_entries[i].trigger_count++;
				/* Refresh last seen time */
				recent_entries[i].message_ts = message_ts;

				/* Allow the start of a burst to be printed normally */
				return recent_entries[i].trigger_count <= CONFIG_TRACE_BURST_COUNT;
			}

			/* Emit and clear this burst */
			if (recent_entries[i].trigger_count > CONFIG_TRACE_BURST_COUNT)
				emit_suppressed_entry(&recent_entries[i]);
			else
				memset(&recent_entries[i], 0, sizeof(recent_entries[i]));

			return true;
		}
	}

	/* Make room for tracking new entry, by emitting the oldest one in the filter */
	if (oldest_entry->entry_id && oldest_entry->trigger_count > CONFIG_TRACE_BURST_COUNT)
		emit_suppressed_entry(oldest_entry);

	/* Start a new burst */
	oldest_entry->entry_id = entry;
	oldest_entry->message_ts = message_ts;
	oldest_entry->first_suppression_ts = message_ts;
	oldest_entry->trigger_count = 1;

	return true;
}
#endif /* CONFIG_TRACE_FILTERING_ADAPTIVE */

/** Implementation shared and invoked by both adaptive filtering and
 * not. Serializes events into trace messages and passes them to
 * dtrace_event()
 */
static void dma_trace_log(bool send_atomic, uint32_t log_entry, const struct tr_ctx *ctx,
			  uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, va_list vargs)
{
	uint32_t data[MESSAGE_SIZE_DWORDS(_TRACE_EVENT_MAX_ARGUMENT_COUNT)];
	const int message_size = MESSAGE_SIZE(arg_count);
	int i;

	/* fill log content. arg_count is in the dictionary. */
	put_header(data, ctx->uuid_p, id_1, id_2, log_entry, sof_cycle_get_64_safe());

	for (i = 0; i < arg_count; ++i)
		data[PAYLOAD_OFFSET(i)] = va_arg(vargs, uint32_t);

	/* send event by */
	if (send_atomic)
		dtrace_event_atomic((const char *)data, message_size);
	else
		dtrace_event((const char *)data, message_size);

}

void trace_log_unfiltered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			  uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, va_list vl)
{
	struct trace *trace = trace_get();

	if (!trace->enable) {
		return;
	}

	dma_trace_log(send_atomic, (uint32_t)log_entry, ctx, lvl, id_1, id_2, arg_count, vl);
}

void trace_log_filtered(bool send_atomic, const void *log_entry, const struct tr_ctx *ctx,
			uint32_t lvl, uint32_t id_1, uint32_t id_2, int arg_count, va_list vl)
{
	struct trace *trace = trace_get();

	if (!trace->enable) {
		return;
	}

#if CONFIG_TRACE_FILTERING_VERBOSITY
	if (!trace_filter_verbosity(lvl, ctx))
		return;
#endif /* CONFIG_TRACE_FILTERING_VERBOSITY */

#if CONFIG_TRACE_FILTERING_ADAPTIVE
	if (!trace->user_filter_override) {
		const uint64_t current_ts = sof_cycle_get_64_safe();

		emit_recent_entries(current_ts);

		if (!trace_filter_flood(lvl, (uint32_t)log_entry, current_ts))
			return;
	}
#endif /* CONFIG_TRACE_FILTERING_ADAPTIVE */

	dma_trace_log(send_atomic, (uint32_t)log_entry, ctx, lvl, id_1, id_2, arg_count, vl);
}

struct sof_ipc_trace_filter_elem *trace_filter_fill(struct sof_ipc_trace_filter_elem *elem,
						    struct sof_ipc_trace_filter_elem *end,
						    struct trace_filter *filter)
{
	filter->log_level = -1;
	filter->uuid_id = 0;
	filter->comp_id = -1;
	filter->pipe_id = -1;

	while (elem <= end) {
		switch (elem->key & SOF_IPC_TRACE_FILTER_ELEM_TYPE_MASK) {
		case SOF_IPC_TRACE_FILTER_ELEM_SET_LEVEL:
			filter->log_level = elem->value;
			break;
		case SOF_IPC_TRACE_FILTER_ELEM_BY_UUID:
			filter->uuid_id = elem->value;
			break;
		case SOF_IPC_TRACE_FILTER_ELEM_BY_COMP:
			filter->comp_id = elem->value;
			break;
		case SOF_IPC_TRACE_FILTER_ELEM_BY_PIPE:
			filter->pipe_id = elem->value;
			break;
		default:
			tr_err(&ipc_tr, "Invalid SOF_IPC_TRACE_FILTER_ELEM 0x%x",
			       elem->key);
			return NULL;
		}

		/* each filter set must be terminated with FIN flag and have new log level */
		if (elem->key & SOF_IPC_TRACE_FILTER_ELEM_FIN) {
			if (filter->log_level < 0) {
				tr_err(&ipc_tr, "Each trace filter set must specify new log level");
				return NULL;
			} else {
				return elem + 1;
			}
		}

		++elem;
	}

	tr_err(&ipc_tr, "Trace filter elements set is not properly terminated");
	return NULL;
}

/* update global components, which tr_ctx is stored inside special section */
static int trace_filter_update_global(int32_t log_level, uint32_t uuid_id)
{
	int cnt = 0;
#if !defined(CONFIG_LIBRARY)
	extern void *_trace_ctx_start;
	extern void *_trace_ctx_end;
	struct tr_ctx *ptr = (struct tr_ctx *)&_trace_ctx_start;
	struct tr_ctx *end = (struct tr_ctx *)&_trace_ctx_end;

	/* iterate over global `tr_ctx` entries located in their own section */
	/* cppcheck-suppress comparePointers */
	while (ptr < end) {
		/*
		 * when looking for specific uuid element,
		 * then find, update and stop searching
		 */
		if ((uintptr_t)ptr->uuid_p == uuid_id) {
			ptr->level = log_level;
			return 1;
		}
		/* otherwise each element should be updated */
		if (!ptr->uuid_p) {
			ptr->level = log_level;
			++cnt;
		}
		++ptr;
	}
#endif

	return cnt;
}

/* return trace context from any ipc component type */
static struct tr_ctx *trace_filter_ipc_comp_context(struct ipc_comp_dev *icd)
{
	switch (icd->type) {
	case COMP_TYPE_COMPONENT:
		return &icd->cd->tctx;
	case COMP_TYPE_BUFFER:
		return &icd->cb->tctx;
	case COMP_TYPE_PIPELINE:
		return &icd->pipeline->tctx;
	/* each COMP_TYPE must be specified */
	default:
		tr_err(&ipc_tr, "Unknown trace context for ipc component type 0x%X",
		       icd->type);
		return NULL;
	}
}

/* update ipc components, wchich tr_ctx may be read from ipc_comp_dev structure */
static int trace_filter_update_instances(int32_t log_level, uint32_t uuid_id,
					 int32_t pipe_id, int32_t comp_id)
{
	struct ipc *ipc = ipc_get();
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct tr_ctx *ctx;
	bool correct_comp;
	int cnt = 0;

	/* compare each ipc component with filter settings and update log level after pass */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		ctx = trace_filter_ipc_comp_context(icd);
		if (!ctx)
			return -EINVAL;
		correct_comp = comp_id == -1 || icd->id == comp_id; /* todo: icd->topo_id */
		correct_comp &= uuid_id == 0 || (uintptr_t)ctx->uuid_p == uuid_id;
		correct_comp &= pipe_id == -1 || ipc_comp_pipe_id(icd) == pipe_id;
		if (correct_comp) {
			ctx->level = log_level;
			++cnt;
		}

	}
	return cnt;
}

int trace_filter_update(const struct trace_filter *filter)
{
	int ret = 0;
#if CONFIG_TRACE_FILTERING_ADAPTIVE
	struct trace *trace = trace_get();

	if (!trace->user_filter_override) {
		trace->user_filter_override = true;
		tr_info(&ipc_tr, "Adaptive filtering disabled by user");
	}
#endif /* CONFIG_TRACE_FILTERING_ADAPTIVE */

	/* validate log level, LOG_LEVEL_CRITICAL has low value, LOG_LEVEL_VERBOSE high */
	if (filter->log_level < LOG_LEVEL_CRITICAL ||
	    filter->log_level > LOG_LEVEL_VERBOSE)
		return -EINVAL;

	/* update `*`, `name*` or global `name` */
	if (filter->pipe_id == -1 && filter->comp_id == -1)
		ret = trace_filter_update_global(filter->log_level, filter->uuid_id);

	/* update `*`, `name*`, `nameX.*` or `nameX.Y`, `name` may be '*' */
	ret += trace_filter_update_instances(filter->log_level, filter->uuid_id,
					     filter->pipe_id, filter->comp_id);
	return ret > 0 ? ret : -EINVAL;
}

/** Sends all pending DMA messages to mailbox (for emergencies) */
void trace_flush_dma_to_mbox(void)
{
	struct trace *trace = trace_get();
	volatile uint64_t *t;
	k_spinlock_key_t key;

	key = k_spin_lock(&trace->lock);

	/* get mailbox position */
	t = (volatile uint64_t *)(MAILBOX_TRACE_BASE + trace->pos);

	/* flush dma trace messages */
	dma_trace_flush((void *)t);

	k_spin_unlock(&trace->lock, key);
}

void trace_on(void)
{
	struct trace *trace = trace_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&trace->lock);

	trace->enable = 1;
	dma_trace_on();

	k_spin_unlock(&trace->lock, key);
}

void trace_off(void)
{
	struct trace *trace = trace_get();
	k_spinlock_key_t key;

	key = k_spin_lock(&trace->lock);

	trace->enable = 0;
	dma_trace_off();

	k_spin_unlock(&trace->lock, key);
}

void trace_init(struct sof *sof)
{
	sof->trace = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sof->trace));
	sof->trace->enable = 1;
	sof->trace->pos = 0;
#if CONFIG_TRACE_FILTERING_ADAPTIVE
	sof->trace->user_filter_override = false;
#endif /* CONFIG_TRACE_FILTERING_ADAPTIVE */
	k_spinlock_init(&sof->trace->lock);

#ifndef __ZEPHYR__
	/* Zephyr owns and has already initialized this buffer (and
	 * likely has already logged to it by the time we get here).
	 * Don't touch.
	 */
	bzero((void *)MAILBOX_TRACE_BASE, MAILBOX_TRACE_SIZE);
	dcache_writeback_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_TRACE_BASE,
					   MAILBOX_TRACE_SIZE);
#endif

	dma_trace_init_early(sof);
}

static void mtrace_dict_entry_vl(bool atomic_context, uint32_t dict_entry_address,
				 int n_args, va_list ap)
{
	int i;
	char packet[MESSAGE_SIZE(_TRACE_EVENT_MAX_ARGUMENT_COUNT)];
	uint32_t *args = (uint32_t *)&packet[MESSAGE_SIZE(0)];
	const uint64_t tstamp = sof_cycle_get_64_safe();

	put_header(packet, dt_tr.uuid_p, _TRACE_INV_ID, _TRACE_INV_ID,
		   dict_entry_address, tstamp);

	for (i = 0; i < MIN(n_args, _TRACE_EVENT_MAX_ARGUMENT_COUNT); i++)
		args[i] = va_arg(ap, uint32_t);

	if (atomic_context) {
		mtrace_event(packet, MESSAGE_SIZE(n_args));
	} else {
		struct trace * const trace = trace_get();
		k_spinlock_key_t key;

		key = k_spin_lock(&trace->lock);
		mtrace_event(packet, MESSAGE_SIZE(n_args));
		k_spin_unlock(&trace->lock, key);
	}
}

void mtrace_dict_entry(bool atomic_context, uint32_t dict_entry_address, int n_args, ...)
{
	va_list ap;

	va_start(ap, n_args);
	mtrace_dict_entry_vl(atomic_context, dict_entry_address, n_args, ap);
	va_end(ap);
}

void _log_sofdict(log_func_t sofdict_logf, bool atomic, const void *log_entry,
		  const struct tr_ctx *ctx, const uint32_t lvl,
		  uint32_t id_1, uint32_t id_2, int arg_count, ...)
{
	va_list ap;

#ifndef __ZEPHYR__ /* for Zephyr see _log_nodict() in trace.h */
	if (lvl <= MTRACE_DUPLICATION_LEVEL) {
		va_start(ap, arg_count);
		mtrace_dict_entry_vl(atomic, (uint32_t)log_entry, arg_count, ap);
		va_end(ap);
	}
#endif

	va_start(ap, arg_count);
	sofdict_logf(atomic, log_entry, ctx, lvl, id_1, id_2, arg_count, ap);
	va_end(ap);
}
