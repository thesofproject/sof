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

/* 42544c92-8e92-4e41-b679-34519f1c1d28 */
DECLARE_SOF_RT_UUID("buffer", buffer_uuid, 0x42544c92, 0x8e92, 0x4e41,
		 0xb6, 0x79, 0x34, 0x51, 0x9f, 0x1c, 0x1d, 0x28);
DECLARE_TR_CTX(buffer_tr, SOF_UUID(buffer_uuid), LOG_LEVEL_INFO);

struct comp_buffer *buffer_alloc(uint32_t size, uint32_t caps, uint32_t align)
{
	struct comp_buffer *buffer;

	tr_dbg(&buffer_tr, "buffer_alloc()");

	/* validate request */
	if (size == 0 || size > HEAP_BUFFER_SIZE) {
		tr_err(&buffer_tr, "buffer_alloc(): new size = %u is invalid",
		       size);
		return NULL;
	}

	/* allocate new buffer */
	buffer = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			 sizeof(*buffer));
	if (!buffer) {
		tr_err(&buffer_tr, "buffer_alloc(): could not alloc structure");
		return NULL;
	}

	buffer->lock = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			       sizeof(*buffer->lock));
	if (!buffer->lock) {
		rfree(buffer);
		tr_err(&buffer_tr, "buffer_alloc(): could not alloc lock");
		return NULL;
	}

	buffer->stream.addr = rballoc_align(0, caps, size, align);
	if (!buffer->stream.addr) {
		rfree(buffer);
		tr_err(&buffer_tr, "buffer_alloc(): could not alloc size = %u bytes of type = %u",
		       size, caps);
		return NULL;
	}

	buffer_init(buffer, size, caps);

	list_init(&buffer->source_list);
	list_init(&buffer->sink_list);
	spinlock_init(buffer->lock);

	return buffer;
}

void buffer_zero(struct comp_buffer *buffer)
{
	buf_dbg(buffer, "stream_zero()");

	bzero(buffer->stream.addr, buffer->stream.size);
	if (buffer->caps & SOF_MEM_CAPS_DMA)
		dcache_writeback_region(buffer->stream.addr,
					buffer->stream.size);
}

int buffer_set_size(struct comp_buffer *buffer, uint32_t size)
{
	void *new_ptr = NULL;

	/* validate request */
	if (size == 0 || size > HEAP_BUFFER_SIZE) {
		buf_err(buffer, "resize size = %u is invalid", size);
		return -EINVAL;
	}

	if (size == buffer->stream.size)
		return 0;

	new_ptr = rbrealloc(buffer->stream.addr, SOF_MEM_FLAG_NO_COPY,
			    buffer->caps, size, buffer->stream.size);

	/* we couldn't allocate bigger chunk */
	if (!new_ptr && size > buffer->stream.size) {
		buf_err(buffer, "resize can't alloc %u bytes type %u",
			buffer->stream.size, buffer->caps);
		return -ENOMEM;
	}

	/* use bigger chunk, else just use the old chunk but set smaller */
	if (new_ptr)
		buffer->stream.addr = new_ptr;

	buffer_init(buffer, size, buffer->caps);

	return 0;
}

int buffer_set_params(struct comp_buffer *buffer, struct sof_ipc_stream_params *params,
		      bool force_update)
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

	buffer->buffer_fmt = params->buffer_fmt;
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		buffer->chmap[i] = params->chmap[i];

	buffer->hw_params_configured = true;

	return 0;
}

bool buffer_params_match(struct comp_buffer *buffer, struct sof_ipc_stream_params *params,
			 uint32_t flag)
{
	assert(params && buffer);

	if ((flag & BUFF_PARAMS_FRAME_FMT) &&
	    buffer->stream.frame_fmt != params->frame_fmt)
		return false;

	if ((flag & BUFF_PARAMS_RATE) &&
	    buffer->stream.rate != params->rate)
		return false;

	if ((flag & BUFF_PARAMS_CHANNELS) &&
	    buffer->stream.channels != params->channels)
		return false;

	return true;
}

/* free component in the pipeline */
void buffer_free(struct comp_buffer *buffer)
{
	struct buffer_cb_free cb_data = {
		.buffer = buffer,
	};

	buf_dbg(buffer, "buffer_free()");

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
		buf_info(buffer, "comp_update_buffer_produce(), no bytes to produce, source->comp.id = %u, source->comp.type = %u, sink->comp.id = %u, sink->comp.type = %u",
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

	buf_dbg(buffer, "comp_update_buffer_produce(), ((buffer->avail << 16) | buffer->free) = %08x, ((buffer->id << 16) | buffer->size) = %08x",
		(audio_stream_get_avail_bytes(&buffer->stream) << 16) |
		 audio_stream_get_free_bytes(&buffer->stream),
		(buffer->id << 16) | buffer->stream.size);
	buf_dbg(buffer, "comp_update_buffer_produce(), ((buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
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
		buf_info(buffer, "comp_update_buffer_consume(), no bytes to consume, source->comp.id = %u, source->comp.type = %u, sink->comp.id = %u, sink->comp.type = %u",
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

	buf_dbg(buffer, "comp_update_buffer_consume(), (buffer->avail << 16) | buffer->free = %08x, (buffer->id << 16) | buffer->size = %08x, (buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
		(audio_stream_get_avail_bytes(&buffer->stream) << 16) |
		 audio_stream_get_free_bytes(&buffer->stream),
		(buffer->id << 16) | buffer->stream.size,
		((char *)buffer->stream.r_ptr - addr) << 16 |
		((char *)buffer->stream.w_ptr - addr));
}
