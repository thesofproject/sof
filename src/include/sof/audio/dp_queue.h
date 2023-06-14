/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_DP_QUEUE_H__
#define __SOF_DP_QUEUE_H__

#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <rtos/bit.h>

/**
 * DP queue is a circular buffer providing safe consumer/producer cached operations cross cores
 *
 * prerequisites:
 *  1) incoming and outgoing data rate MUST be the same
 *  2) Both data consumer and data producer declare max chunk sizes they want to use (IBS/OBS)
 *     any requests of bigger data than IBS/OBS will be rejected
 *
 * The queue may work in 4 modes
 * 1) simple mode
 *    in case both receiver and sender are located on the same core and cache coherency
 *    does not matter.
 *    In this case DP Queue is a simple ring buffer, with IBS and OBS
 *    Buffer size must be:
 *	- 2*MAX(IBS,OBS) if IBS(obs) is a multiplication of OBS(IBS)
 *	- 3*MAX(IBS,OBS) otherwise
 *
 * 2) shared mode
 *    In this case we need to writeback cache when new data arrive and invalidate cache on
 *    secondary core. That means the whole cacheline must be used exclusively by sink or by source
 *    incoming data will be available to use when a cacheline is filled completely
 *
 *    buffer size is always 3*MAX(IBS,OBS,CACHELINE)
 *
 *
 * 3) linear mode (TODO)
 *    In this mode the data are organized linearly
 *    That implies one data copy operation inside the queue
 *
 * 4) linear shared mode (TODO)
 *    In this mode the data are organized linearly and shared between cores
 *    That implies one data copy operation
 *
 * DpQueue itself is a cross core structure, it must be allocated and used in non-cached memory
 *
 * TODO: audio_set_ipc_params
 *	set_alignment_constants
 *	linear mode
 *
 */

struct dp_queue;
struct sof_audio_stream_params;

#define DP_QUEUE_MODE_SIMPLE 0
#define DP_QUEUE_MODE_SHARED BIT(1)
#define DP_QUEUE_MODE_LINEAR BIT(2)

/**
 * @param ibs input buffer size
 *		the size of data to be produced in 1 cycle
 *		the data producer declares here how much data it will produce in single cycle
 *		it is enforced - the prouder CANNOT request more space in single cycle
 *		it may, however, request less
 *
 * @param obs output buffer size
 *		the size of data to be consumed in 1 cycle
 *		the data receiver declares here how much data it will consume in single cycle
 *		it is enforced - the consumer CANNOT request more data in single cycle
 *		it may, however, request less
 *
 * @param flags a combinatin of DP_QUEUE_MODE_* flags determining working mode
 *
 * @param external_stream_params
 *		this is a trick needed till pipeline 2.0 is introduced
 *		currently module adapter must copy from audio_stream to dp_queue
 *		both objects must have identical audio parameters
 *		To keep both consistent, dp_queue does not keep its own set of parameters, but
 *		links directly to corresponding audio_stream.
 *		Note that this link MUST be an uncached alias
 */
struct dp_queue *dp_queue_create(uint32_t ibs, uint32_t obs, uint32_t flags,
				 struct sof_audio_stream_params *external_stream_params);
/**
 * @brief free dp queue memory
 */
void dp_queue_free(struct dp_queue *dp_queue);

/**
 * @brief return a handler to sink API of dp_queue.
 *		  the handler may be used by helper functions defined in sink_api.h
 */
struct sof_sink __sparse_cache *dp_queue_get_sink(struct dp_queue *dp_queue);

/**
 * @brief return a handler to source API of dp_queue
 *		  the handler may be used by helper functions defined in source_api.h
 */
struct sof_source __sparse_cache *dp_queue_get_source(struct dp_queue *dp_queue);

/* get handler to dp_queue from sink/source handlers
 */
struct dp_queue *dp_queue_get_from_sink(struct sof_sink __sparse_cache *sink);
struct dp_queue *dp_queue_get_from_source(struct sof_source __sparse_cache *source);

#endif /* __SOF_DP_QUEUE_H__ */
