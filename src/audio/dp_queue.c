// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//

#include <sof/common.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>

#include <sof/audio/dp_queue.h>

#include <rtos/alloc.h>
#include <ipc/topology.h>

LOG_MODULE_REGISTER(dp_queue, CONFIG_SOF_LOG_LEVEL);

/* 393608d8-4188-11ee-be56-0242ac120002 */
DECLARE_SOF_RT_UUID("dp_queue", dp_queue_uuid, 0x393608d8, 0x4188, 0x11ee,
		    0xbe, 0x56, 0x02, 0x42, 0xac, 0x12, 0x20, 0x02);
DECLARE_TR_CTX(dp_queue_tr, SOF_UUID(dp_queue_uuid), LOG_LEVEL_INFO);

static inline uint8_t __sparse_cache *dp_queue_buffer_end(struct dp_queue *dp_queue)
{
	return dp_queue->_data_buffer + dp_queue->data_buffer_size;
}

static inline struct dp_queue *dp_queue_from_sink(struct sof_sink *sink)
{
	return container_of(sink, struct dp_queue, _sink_api);
}

static inline struct dp_queue *dp_queue_from_source(struct sof_source *source)
{
	return container_of(source, struct dp_queue, _source_api);
}

static inline void dp_queue_invalidate_shared(struct dp_queue *dp_queue,
					      void __sparse_cache *ptr, size_t size)
{
	/* no cache required in case of not shared queue */
	if (!dp_queue_is_shared(dp_queue))
		return;

	/* wrap-around? */
	if ((uintptr_t)ptr + size > (uintptr_t)dp_queue_buffer_end(dp_queue)) {
		/* writeback till the end of circular buffer */
		dcache_invalidate_region
			(ptr, (uintptr_t)dp_queue_buffer_end(dp_queue) - (uintptr_t)ptr);
		size -= (uintptr_t)dp_queue_buffer_end(dp_queue) - (uintptr_t)ptr;
		ptr = dp_queue->_data_buffer;
	}
	/* invalidate rest of data */
	dcache_invalidate_region(ptr, size);
}

static inline void dp_queue_writeback_shared(struct dp_queue *dp_queue,
					     void __sparse_cache *ptr, size_t size)
{
	/* no cache required in case of not shared queue */
	if (!dp_queue_is_shared(dp_queue))
		return;

	/* wrap-around? */
	if ((uintptr_t)ptr + size > (uintptr_t)dp_queue_buffer_end(dp_queue)) {
		/* writeback till the end of circular buffer */
		dcache_writeback_region
			(ptr, (uintptr_t)dp_queue_buffer_end(dp_queue) - (uintptr_t)ptr);
		size -= (uintptr_t)dp_queue_buffer_end(dp_queue) - (uintptr_t)ptr;
		ptr = dp_queue->_data_buffer;
	}
	/* writeback rest of data */
	dcache_writeback_region(ptr, size);
}

static inline
uint8_t __sparse_cache *dp_queue_get_pointer(struct dp_queue *dp_queue, size_t offset)
{
	/* check if offset is not in "double area"
	 * lines below do a quicker version of offset %= dp_queue->data_buffer_size;
	 */
	if (offset >= dp_queue->data_buffer_size)
		offset -= dp_queue->data_buffer_size;
	return dp_queue->_data_buffer + offset;
}

static inline
size_t dp_queue_inc_offset(struct dp_queue *dp_queue, size_t offset, size_t inc)
{
	assert(inc <= dp_queue->data_buffer_size);
	offset += inc;
	/* wrap around ? 2*size because of "double area" */
	if (offset >= 2 * dp_queue->data_buffer_size)
		offset -= 2 * dp_queue->data_buffer_size;
	return offset;
}

static inline
size_t _dp_queue_get_data_available(struct dp_queue *dp_queue)
{
	int32_t avail_data =  dp_queue->_write_offset - dp_queue->_read_offset;
	/* wrap around ? 2*size because of "double area" */
	if (avail_data < 0)
		avail_data = 2 * dp_queue->data_buffer_size + avail_data;

	return avail_data;
}

static size_t dp_queue_get_data_available(struct sof_source *source)
{
	struct dp_queue *dp_queue = dp_queue_from_source(source);

	CORE_CHECK_STRUCT(dp_queue);
	return _dp_queue_get_data_available(dp_queue);
}

static size_t dp_queue_get_free_size(struct sof_sink *sink)
{
	struct dp_queue *dp_queue = dp_queue_from_sink(sink);

	CORE_CHECK_STRUCT(dp_queue);
	return dp_queue->data_buffer_size - _dp_queue_get_data_available(dp_queue);
}

static int dp_queue_get_buffer(struct sof_sink *sink, size_t req_size,
			       void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct dp_queue *dp_queue = dp_queue_from_sink(sink);

	CORE_CHECK_STRUCT(dp_queue);
	if (req_size > dp_queue_get_free_size(sink))
		return -ENODATA;

	/* note, __sparse_force is to be removed once sink/src use __sparse_cache for data ptrs */
	*data_ptr = (__sparse_force void *)dp_queue_get_pointer(dp_queue, dp_queue->_write_offset);
	*buffer_start = (__sparse_force void *)dp_queue->_data_buffer;
	*buffer_size = dp_queue->data_buffer_size;

	/* no need to invalidate cache - buffer is to be written only */
	return 0;
}

static int dp_queue_commit_buffer(struct sof_sink *sink, size_t commit_size)
{
	struct dp_queue *dp_queue = dp_queue_from_sink(sink);

	CORE_CHECK_STRUCT(dp_queue);
	if (commit_size) {
		dp_queue_writeback_shared(dp_queue,
					  dp_queue_get_pointer(dp_queue, dp_queue->_write_offset),
					  commit_size);

		/* move write pointer */
		dp_queue->_write_offset =
				dp_queue_inc_offset(dp_queue, dp_queue->_write_offset, commit_size);
	}

	return 0;
}

static int dp_queue_get_data(struct sof_source *source, size_t req_size,
			     void const **data_ptr, void const **buffer_start, size_t *buffer_size)
{
	struct dp_queue *dp_queue = dp_queue_from_source(source);
	__sparse_cache void *data_ptr_c;

	CORE_CHECK_STRUCT(dp_queue);
	if (req_size > dp_queue_get_data_available(source))
		return -ENODATA;

	data_ptr_c = dp_queue_get_pointer(dp_queue, dp_queue->_read_offset);

	/* clean cache in provided data range */
	dp_queue_invalidate_shared(dp_queue, data_ptr_c, req_size);

	*buffer_start = (__sparse_force void *)dp_queue->_data_buffer;
	*buffer_size = dp_queue->data_buffer_size;
	*data_ptr = (__sparse_force void *)data_ptr_c;

	return 0;
}

static int dp_queue_release_data(struct sof_source *source, size_t free_size)
{
	struct dp_queue *dp_queue = dp_queue_from_source(source);

	CORE_CHECK_STRUCT(dp_queue);
	if (free_size) {
		/* data consumed, free buffer space, no need for any special cache operations */
		dp_queue->_read_offset =
				dp_queue_inc_offset(dp_queue, dp_queue->_read_offset, free_size);
	}

	return 0;
}

static int dp_queue_set_ipc_params(struct dp_queue *dp_queue,
				   struct sof_ipc_stream_params *params,
				   bool force_update)
{
	CORE_CHECK_STRUCT(dp_queue);
	if (dp_queue->_hw_params_configured && !force_update)
		return 0;

	dp_queue->audio_stream_params.frame_fmt = params->frame_fmt;
	dp_queue->audio_stream_params.rate = params->rate;
	dp_queue->audio_stream_params.channels = params->channels;
	dp_queue->audio_stream_params.buffer_fmt = params->buffer_fmt;

	dp_queue->_hw_params_configured = true;

	return 0;
}

static int dp_queue_set_ipc_params_source(struct sof_source *source,
					  struct sof_ipc_stream_params *params,
					  bool force_update)
{
	struct dp_queue *dp_queue = dp_queue_from_source(source);

	CORE_CHECK_STRUCT(dp_queue);
	return dp_queue_set_ipc_params(dp_queue, params, force_update);
}

static int dp_queue_set_ipc_params_sink(struct sof_sink *sink,
					struct sof_ipc_stream_params *params,
					bool force_update)
{
	struct dp_queue *dp_queue = dp_queue_from_sink(sink);

	CORE_CHECK_STRUCT(dp_queue);
	return dp_queue_set_ipc_params(dp_queue, params, force_update);
}

static const struct source_ops dp_queue_source_ops = {
	.get_data_available = dp_queue_get_data_available,
	.get_data = dp_queue_get_data,
	.release_data = dp_queue_release_data,
	.audio_set_ipc_params = dp_queue_set_ipc_params_source,
};

static const struct sink_ops dp_queue_sink_ops = {
	.get_free_size = dp_queue_get_free_size,
	.get_buffer = dp_queue_get_buffer,
	.commit_buffer = dp_queue_commit_buffer,
	.audio_set_ipc_params = dp_queue_set_ipc_params_sink,
};

struct dp_queue *dp_queue_create(size_t min_available, size_t min_free_space, uint32_t flags)
{
	struct dp_queue *dp_queue;

	/* allocate DP structure */
	if (flags & DP_QUEUE_MODE_SHARED)
		dp_queue = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
				   sizeof(*dp_queue));
	else
		dp_queue = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*dp_queue));
	if (!dp_queue)
		return NULL;

	dp_queue->_flags = flags;

	CORE_CHECK_STRUCT_INIT(dp_queue, flags & DP_QUEUE_MODE_SHARED);

	/* initiate structures */
	source_init(dp_queue_get_source(dp_queue), &dp_queue_source_ops,
		    &dp_queue->audio_stream_params);
	sink_init(dp_queue_get_sink(dp_queue), &dp_queue_sink_ops,
		  &dp_queue->audio_stream_params);

	list_init(&dp_queue->list);

	/* set obs/ibs in sink/source interfaces */
	sink_set_min_free_space(&dp_queue->_sink_api, min_free_space);
	source_set_min_available(&dp_queue->_source_api, min_available);

	uint32_t max_ibs_obs = MAX(min_available, min_free_space);

	/* calculate required buffer size */
	dp_queue->data_buffer_size = 2 * max_ibs_obs;

	/* allocate data buffer - always in cached memory alias */
	dp_queue->data_buffer_size = ALIGN_UP(dp_queue->data_buffer_size, PLATFORM_DCACHE_ALIGN);
	dp_queue->_data_buffer = (__sparse_force __sparse_cache void *)
			rballoc_align(0, 0, dp_queue->data_buffer_size, PLATFORM_DCACHE_ALIGN);
	if (!dp_queue->_data_buffer)
		goto err;

	tr_info(&dp_queue_tr, "DpQueue created, shared: %u min_available: %u min_free_space %u, size %u",
		dp_queue_is_shared(dp_queue), min_available, min_free_space,
		dp_queue->data_buffer_size);

	/* return a pointer to allocated structure */
	return dp_queue;
err:
	tr_err(&dp_queue_tr, "DpQueue creation failure");
	rfree(dp_queue);
	return NULL;
}
