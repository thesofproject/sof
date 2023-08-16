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
 * required Buffer size:
 *	- 2*MAX(IBS,OBS) if the larger of IBS/OBS is multiplication of smaller
 *	- 3*MAX(IBS,OBS) otherwise
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

	/* public: read only */
	struct sof_audio_stream_params audio_stream_params;
	size_t data_buffer_size;

	/* private: */
	struct sof_source _source_api;  /**< src api handler */
	struct sof_sink _sink_api;      /**< sink api handler */

	uint32_t _flags;		/* DP_QUEUE_MODE_* */

	uint8_t __sparse_cache *_data_buffer;
	uint32_t _write_offset;		/* private: to be modified by data producer using API */
	uint32_t _read_offset;		/* private: to be modified by data consumer using API */

	bool _hw_params_configured;
};

/**
 * @param ibs input buffer size
 *		the size of data to be produced in 1 cycle
 *		the data producer declares here how much data it will produce in single cycle
 *
 * @param obs output buffer size
 *		the size of data to be consumed in 1 cycle
 *		the data receiver declares here how much data it will consume in single cycle
 *
 * @param flags a combinatin of DP_QUEUE_MODE_* flags determining working mode
 *
 */
struct dp_queue *dp_queue_create(size_t ibs, size_t obs, uint32_t flags);

/**
 * @brief free dp queue memory
 */
static inline
void dp_queue_free(struct dp_queue *dp_queue)
{
	CORE_CHECK_STRUCT(dp_queue);
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

#endif /* __SOF_DP_QUEUE_H__ */
