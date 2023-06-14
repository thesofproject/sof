// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//

#include <sof/audio/dp_queue.h>
#include <sof/audio/sink_api_implementation.h>
#include <sof/audio/source_api_implementation.h>
#include <sof/audio/audio_stream.h>

#include <rtos/alloc.h>
#include <ipc/topology.h>

/**
 * sink and source structures are cached and belong exclusively to a single core
 * they must not be a part of coherent dp_queue structure
 */
struct dp_queue_sink_source_c {
	struct dp_queue *owner; /**< pointer to uncached dp_queue structure */
	struct sof_source source;	/**< src api handler */
	struct sof_sink sink;	/**< sink api handler */
};

struct dp_queue {
	/* sink/src structures, located in cached memory */
	struct dp_queue_sink_source_c __sparse_cache *sink_src_c;

	uint32_t ibs;
	uint32_t obs;
	uint32_t flags;

	size_t data_buffer_size;
	uint8_t *data_buffer;

	uint32_t write_offset;
	uint32_t read_offset;
	uint32_t available_data; /* amount of data ready for immediate reading */
	/* free buffer space
	 * NOTE!
	 *  - when dp queue is shared, available_data + free_space DOES NOT eq data_buffer_size
	 *  - when dp queue is not shared available_data + free_space always == data_buffer_size
	 */
	uint32_t free_space;

	struct sof_audio_stream_params audio_stream_params;

	struct k_spinlock lock;
};

static inline bool dp_queue_is_shared(struct dp_queue *dp_queue)
{
	return !!(dp_queue->flags & DP_QUEUE_MODE_SHARED);
}

static inline uint8_t *dp_queue_buffer_end(struct dp_queue *dp_queue)
{
	return dp_queue->data_buffer + dp_queue->data_buffer_size;
}

struct dp_queue *dp_queue_get_from_sink(struct sof_sink __sparse_cache *sink)
{
	struct dp_queue_sink_source_c __sparse_cache *sink_src_c =
		attr_container_of(sink, struct dp_queue_sink_source_c __sparse_cache,
				  sink, __sparse_cache);

	return sink_src_c->owner;
}

struct dp_queue *dp_queue_get_from_source(struct sof_source __sparse_cache *source)
{
	struct dp_queue_sink_source_c __sparse_cache *sink_src_c =
		attr_container_of(source, struct dp_queue_sink_source_c __sparse_cache,
				  source, __sparse_cache);
	return sink_src_c->owner;
}

static inline void dp_queue_invalidate_shared(struct dp_queue *dp_queue,
					      void __sparse_cache *ptr, uint32_t size)
{
	/* rollback? */
	if ((uintptr_t)ptr + size > (uintptr_t)dp_queue_buffer_end(dp_queue)) {
		/* writeback till the end of circular buffer */
		dcache_invalidate_region
			(ptr, (uintptr_t)dp_queue_buffer_end(dp_queue) - (uintptr_t)ptr);
		size -= (uintptr_t)dp_queue_buffer_end(dp_queue) - (uintptr_t)ptr;
	}
	/* invalidate rest of data - from begin of buffer, remaining size */
	dcache_invalidate_region((__sparse_force void __sparse_cache *)dp_queue->data_buffer, size);
}

static inline k_spinlock_key_t dp_queue_lock(struct dp_queue *dp_queue)
{
	return k_spin_lock(&dp_queue->lock);
}

static inline void dp_queue_unlock(struct dp_queue *dp_queue, k_spinlock_key_t key)
{
	k_spin_unlock(&dp_queue->lock, key);
}

struct sof_sink __sparse_cache *dp_queue_get_sink(struct dp_queue *dp_queue)
{
	return &dp_queue->sink_src_c->sink;
}

struct sof_source __sparse_cache *dp_queue_get_source(struct dp_queue *dp_queue)
{
	return &dp_queue->sink_src_c->source;
}

static size_t dp_queue_get_free_size(struct sof_sink __sparse_cache *sink)
{
	struct dp_queue *dp_queue = dp_queue_get_from_sink(sink);

	return dp_queue->free_space;
}

static int dp_queue_get_buffer(struct sof_sink __sparse_cache *sink, size_t req_size,
			       void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct dp_queue *dp_queue = dp_queue_get_from_sink(sink);

	/* dp_queue_get_free_size will return free size with adjustment for cacheline if needed */
	if (req_size > dp_queue->ibs || req_size > dp_queue_get_free_size(sink))
		return -ENODATA;

	/* no need to lock, just reading data that may be modified by commit_buffer only */
	*data_ptr = dp_queue->data_buffer + dp_queue->write_offset;
	*buffer_start = dp_queue->data_buffer;
	*buffer_size = dp_queue->data_buffer_size;

	/* provided buffer is an empty space, the requestor will perform write operations only
	 * no need to invalidate cache - will be overwritten anyway
	 */
	return 0;
}

static int dp_queue_commit_buffer(struct sof_sink __sparse_cache *sink, size_t commit_size)
{
	struct dp_queue *dp_queue = dp_queue_get_from_sink(sink);

	if (commit_size) {
		k_spinlock_key_t key = dp_queue_lock(dp_queue);

		if (dp_queue_is_shared(dp_queue)) {
			/* a shared queue. We need to go through committed cachelines one-by-one
			 * and if the whole cacheline is committed - writeback cache
			 * and mark data as available for reading
			 *
			 * first, calculate the current and last committed cacheline
			 * as offsets from buffer start
			 */
			uint32_t current_cacheline = dp_queue->write_offset /
						     PLATFORM_DCACHE_ALIGN;

			/* Last used cacheline may not be filled completely, calculate cacheline
			 * containing 1st free byte
			 */
			uint32_t last_cacheline = (dp_queue->write_offset + commit_size + 1) /
						   PLATFORM_DCACHE_ALIGN;
			uint32_t total_num_of_cachelines = dp_queue->data_buffer_size /
							   PLATFORM_DCACHE_ALIGN;
			/* overlap? */
			last_cacheline %= total_num_of_cachelines;

			/* now go one by one.
			 * if current_cacheline == last_full_cacheline - nothing to do
			 */
			while (current_cacheline != last_cacheline) {
				/* writeback / invalidate */
				uint8_t *ptr = dp_queue->data_buffer +
						(current_cacheline * PLATFORM_DCACHE_ALIGN);
				dcache_writeback_invalidate_region
					((__sparse_force void __sparse_cache *)ptr,
					PLATFORM_DCACHE_ALIGN);

				/* mark data as available to read */
				dp_queue->available_data += PLATFORM_DCACHE_ALIGN;
				/* get next cacheline */
				current_cacheline = (current_cacheline + 1) %
						    total_num_of_cachelines;
			}
		} else {
			/* not shared */
			dp_queue->available_data += commit_size;
		}

		/* move write pointer */
		dp_queue->free_space -= commit_size;
		dp_queue->write_offset = (dp_queue->write_offset + commit_size) %
					  dp_queue->data_buffer_size;

		dp_queue_unlock(dp_queue, key);
	}

	return 0;
}

static size_t dp_queue_get_data_available(struct sof_source __sparse_cache *source)
{
	struct dp_queue *dp_queue = dp_queue_get_from_source(source);

	/* access is read only, using uncached alias, no need to lock */

	return dp_queue->available_data;
}

static int dp_queue_get_data(struct sof_source __sparse_cache *source, size_t req_size,
			     void **data_ptr, void **buffer_start, size_t *buffer_size)
{
	struct dp_queue *dp_queue = dp_queue_get_from_source(source);

	/* no need to lock, just reading data */
	if (req_size > dp_queue->obs || req_size > dp_queue_get_data_available(source))
		return -ENODATA;

	*buffer_start = dp_queue->data_buffer;
	*buffer_size = dp_queue->data_buffer_size;
	*data_ptr = dp_queue->data_buffer + dp_queue->read_offset;

	/* clean cache in provided data range */
	if (dp_queue_is_shared(dp_queue))
		dp_queue_invalidate_shared(dp_queue,
					  (__sparse_force void __sparse_cache *)*data_ptr,
					  req_size);

	return 0;
}

static int dp_queue_release_data(struct sof_source __sparse_cache *source, size_t free_size)
{
	struct dp_queue *dp_queue = dp_queue_get_from_source(source);

	if (free_size) {
		/* data consumed, free buffer space, no need for any special cache operations */
		k_spinlock_key_t key = dp_queue_lock(dp_queue);

		dp_queue->available_data -= free_size;
		dp_queue->free_space += free_size;
		dp_queue->read_offset =
			(dp_queue->read_offset + free_size) % dp_queue->data_buffer_size;

		dp_queue_unlock(dp_queue, key);
	}

	return 0;
}

static const struct source_ops dp_queue_source_ops = {
	.get_data_available = dp_queue_get_data_available,
	.get_data = dp_queue_get_data,
	.release_data = dp_queue_release_data,
};

static const struct sink_ops dp_queue_sink_ops = {
	.get_free_size = dp_queue_get_free_size,
	.get_buffer = dp_queue_get_buffer,
	.commit_buffer = dp_queue_commit_buffer,
};

struct dp_queue *dp_queue_create(uint32_t ibs, uint32_t obs, uint32_t flags,
				 struct sof_audio_stream_params *external_stream_params)
{
	uint32_t align;
	struct dp_queue *dp_queue;

	/* allocate dp queue as a shared sctructure, regardless of shared/non shared flag
	 * it may be used by dp sheduler which always works on core 0
	 */
	dp_queue = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dp_queue));
	if (!dp_queue)
		return NULL;

	dp_queue->flags = flags;
	/* allocate sink/source structures as a cached data */
	dp_queue->sink_src_c = (__sparse_force void __sparse_cache *)
		rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*dp_queue->sink_src_c));

	if (!dp_queue->sink_src_c)
		goto out;

	/* initiate sink/source provide uncached pointers to audio_stream_params */
	source_init(dp_queue_get_source(dp_queue), &dp_queue_source_ops, external_stream_params);
	sink_init(dp_queue_get_sink(dp_queue), &dp_queue_sink_ops, external_stream_params);

	dp_queue->sink_src_c->owner = dp_queue;

	/* calculate required buffer size */
	if (dp_queue_is_shared(dp_queue)) {
		ibs = MAX(ibs, PLATFORM_DCACHE_ALIGN);
		obs = MAX(obs, PLATFORM_DCACHE_ALIGN);
		align = PLATFORM_DCACHE_ALIGN;
		dp_queue->data_buffer_size = 3 * MAX(ibs, obs);
	} else {
		if ((ibs % obs == 0) && (obs % ibs == 0))
			dp_queue->data_buffer_size = 2 * MAX(ibs, obs);
		else
			dp_queue->data_buffer_size = 3 * MAX(ibs, obs);
		align = 0;
	}
	dp_queue->ibs = ibs;
	dp_queue->obs = obs;
	dp_queue->available_data = 0;
	dp_queue->free_space = dp_queue->data_buffer_size;

	/* allocate data buffer in cached memory */
	dp_queue->data_buffer = rballoc_align(0, 0, dp_queue->data_buffer_size, align);
	if (!dp_queue->data_buffer)
		goto out;

	/* return allocated structure */
	return dp_queue;
out:
	rfree((__sparse_force void *)dp_queue->sink_src_c);
	rfree(dp_queue);
	return NULL;
}

void dp_queue_free(struct dp_queue *dp_queue)
{
	dp_queue_invalidate_shared(dp_queue,
				   (__sparse_force void __sparse_cache *)dp_queue->data_buffer,
				   dp_queue->data_buffer_size);

	rfree(dp_queue->data_buffer);
	rfree((__sparse_force void *)dp_queue->sink_src_c);
	rfree(dp_queue);
}
