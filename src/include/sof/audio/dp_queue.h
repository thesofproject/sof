/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_DP_QUEUE_H__
#define __SOF_DP_QUEUE_H__

#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api_implementation.h>
#include <sof/audio/source_api_implementation.h>
#include <sof/audio/audio_stream.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <ipc/topology.h>
#include <sof/coherent.h>

/**
 * DP queue is a lockless circular buffer
 * providing safe consumer/producer cached operations cross cores
 *
 * prerequisites:
 *  1) incoming and outgoing data rate MUST be the same
 *  2) Both data consumer and data producer declare max chunk sizes they want to use (IBS/OBS)
 *
 * required Buffer size is 2*MAX(IBS,OBS) to allow free read/write in various data chunk sizes
 * and execution periods (of course in/out data rates must be same)
 * example:
 *  Consumer reads 5bytes each 3 cycles (IBS = 5)
 *  producer writes 3bytes every 5 cycles (OBS = 3)
 *    - cycle0 buffer empty, producer starting processing, consumer must wait
 *    - cycle3 produce 3 bytes (buf occupation = 3)
 *    - cycle6 produce 3 bytes (buf occupation = 6), consumer becomes ready
 *		in DP thread will start now - asyn to LL cycles
 *		in this example assuming it consumes data in next cycle
 *    - cycle7 consume 5 bytes, (buf occupation = 1)
 *    - cycle9 produce 3 bytes (buf occupation = 4)
 *    - cycle12 (producer goes first) produce 3 bytes (buf occupation = 7)
 *		consume 5 bytes (buf occupation = 2)
 *    - cycle15 produce 3 bytes (buf occupation = 5)
 *		consumer has enough data, but is busy processing prev data
 *    - cycle15 consume 5 bytes (buf occupation = 0)
 *
 * ===> max buf occupation = 7
 *
 * The worst case is when IBS=OBS and equal periods of consumer/producer
 * the buffer must be 2*MAX(IBS,OBS) as we do not know who goes first - consumer or producer,
 * especially when both are located on separate cores and EDF scheduling is used
 *
 *  Consumer reads 5 bytes every cycle (IBS = 5)
 *  producer writes 5 bytes every cycle (OBS = 5)
 *    - cycle0 consumer goes first - must wait (buf occupation = 0)
 *		producer produce 5 bytes (buf occupation = 5)
 *    - cycle1 producer goes first - produce 5 bytes (buf occupation = 10)
 *		consumer consumes 5 bytes (buf occupation = 5)
 * ===> max buf occupation = 10
 *
 *
 * The queue may work in 2 modes
 * 1) local mode
 *    in case both receiver and sender are located on the same core and cache coherency
 *    does not matter. dp_queue structure is located in cached memory
 *    In this case DP Queue is a simple ring buffer
 *
 * 2) shared mode
 *    In this case we need to writeback cache when new data arrive and invalidate cache on
 *    secondary core. dp_queue structure is located in shared memory
 *
 *
 * dpQueue is a lockless consumer/producer safe buffer. It is achieved by having only 2 shared
 * variables:
 *  _write_offset - can be modified by data producer only
 *  _read_offset - can be modified by data consumer only
 *
 *  as 32 bit operations are atomic, it is multi-thread and multi-core save
 *
 * There some explanation needed how free_space and available_data are calculated
 *
 * number of avail data in circular buffer may be calculated as:
 *	data_avail = _write_offset - _read_offset
 *   and check for wrap around
 *	if (data_avail < 0) data_avail = buffer_size - data_avail
 *
 * The problem is when _write_offset == _read_offset,
 * !!! it may mean either that the buffer is empty or the buffer is completely filled !!!
 *
 * To solve the above issue having only 2 variables mentioned before:
 *  - allow both offsets to point from 0 to DOUBLE buffer_size
 *  - when calculating pointers to data, use: data_bufer[offset % buffer_size]
 *  - use double buffer size in wrap around check when calculating available data
 *
 * And now:
 *   - _write_offset == _read_offset
 *		always means "buffer empty"
 *   - _write_offset == _read_offset + buffer_size
 *		always means "buffer full"
 */

struct dp_queue;
struct sof_audio_stream_params;

/* DP flags */
#define DP_QUEUE_MODE_LOCAL 0
#define DP_QUEUE_MODE_SHARED BIT(1)

/* the dpQueue structure */
struct dp_queue {
	CORE_CHECK_STRUCT_FIELD;

	/* public */
	struct list_item list;	/**< fields for connection queues in a list */

	/* public: read only */
	struct sof_audio_stream_params audio_stream_params;
	size_t data_buffer_size;

	/* private: */
	struct sof_source _source_api;  /**< src api handler */
	struct sof_sink _sink_api;      /**< sink api handler */

	uint32_t _flags;		/* DP_QUEUE_MODE_* */

	uint8_t __sparse_cache *_data_buffer;
	size_t _write_offset;		/* private: to be modified by data producer using API */
	size_t _read_offset;		/* private: to be modified by data consumer using API */

	bool _hw_params_configured;
};

/**
 *
 * @param min_available  minimum data available in queue required by the module using
 *			 dp_queue's source api
 * @param min_free_space minimum buffer space in queue required by the module using
 *			 dp_queue's sink api
 *
 * @param flags a combinatin of DP_QUEUE_MODE_* flags determining working mode
 *
 */
struct dp_queue *dp_queue_create(size_t min_available, size_t min_free_space, uint32_t flags);

/**
 * @brief remove the queue from the list, free dp queue memory
 */
static inline
void dp_queue_free(struct dp_queue *dp_queue)
{
	CORE_CHECK_STRUCT(dp_queue);
	list_item_del(&dp_queue->list);
	rfree((__sparse_force void *)dp_queue->_data_buffer);
	rfree(dp_queue);
}

/**
 * @brief return a handler to sink API of dp_queue.
 *		  the handler may be used by helper functions defined in sink_api.h
 */
static inline
struct sof_sink *dp_queue_get_sink(struct dp_queue *dp_queue)
{
	CORE_CHECK_STRUCT(dp_queue);
	return &dp_queue->_sink_api;
}

/**
 * @brief return a handler to source API of dp_queue
 *		  the handler may be used by helper functions defined in source_api.h
 */
static inline
struct sof_source *dp_queue_get_source(struct dp_queue *dp_queue)
{
	CORE_CHECK_STRUCT(dp_queue);
	return &dp_queue->_source_api;
}

/**
 * @brief this is a backdoor to get complete audio params structure from dp_queue
 *	  it is needed till pipeline 2.0 is ready
 *
 */
static inline
struct sof_audio_stream_params *dp_queue_get_audio_params(struct dp_queue *dp_queue)
{
	CORE_CHECK_STRUCT(dp_queue);
	return &dp_queue->audio_stream_params;
}

/**
 * @brief return true if the queue is shared between 2 cores
 */
static inline
bool dp_queue_is_shared(struct dp_queue *dp_queue)
{
	CORE_CHECK_STRUCT(dp_queue);
	return !!(dp_queue->_flags & DP_QUEUE_MODE_SHARED);
}

/**
 * @brief append a dp_queue to the list
 */
static inline void dp_queue_append_to_list(struct dp_queue *item, struct list_item *list)
{
	list_item_append(&item->list, list);
}

/**
 * @brief return a pointer to the first dp_queue on the list
 */
static inline struct dp_queue *dp_queue_get_first_item(struct list_item *list)
{
	return list_first_item(list, struct dp_queue, list);
}

/**
 * @brief return a pointer to the next dp_queue on the list
 */
static inline struct dp_queue *dp_queue_get_next_item(struct dp_queue *item)
{
	return list_next_item(item, list);
}

#endif /* __SOF_DP_QUEUE_H__ */
