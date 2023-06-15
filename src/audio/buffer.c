// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <rtos/interrupt.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(buffer, CONFIG_SOF_LOG_LEVEL);

/* 42544c92-8e92-4e41-b679-34519f1c1d28 */
DECLARE_SOF_RT_UUID("buffer", buffer_uuid, 0x42544c92, 0x8e92, 0x4e41,
		 0xb6, 0x79, 0x34, 0x51, 0x9f, 0x1c, 0x1d, 0x28);
DECLARE_TR_CTX(buffer_tr, SOF_UUID(buffer_uuid), LOG_LEVEL_INFO);

struct comp_buffer *buffer_alloc(uint32_t size, uint32_t caps, uint32_t flags, uint32_t align)
{
	struct comp_buffer *buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	void *stream_addr;

	tr_dbg(&buffer_tr, "buffer_alloc()");

	/* validate request */
	if (size == 0) {
		tr_err(&buffer_tr, "buffer_alloc(): new size = %u is invalid",
		       size);
		return NULL;
	}

	/*
	 * allocate new buffer, align the allocation size to a cache line for
	 * the coherent API
	 */
	buffer = coherent_init_thread(struct comp_buffer, c);
	if (!buffer) {
		tr_err(&buffer_tr, "buffer_alloc(): could not alloc structure");
		return NULL;
	}

	stream_addr = rballoc_align(0, caps, size, align);
	if (!stream_addr) {
		rfree(buffer);
		tr_err(&buffer_tr, "buffer_alloc(): could not alloc size = %u bytes of type = %u",
		       size, caps);
		return NULL;
	}

	/* From here no more uncached access to the buffer object, except its list headers */
	buffer_c = buffer_acquire(buffer);
	audio_stream_set_addr(&buffer_c->stream, stream_addr);
	buffer_init(buffer_c, size, caps);

	audio_stream_set_underrun(&buffer_c->stream, !!(flags & SOF_BUF_UNDERRUN_PERMITTED));
	audio_stream_set_overrun(&buffer_c->stream, !!(flags & SOF_BUF_OVERRUN_PERMITTED));

	buffer_release(buffer_c);

	/*
	 * The buffer hasn't yet been marked as shared, hence buffer_release()
	 * hasn't written back and invalidated the cache. Therefore we have to
	 * do this manually now before adding to the lists. Buffer list
	 * structures are always accessed uncached and they're never modified at
	 * run-time, i.e. buffers are never relinked. So we have to make sure,
	 * that what we have written into buffer's cache is in RAM before
	 * modifying that RAM bypassing cache, and that after this cache is
	 * re-loaded again.
	 */
	dcache_writeback_invalidate_region(uncache_to_cache(buffer), sizeof(*buffer));

	list_init(&buffer->source_list);
	list_init(&buffer->sink_list);

	return buffer;
}

void buffer_zero(struct comp_buffer __sparse_cache *buffer)
{
	buf_dbg(buffer, "stream_zero()");

	bzero(audio_stream_get_addr(&buffer->stream), audio_stream_get_size(&buffer->stream));
	if (buffer->caps & SOF_MEM_CAPS_DMA)
		dcache_writeback_region((__sparse_force void __sparse_cache *)
					audio_stream_get_addr(&buffer->stream),
					audio_stream_get_size(&buffer->stream));
}

int buffer_set_size(struct comp_buffer __sparse_cache *buffer, uint32_t size, uint32_t alignment)
{
	void *new_ptr = NULL;

	/* validate request */
	if (size == 0) {
		buf_err(buffer, "resize size = %u is invalid", size);
		return -EINVAL;
	}

	if (size == audio_stream_get_size(&buffer->stream))
		return 0;

	if (!alignment)
		new_ptr = rbrealloc(audio_stream_get_addr(&buffer->stream), SOF_MEM_FLAG_NO_COPY,
				    buffer->caps, size, audio_stream_get_size(&buffer->stream));
	else
		new_ptr = rbrealloc_align(audio_stream_get_addr(&buffer->stream),
					  SOF_MEM_FLAG_NO_COPY, buffer->caps, size,
					  audio_stream_get_size(&buffer->stream), alignment);
	/* we couldn't allocate bigger chunk */
	if (!new_ptr && size > audio_stream_get_size(&buffer->stream)) {
		buf_err(buffer, "resize can't alloc %u bytes type %u",
			audio_stream_get_size(&buffer->stream), buffer->caps);
		return -ENOMEM;
	}

	/* use bigger chunk, else just use the old chunk but set smaller */
	if (new_ptr)
		buffer->stream.addr = new_ptr;

	buffer_init(buffer, size, buffer->caps);

	return 0;
}

int buffer_set_params(struct comp_buffer __sparse_cache *buffer,
		      struct sof_ipc_stream_params *params, bool force_update)
{
	int ret;
	int i;

	if (!params) {
		buf_err(buffer, "buffer_set_params(): !params");
		return -EINVAL;
	}

	if (buffer->hw_params_configured && !force_update)
		return 0;

	ret = audio_stream_set_params(&buffer->stream, params);
	if (ret < 0) {
		buf_err(buffer, "buffer_set_params(): audio_stream_set_params failed");
		return -EINVAL;
	}

	audio_stream_set_buffer_fmt(&buffer->stream, params->buffer_fmt);
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buffer->chmap[i] = params->chmap[i];

	buffer->hw_params_configured = true;

	return 0;
}

bool buffer_params_match(struct comp_buffer __sparse_cache *buffer,
			 struct sof_ipc_stream_params *params, uint32_t flag)
{
	assert(params);

	if ((flag & BUFF_PARAMS_FRAME_FMT) &&
	     audio_stream_get_frm_fmt(&buffer->stream) != params->frame_fmt)
		return false;

	if ((flag & BUFF_PARAMS_RATE) &&
	     audio_stream_get_rate(&buffer->stream) != params->rate)
		return false;

	if ((flag & BUFF_PARAMS_CHANNELS) &&
	     audio_stream_get_channels(&buffer->stream) != params->channels)
		return false;

	return true;
}

/* free component in the pipeline */
void buffer_free(struct comp_buffer *buffer)
{
	struct buffer_cb_free cb_data = {
		.buffer = buffer,
	};

	if (!buffer)
		return;

	buf_dbg(buffer, "buffer_free()");

	notifier_event(buffer, NOTIFIER_ID_BUFFER_FREE,
		       NOTIFIER_TARGET_CORE_LOCAL, &cb_data, sizeof(cb_data));

	/* In case some listeners didn't unregister from buffer's callbacks */
	notifier_unregister_all(NULL, buffer);

	rfree(buffer->stream.addr);
	coherent_free_thread(buffer, c);
}

/*
 * comp_update_buffer_produce() and comp_update_buffer_consume() send
 * NOTIFIER_ID_BUFFER_PRODUCE and NOTIFIER_ID_BUFFER_CONSUME notifier events
 * respectively. The only recipient of those notifications is probes. The
 * target for those notifications is always the current core, therefore notifier
 * callbacks will be called synchronously from notifier_event() calls. Therefore
 * we cannot pass unlocked buffer pointers to probes, because if they try to
 * acquire the buffer, that can cause a deadlock. In general locked objects
 * shouldn't be passed to potentially asynchronous contexts, but here we have no
 * choice but to use our knowledge of the local notifier behaviour and pass
 * locked buffers to notification recipients.
 */
void comp_update_buffer_produce(struct comp_buffer __sparse_cache *buffer, uint32_t bytes)
{
	struct buffer_cb_transact cb_data = {
		.buffer = buffer,
		.transaction_amount = bytes,
		.transaction_begin_address = audio_stream_get_wptr(&buffer->stream),
	};

	/* return if no bytes */
	if (!bytes) {
		buf_dbg(buffer, "comp_update_buffer_produce(), no bytes to produce, source->comp.id = %u, source->comp.type = %u, sink->comp.id = %u, sink->comp.type = %u",
			buffer->source ? dev_comp_id(buffer->source) : (unsigned int)UINT32_MAX,
			buffer->source ? dev_comp_type(buffer->source) : (unsigned int)UINT32_MAX,
			buffer->sink ? dev_comp_id(buffer->sink) : (unsigned int)UINT32_MAX,
			buffer->sink ? dev_comp_type(buffer->sink) : (unsigned int)UINT32_MAX);
		return;
	}

	audio_stream_produce(&buffer->stream, bytes);

	/* Notifier looks for the pointer value to match it against registration */
	notifier_event(cache_to_uncache(buffer), NOTIFIER_ID_BUFFER_PRODUCE,
		       NOTIFIER_TARGET_CORE_LOCAL, &cb_data, sizeof(cb_data));

	buf_dbg(buffer, "comp_update_buffer_produce(), ((buffer->avail << 16) | buffer->free) = %08x, ((buffer->id << 16) | buffer->size) = %08x",
		(audio_stream_get_avail_bytes(&buffer->stream) << 16) |
		 audio_stream_get_free_bytes(&buffer->stream),
		(buffer->id << 16) | audio_stream_get_size(&buffer->stream));
	buf_dbg(buffer, "comp_update_buffer_produce(), ((buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
		((char *)audio_stream_get_rptr(&buffer->stream) -
		 (char *)audio_stream_get_addr(&buffer->stream)) << 16 |
		((char *)audio_stream_get_wptr(&buffer->stream) -
		 (char *)audio_stream_get_addr(&buffer->stream)));
}

void comp_update_buffer_consume(struct comp_buffer __sparse_cache *buffer, uint32_t bytes)
{
	struct buffer_cb_transact cb_data = {
		.buffer = buffer,
		.transaction_amount = bytes,
		.transaction_begin_address = audio_stream_get_rptr(&buffer->stream),
	};

	/* return if no bytes */
	if (!bytes) {
		buf_dbg(buffer, "comp_update_buffer_consume(), no bytes to consume, source->comp.id = %u, source->comp.type = %u, sink->comp.id = %u, sink->comp.type = %u",
			buffer->source ? dev_comp_id(buffer->source) : (unsigned int)UINT32_MAX,
			buffer->source ? dev_comp_type(buffer->source) : (unsigned int)UINT32_MAX,
			buffer->sink ? dev_comp_id(buffer->sink) : (unsigned int)UINT32_MAX,
			buffer->sink ? dev_comp_type(buffer->sink) : (unsigned int)UINT32_MAX);
		return;
	}

	audio_stream_consume(&buffer->stream, bytes);

	notifier_event(cache_to_uncache(buffer), NOTIFIER_ID_BUFFER_CONSUME,
		       NOTIFIER_TARGET_CORE_LOCAL, &cb_data, sizeof(cb_data));

	buf_dbg(buffer, "comp_update_buffer_consume(), (buffer->avail << 16) | buffer->free = %08x, (buffer->id << 16) | buffer->size = %08x, (buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
		(audio_stream_get_avail_bytes(&buffer->stream) << 16) |
		 audio_stream_get_free_bytes(&buffer->stream),
		(buffer->id << 16) | audio_stream_get_size(&buffer->stream),
		((char *)audio_stream_get_rptr(&buffer->stream) -
		 (char *)audio_stream_get_addr(&buffer->stream)) << 16 |
		((char *)audio_stream_get_wptr(&buffer->stream) -
		 (char *)audio_stream_get_addr(&buffer->stream)));
}

/*
 * Locking: must be called with interrupts disabled! Serialized IPCs protect us
 * from racing attach / detach calls, but the scheduler can interrupt the IPC
 * thread and begin using the buffer for streaming. FIXME: this is still a
 * problem with different cores.
 */
void buffer_attach(struct comp_buffer *buffer, struct list_item *head, int dir)
{
	struct list_item *list = buffer_comp_list(buffer, dir);
	struct list_item __sparse_cache *needs_sync;
	bool further_buffers_exist;

	/*
	 * There can already be buffers on the target list. If we just link this
	 * buffer, we modify the first buffer's list header via uncached alias,
	 * so its cached copy can later be written back, overwriting the
	 * modified header. FIXME: this is still a problem with different cores.
	 */
	further_buffers_exist = !list_is_empty(head);
	needs_sync = uncache_to_cache(head->next);
	if (further_buffers_exist)
		dcache_writeback_region(needs_sync, sizeof(struct list_item));
	/* The cache line can be prefetched here, invalidate it after prepending */
	list_item_prepend(list, head);
	if (further_buffers_exist)
		dcache_invalidate_region(needs_sync, sizeof(struct list_item));
	/* no dirty cache lines exist for this buffer yet, no need to write back */
	dcache_invalidate_region(uncache_to_cache(list), sizeof(*list));
}

/*
 * Locking: must be called with interrupts disabled! See buffer_attach() above
 * for details
 */
void buffer_detach(struct comp_buffer *buffer, struct list_item *head, int dir)
{
	struct list_item __sparse_cache *needs_sync_prev, *needs_sync_next;
	bool buffers_after_exist, buffers_before_exist;
	struct list_item *buf_list = buffer_comp_list(buffer, dir);

	/*
	 * There can be more buffers linked together with this one, that will
	 * still be staying on their respective pipelines and might get used via
	 * their cached aliases. If we just unlink this buffer, we modify their
	 * list header via uncached alias, so their cached copy can later be
	 * written back, overwriting the modified header. FIXME: this is still a
	 * problem with different cores.
	 */
	buffers_after_exist = head != buf_list->next;
	buffers_before_exist = head != buf_list->prev;
	needs_sync_prev = uncache_to_cache(buf_list->prev);
	needs_sync_next = uncache_to_cache(buf_list->next);
	if (buffers_after_exist)
		dcache_writeback_region(needs_sync_next, sizeof(struct list_item));
	if (buffers_before_exist)
		dcache_writeback_region(needs_sync_prev, sizeof(struct list_item));
	dcache_writeback_region(uncache_to_cache(buf_list), sizeof(*buf_list));
	/* buffers before or after can be prefetched here */
	list_item_del(buf_list);
	dcache_invalidate_region(uncache_to_cache(buf_list), sizeof(*buf_list));
	if (buffers_after_exist)
		dcache_invalidate_region(needs_sync_next, sizeof(struct list_item));
	if (buffers_before_exist)
		dcache_invalidate_region(needs_sync_prev, sizeof(struct list_item));
}
