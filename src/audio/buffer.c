// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct comp_buffer *buffer_alloc(uint32_t size, uint32_t caps, uint32_t align)
{
	struct comp_buffer *buffer;

	tracev_buffer("buffer_alloc()");

	/* validate request */
	if (size == 0 || size > HEAP_BUFFER_SIZE) {
		trace_buffer_error("buffer_alloc() error: "
				   "new size = %u is invalid", size);
		return NULL;
	}

	/* allocate new buffer */
	buffer = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			 sizeof(*buffer));
	if (!buffer) {
		trace_buffer_error("buffer_alloc() error: "
				   "could not alloc structure");
		return NULL;
	}

	buffer->lock = rzalloc(SOF_MEM_ZONE_RUNTIME, SOF_MEM_FLAG_SHARED,
			       SOF_MEM_CAPS_RAM, sizeof(*buffer->lock));
	if (!buffer->lock) {
		rfree(buffer);
		trace_buffer_error("buffer_alloc() error: could not alloc lock");
		return NULL;
	}

	buffer->stream.addr = rballoc_align(0, caps, size, align);
	if (!buffer->stream.addr) {
		rfree(buffer);
		trace_buffer_error("buffer_alloc() error: "
				   "could not alloc size = %u "
				   "bytes of type = %u",
				   size, caps);
		return NULL;
	}

	buffer_init(buffer, size, caps);

	list_init(&buffer->source_list);
	list_init(&buffer->sink_list);
	spinlock_init(buffer->lock);

	return buffer;
}

/* create a new component in the pipeline */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc)
{
	struct comp_buffer *buffer;

	trace_buffer("buffer_new()");

	/* allocate buffer */
	buffer = buffer_alloc(desc->size, desc->caps, PLATFORM_DCACHE_ALIGN);
	if (buffer) {
		buffer->id = desc->comp.id;
		buffer->pipeline_id = desc->comp.pipeline_id;
		buffer->core = desc->comp.core;

		dcache_writeback_invalidate_region(buffer, sizeof(*buffer));
	}

	return buffer;
}

int buffer_set_size(struct comp_buffer *buffer, uint32_t size)
{
	void *new_ptr = NULL;

	/* validate request */
	if (size == 0 || size > HEAP_BUFFER_SIZE) {
		trace_buffer_error_with_ids(buffer, "resize error: size = %u is invalid",
					    size);
		return -EINVAL;
	}

	if (size == buffer->stream.size)
		return 0;

	new_ptr = rbrealloc(buffer->stream.addr, 0, buffer->caps, size);

	/* we couldn't allocate bigger chunk */
	if (!new_ptr && size > buffer->stream.size) {
		trace_buffer_error_with_ids(buffer, "resize error: can't alloc %u bytes type %u",
					    buffer->stream.size, buffer->caps);
		return -ENOMEM;
	}

	/* use bigger chunk, else just use the old chunk but set smaller */
	if (new_ptr)
		buffer->stream.addr = new_ptr;

	buffer_init(buffer, size, buffer->caps);

	return 0;
}

/* free component in the pipeline */
void buffer_free(struct comp_buffer *buffer)
{
	struct buffer_cb_free cb_data = {
		.buffer = buffer,
	};

	tracev_buffer_with_ids(buffer, "buffer_free()");

	notifier_event(buffer, NOTIFIER_ID_BUFFER_FREE,
		       NOTIFIER_TARGET_CORE_LOCAL, &cb_data, sizeof(cb_data));

	/* In case some listeners didn't unregister from buffer's callbacks */
	notifier_unregister_all(NULL, buffer);

	list_item_del(&buffer->source_list);
	list_item_del(&buffer->sink_list);
	rfree(buffer->stream.addr);
	rfree(buffer->lock);
	rfree(buffer);
}

void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes)
{
	uint32_t flags = 0;
	struct buffer_cb_transact cb_data = {
		.buffer = buffer,
		.transaction_amount = bytes,
		.transaction_begin_address = buffer->stream.w_ptr,
	};
	char *addr;

	/* return if no bytes */
	if (!bytes) {
		trace_buffer_with_ids(buffer,
				      "comp_update_buffer_produce(), no bytes to produce, source->comp.id = %u, source->comp.type = %u, sink->comp.id = %u, sink->comp.type = %u",
				      dev_comp_id(buffer->source),
				      dev_comp_type(buffer->source),
				      dev_comp_id(buffer->sink),
				      dev_comp_type(buffer->sink));
		return;
	}

	buffer_lock(buffer, &flags);

	audio_stream_produce(&buffer->stream, bytes);

	notifier_event(buffer, NOTIFIER_ID_BUFFER_PRODUCE,
		       NOTIFIER_TARGET_CORE_LOCAL, &cb_data, sizeof(cb_data));

	buffer_unlock(buffer, flags);

	addr = buffer->stream.addr;

	tracev_buffer_with_ids(buffer,
			       "comp_update_buffer_produce(), ((buffer->avail << 16) | buffer->free) = %08x, ((buffer->id << 16) | buffer->size) = %08x",
			       (buffer->stream.avail << 16) |
			       buffer->stream.free,
			       (buffer->id << 16) | buffer->stream.size);
	tracev_buffer_with_ids(buffer,
			       "comp_update_buffer_produce(), ((buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
			       ((char *)buffer->stream.r_ptr - addr) << 16 |
			       ((char *)buffer->stream.w_ptr - addr));
}

void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes)
{
	uint32_t flags = 0;
	struct buffer_cb_transact cb_data = {
		.buffer = buffer,
		.transaction_amount = bytes,
		.transaction_begin_address = buffer->stream.r_ptr,
	};
	char *addr;

	/* return if no bytes */
	if (!bytes) {
		trace_buffer_with_ids(buffer,
				      "comp_update_buffer_consume(), no bytes to consume, source->comp.id = %u, source->comp.type = %u, sink->comp.id = %u, sink->comp.type = %u",
				      dev_comp_id(buffer->source),
				      dev_comp_type(buffer->source),
				      dev_comp_id(buffer->sink),
				      dev_comp_type(buffer->sink));
		return;
	}

	buffer_lock(buffer, &flags);

	audio_stream_consume(&buffer->stream, bytes);

	notifier_event(buffer, NOTIFIER_ID_BUFFER_CONSUME,
		       NOTIFIER_TARGET_CORE_LOCAL, &cb_data, sizeof(cb_data));

	buffer_unlock(buffer, flags);

	addr = buffer->stream.addr;

	tracev_buffer_with_ids(buffer,
			       "comp_update_buffer_consume(), (buffer->avail << 16) | buffer->free = %08x, (buffer->id << 16) | buffer->size = %08x, (buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
			       (buffer->stream.avail << 16) |
			       buffer->stream.free,
			       (buffer->id << 16) | buffer->stream.size,
			       ((char *)buffer->stream.r_ptr - addr) << 16 |
			       ((char *)buffer->stream.w_ptr - addr));
}
