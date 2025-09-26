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


/**
 * @brief remove the queue from the list, free memory
 */
static void ring_buffer_free(struct sof_audio_buffer *audio_buffer)
{
	if (!audio_buffer)
		return;

	struct ring_buffer *ring_buffer =
			container_of(audio_buffer, struct ring_buffer, audio_buffer);

	sof_heap_free(audio_buffer->heap, (__sparse_force void *)ring_buffer->_data_buffer);
	sof_heap_free(audio_buffer->heap, ring_buffer);
}

static void ring_buffer_reset(struct sof_audio_buffer *audio_buffer)
{
	struct ring_buffer *ring_buffer =
			container_of(audio_buffer, struct ring_buffer, audio_buffer);

	ring_buffer->_write_offset = 0;
	ring_buffer->_read_offset = 0;

	ring_buffer_invalidate_shared(ring_buffer, ring_buffer->_data_buffer,
				      ring_buffer->data_buffer_size);
	bzero((__sparse_force void *)ring_buffer->_data_buffer, ring_buffer->data_buffer_size);
	ring_buffer_writeback_shared(ring_buffer, ring_buffer->_data_buffer,
				     ring_buffer->data_buffer_size);
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

int ring_buffer_module_unbind(struct sof_sink *sink)
{
	struct ring_buffer *ring_buffer = ring_buffer_from_sink(sink);

	CORE_CHECK_STRUCT(&ring_buffer->audio_buffer);

	/* in case of disconnection, invalidate all cache. This method is guaranteed be called on
	 * core that have been using sink API
	 */
	ring_buffer_invalidate_shared(ring_buffer, ring_buffer->_data_buffer,
				      ring_buffer->data_buffer_size);

	return 0;
}

static const struct source_ops ring_buffer_source_ops = {
	.get_data_available = ring_buffer_get_data_available,
	.get_data = ring_buffer_get_data,
	.release_data = ring_buffer_release_data,
	.audio_set_ipc_params = audio_buffer_source_set_ipc_params,
	.on_audio_format_set = audio_buffer_source_on_audio_format_set,
	.set_alignment_constants = audio_buffer_source_set_alignment_constants,
};

static const struct sink_ops ring_buffer_sink_ops = {
	.get_free_size = ring_buffer_get_free_size,
	.get_buffer = ring_buffer_get_buffer,
	.commit_buffer = ring_buffer_commit_buffer,
	.on_unbind = ring_buffer_module_unbind,
	.audio_set_ipc_params = audio_buffer_sink_set_ipc_params,
	.on_audio_format_set = audio_buffer_sink_on_audio_format_set,
	.set_alignment_constants = audio_buffer_sink_set_alignment_constants,
};

static const struct audio_buffer_ops audio_buffer_ops = {
	.free = ring_buffer_free,
	.reset = ring_buffer_reset
};

struct ring_buffer *ring_buffer_create(struct k_heap *heap, size_t min_available,
				       size_t min_free_space, bool is_shared, uint32_t id)
{
	uint32_t flags = is_shared ? SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT : SOF_MEM_FLAG_USER;
	struct ring_buffer *ring_buffer = sof_heap_alloc(heap, flags, sizeof(*ring_buffer), 0);

	if (!ring_buffer)
		return NULL;

	memset(ring_buffer, 0, sizeof(*ring_buffer));

	/* init base structure. The audio_stream_params is NULL because ring_buffer
	 * is currently used as a secondary buffer for DP only
	 *
	 * pointer in audio_buffer will be overwritten when attaching ring_buffer as a
	 * secondary buffer
	 */
	audio_buffer_init(&ring_buffer->audio_buffer, BUFFER_TYPE_RING_BUFFER,
			  is_shared, &ring_buffer_source_ops, &ring_buffer_sink_ops,
			  &audio_buffer_ops, NULL);
	ring_buffer->audio_buffer.heap = heap;

	/* set obs/ibs in sink/source interfaces */
	sink_set_min_free_space(audio_buffer_get_sink(&ring_buffer->audio_buffer),
				min_free_space);
	source_set_min_available(audio_buffer_get_source(&ring_buffer->audio_buffer),
				 min_available);

	uint32_t max_ibs_obs = MAX(min_available, min_free_space);

	/* Calculate required buffer size. This buffer must hold at least three times max_ibs_obs
	 * bytes to avoid starving the DP modules that process different block sizes in different
	 * periods (44.1 kHz case).
	 *
	 * The following example consists of one pipeline processed on core 0. One of the modules
	 * of this pipeline is a DP module, which processing is performed on core 1.
	 * DP module threads are started after LL processing on a given core is completed.
	 *
	 * An example of a DP module is src lite, which converts a 32-bit stereo stream with
	 * a sampling rate of 44.1 kHz to a rate of 16 kHz. With a period of 1 ms, the module should
	 * process 44.1 frames. In reality, this module processes 42 frames (336 bytes) for
	 * 9 periods and then processes 63 frames (504 bytes) for the tenth period. In this way,
	 * on average, over 10 periods, the module processes 44.1 frames.
	 *
	 * Consider the case of a 768-byte buffer (max_ibs_obs = 384) that is completely filled
	 * with samples. In the first period, the module consumes 336 bytes. The buffer is not
	 * replenished because the consumption occurred in the DP cycle, which is executed after
	 * the LL cycle. Data is transferred between modules in the pipeline during the LL cycle.
	 * The figure below shows the next period of the ongoing processing.
	 *
	 *	/-----------------------------------------------------------------\
	 *	| Core 0 | [              LL 0    (B)         ] [ DP 0 ]          |
	 *	|------------------------------------------------------------------
	 *	| Core 1 | [ LL 1 ] (A) [         DP 1         ] (C)              |
	 *	\-----------------------------------------------------------------/
	 *
	 * A.	The DP module on core 1 has 432 bytes available in the input buffer, so it can only
	 *	process a block of 336 bytes. It is unable to process a block of 502 bytes.
	 *	The module starts processing data.
	 *
	 * B.	Pipeline processing on core 0 reaches the point where more data is delivered to
	 *	the DP module source buffer, and processed data is received from the sink buffer.
	 *	The source buffer has 336 free bytes, and that is how many are copied.
	 *
	 * C.	The DP module finishes processing data and flushes 336 bytes from the source buffer.
	 *	Since this buffer has just been completely filled, there are again left only
	 *	432 bytes available at the end of the period.
	 *
	 * As shown in the above example, the DP module will eventually be starved, causing a glitch
	 * in the output signal. To resolve this situation and allow the module to process
	 * correctly, it is necessary to allocate a buffer three times larger than max_ibs_obs.
	 */
	ring_buffer->data_buffer_size = 3 * max_ibs_obs;

	/* allocate data buffer - always in cached memory alias */
	ring_buffer->data_buffer_size = ALIGN_UP(ring_buffer->data_buffer_size,
						 PLATFORM_DCACHE_ALIGN);
	ring_buffer->_data_buffer = (__sparse_force __sparse_cache void *)
		sof_heap_alloc(heap, SOF_MEM_FLAG_USER, ring_buffer->data_buffer_size,
			       PLATFORM_DCACHE_ALIGN);
	if (!ring_buffer->_data_buffer)
		goto err;

	tr_info(&ring_buffer_tr, "Ring buffer created, id: %u shared: %u min_available: %u min_free_space %u, size %u",
		id, ring_buffer_is_shared(ring_buffer), min_available, min_free_space,
		ring_buffer->data_buffer_size);

	/* return a pointer to allocated structure */
	return ring_buffer;
err:
	tr_err(&ring_buffer_tr, "Ring buffer creation failure");
	sof_heap_free(heap, ring_buffer);
	return NULL;
}
