// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/common.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>

#include <sof/audio/ring_buffer.h>

#include <rtos/alloc.h>
#include <ipc/topology.h>

LOG_MODULE_REGISTER(ring_buffer, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(ring_buffer);
DECLARE_TR_CTX(ring_buffer_tr, SOF_UUID(ring_buffer_uuid), LOG_LEVEL_INFO);

static inline struct ring_buffer *ring_buffer_from_sink(struct sof_sink *sink)
{
	struct sof_audio_buffer *audio_buffer = sof_audio_buffer_from_sink(sink);

	return container_of(audio_buffer, struct ring_buffer, audio_buffer);
}

static inline struct ring_buffer *ring_buffer_from_source(struct sof_source *source)
{
	struct sof_audio_buffer *audio_buffer = sof_audio_buffer_from_source(source);

	return container_of(audio_buffer, struct ring_buffer, audio_buffer);
}

/**
 * @brief remove the queue from the list, free memory
 */
static void ring_buffer_free(struct sof_audio_buffer *buffer)
{
	struct ring_buffer *ring_buffer = (struct ring_buffer *)buffer;

	rfree((__sparse_force void *)ring_buffer->_data_buffer);
}

/**
 * @brief return true if the ring buffer is shared between 2 cores
 */
static inline
bool ring_buffer_is_shared(struct ring_buffer *ring_buffer)
{
	return audio_buffer_is_shared(&ring_buffer->audio_buffer);
}

static inline uint8_t __sparse_cache *ring_buffer_buffer_end(struct ring_buffer *ring_buffer)
{
	return ring_buffer->_data_buffer + ring_buffer->data_buffer_size;
}

static inline void ring_buffer_invalidate_shared(struct ring_buffer *ring_buffer,
						 void __sparse_cache *ptr, size_t size)
{
	/* no cache required in case of not shared queue */
	if (!ring_buffer_is_shared(ring_buffer))
		return;

	/* wrap-around? */
	if ((uintptr_t)ptr + size > (uintptr_t)ring_buffer_buffer_end(ring_buffer)) {
		/* writeback till the end of circular buffer */
		dcache_invalidate_region
			(ptr, (uintptr_t)ring_buffer_buffer_end(ring_buffer) - (uintptr_t)ptr);
		size -= (uintptr_t)ring_buffer_buffer_end(ring_buffer) - (uintptr_t)ptr;
		ptr = ring_buffer->_data_buffer;
	}
	/* invalidate rest of data */
	dcache_invalidate_region(ptr, size);
}

static inline void ring_buffer_writeback_shared(struct ring_buffer *ring_buffer,
						void __sparse_cache *ptr, size_t size)
{
	/* no cache required in case of not shared queue */
	if (!ring_buffer_is_shared(ring_buffer))
		return;

	/* wrap-around? */
	if ((uintptr_t)ptr + size > (uintptr_t)ring_buffer_buffer_end(ring_buffer)) {
		/* writeback till the end of circular buffer */
		dcache_writeback_region
			(ptr, (uintptr_t)ring_buffer_buffer_end(ring_buffer) - (uintptr_t)ptr);
		size -= (uintptr_t)ring_buffer_buffer_end(ring_buffer) - (uintptr_t)ptr;
		ptr = ring_buffer->_data_buffer;
	}
	/* writeback rest of data */
	dcache_writeback_region(ptr, size);
}

static inline
uint8_t __sparse_cache *ring_buffer_get_pointer(struct ring_buffer *ring_buffer, size_t offset)
{
	/* check if offset is not in "double area"
	 * lines below do a quicker version of offset %= ring_buffer->data_buffer_size;
	 */
	if (offset >= ring_buffer->data_buffer_size)
		offset -= ring_buffer->data_buffer_size;
	return ring_buffer->_data_buffer + offset;
}

static inline
size_t ring_buffer_inc_offset(struct ring_buffer *ring_buffer, size_t offset, size_t inc)
{
	assert(inc <= ring_buffer->data_buffer_size);
	offset += inc;
	/* wrap around ? 2*size because of "double area" */
	if (offset >= 2 * ring_buffer->data_buffer_size)
		offset -= 2 * ring_buffer->data_buffer_size;
	return offset;
}

static inline
size_t _ring_buffer_get_data_available(struct ring_buffer *ring_buffer)
{
	int32_t avail_data =  ring_buffer->_write_offset - ring_buffer->_read_offset;
	/* wrap around ? 2*size because of "double area" */
	if (avail_data < 0)
		avail_data = 2 * ring_buffer->data_buffer_size + avail_data;

	return avail_data;
}

static size_t ring_buffer_get_data_available(struct sof_source *source)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_source(source);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	return _ring_buffer_get_data_available(ring_buffer);
}

static size_t ring_buffer_get_free_size(struct sof_sink *sink)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_sink(sink);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	return ring_buffer->data_buffer_size - _ring_buffer_get_data_available(ring_buffer);
}

static int ring_buffer_get_buffer(struct sof_sink *sink, size_t req_size,
				  void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_sink(sink);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	if (req_size > ring_buffer_get_free_size(sink))
		return -ENODATA;

	/* note, __sparse_force is to be removed once sink/src use __sparse_cache for data ptrs */
	*data_ptr = (__sparse_force void *)ring_buffer_get_pointer(ring_buffer,
								   ring_buffer->_write_offset);
	*buffer_start = (__sparse_force void *)ring_buffer->_data_buffer;
	*buffer_size = ring_buffer->data_buffer_size;

	/* no need to invalidate cache - buffer is to be written only */
	return 0;
}

static int ring_buffer_commit_buffer(struct sof_sink *sink, size_t commit_size)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_sink(sink);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	if (commit_size) {
		ring_buffer_writeback_shared(ring_buffer,
					     ring_buffer_get_pointer(ring_buffer,
								     ring_buffer->_write_offset),
					     commit_size);

		/* move write pointer */
		ring_buffer->_write_offset = ring_buffer_inc_offset(ring_buffer,
								    ring_buffer->_write_offset,
								    commit_size);
	}

	return 0;
}

static int ring_buffer_get_data(struct sof_source *source, size_t req_size,
				void const **data_ptr, void const **buffer_start,
				size_t *buffer_size)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_source(source);
	__sparse_cache void *data_ptr_c;

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	if (req_size > ring_buffer_get_data_available(source))
		return -ENODATA;

	data_ptr_c = ring_buffer_get_pointer(ring_buffer, ring_buffer->_read_offset);

	/* clean cache in provided data range */
	ring_buffer_invalidate_shared(ring_buffer, data_ptr_c, req_size);

	*buffer_start = (__sparse_force void *)ring_buffer->_data_buffer;
	*buffer_size = ring_buffer->data_buffer_size;
	*data_ptr = (__sparse_force void *)data_ptr_c;

	return 0;
}

static int ring_buffer_release_data(struct sof_source *source, size_t free_size)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_source(source);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	if (free_size) {
		/* data consumed, free buffer space, no need for any special cache operations */
		ring_buffer->_read_offset = ring_buffer_inc_offset(ring_buffer,
								   ring_buffer->_read_offset,
								   free_size);
	}

	return 0;
}

static int ring_buffer_set_ipc_params(struct ring_buffer *ring_buffer,
				      struct sof_ipc_stream_params *params,
				      bool force_update)
{
	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);

	if (audio_buffer_hw_params_configured(&ring_buffer->audio_buffer) && !force_update)
		return 0;

	struct sof_audio_stream_params *audio_stream_params =
		audio_buffer_get_stream_params(&ring_buffer->audio_buffer);

	audio_stream_params->frame_fmt = params->frame_fmt;
	audio_stream_params->rate = params->rate;
	audio_stream_params->channels = params->channels;
	audio_stream_params->buffer_fmt = params->buffer_fmt;

	audio_buffer_set_hw_params_configured(&ring_buffer->audio_buffer);

	return 0;
}

static int ring_buffer_set_ipc_params_source(struct sof_source *source,
					     struct sof_ipc_stream_params *params,
					     bool force_update)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_source(source);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	return ring_buffer_set_ipc_params(ring_buffer, params, force_update);
}

static int ring_buffer_set_ipc_params_sink(struct sof_sink *sink,
					   struct sof_ipc_stream_params *params,
					   bool force_update)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_sink(sink);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);
	return ring_buffer_set_ipc_params(ring_buffer, params, force_update);
}

static const struct source_ops ring_buffer_source_ops = {
	.get_data_available = ring_buffer_get_data_available,
	.get_data = ring_buffer_get_data,
	.release_data = ring_buffer_release_data,
	.audio_set_ipc_params = ring_buffer_set_ipc_params_source,
};

static const struct sink_ops ring_buffer_sink_ops = {
	.get_free_size = ring_buffer_get_free_size,
	.get_buffer = ring_buffer_get_buffer,
	.commit_buffer = ring_buffer_commit_buffer,
	.audio_set_ipc_params = ring_buffer_set_ipc_params_sink,
};

static const struct audio_buffer_ops audio_buffer_ops = {
	.free = ring_buffer_free,
};

struct ring_buffer *ring_buffer_create(size_t min_available, size_t min_free_space, bool is_shared,
				       uint32_t id)
{
	struct ring_buffer *ring_buffer;

	/* allocate ring_buffer structure */
	if (is_shared)
		ring_buffer = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
				      sizeof(*ring_buffer));
	else
		ring_buffer = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				      sizeof(*ring_buffer));
	if (!ring_buffer)
		return NULL;

	/* init base structure. The audio_stream_params is NULL because ring_buffer
	 * is currently used as a secondary buffer for DP only
	 *
	 * pointer in audio_buffer will be overwritten when attaching ring_buffer as a
	 * secondary buffer
	 */
	audio_buffer_init(&ring_buffer->audio_buffer, BUFFER_TYPE_RING_BUFFER,
			  is_shared, &ring_buffer_source_ops, &ring_buffer_sink_ops,
			  &audio_buffer_ops, NULL);

	/* set obs/ibs in sink/source interfaces */
	sink_set_min_free_space(audio_buffer_get_sink(&ring_buffer->audio_buffer),
				min_free_space);
	source_set_min_available(audio_buffer_get_source(&ring_buffer->audio_buffer),
				 min_available);

	uint32_t max_ibs_obs = MAX(min_available, min_free_space);

	/* calculate required buffer size */
	ring_buffer->data_buffer_size = 2 * max_ibs_obs;

	/* allocate data buffer - always in cached memory alias */
	ring_buffer->data_buffer_size =
			ALIGN_UP(ring_buffer->data_buffer_size, PLATFORM_DCACHE_ALIGN);
	ring_buffer->_data_buffer = (__sparse_force __sparse_cache void *)
			rballoc_align(0, 0, ring_buffer->data_buffer_size, PLATFORM_DCACHE_ALIGN);
	if (!ring_buffer->_data_buffer)
		goto err;

	tr_info(&ring_buffer_tr, "Ring buffer created, id: %u shared: %u min_available: %u min_free_space %u, size %u",
		id, ring_buffer_is_shared(ring_buffer), min_available, min_free_space,
		ring_buffer->data_buffer_size);

	/* return a pointer to allocated structure */
	return ring_buffer;
err:
	tr_err(&ring_buffer_tr, "Ring buffer creation failure");
	rfree(ring_buffer);
	return NULL;
}
