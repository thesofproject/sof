/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SINK_API_H__
#define __SOF_SINK_API_H__

#include <sof/common.h>
#include <sof/audio/format.h>
#include <errno.h>

/**
 * this is a definition of API to sink of audio data
 *
 * THE SINK is any component that can store data somehow and provide a buffer to be filled
 * with data at request. The sink API does not define how the data will be processed/used
 *
 * The user - a module - sees this API as a destination it must send data to
 * The IMPLEMENTATION - audio_stream, DP Queue - sees this as a producer that
 *			PROVIDES data for processing
 *
 * Examples of components that should expose SINK api
 * - /dev/null
 *     all the data stored in sink buffer are just simply discarded
 * - I2S sender
 *     Data stored in sink buffer will be sent to the external world
 * - a memory ring buffer
 *     data stored in the buffer will be sent to another module (usually using source API, but it
 *     does not matter in fact).
 *
 *  The main advantage of using sink API instead of just taking pointers to the buffers is that
 *  the buffer may be prepared at the moment the data producer is requesting it. i.e.
 *   - cache may be written back/invalidated if necessary
 *   - data may be moved to make linear space
 *   - part of the buffer may be locked to prevent reading
 *   etc.etc. it depends on implementation of the data sink
 *
 * NOTE: the module should get a complete portion of space it needs for processing, fill it
 *       than release. The reason is - the depending on the implementation, the calls may be
 *       expensive - may involve some data moving in memory, cache writebacks, etc.
 *
 * Multicore:
 *  single sink is intended to be used by a single component only, so all calls to it
 *  may safely be performed using a cached pointer alias. All data modified by sink helper are
 *  local and specific to a single sink only
 *  The implementation, however, must take into account all cache coherence precautions
 */

/** definition of obfsfucated handler of sink API */
struct sink;

/**
 * Retrieves size of free space available in sink (in bytes)
 * return number of free bytes in buffer available to immediate filling
 */
size_t sink_get_free_size(struct sink __sparse_cache *sink);

/**
 * Retrieves size of free space available in sink (in frames)
 * return number of free bytes in buffer available to immediate filling
 */
size_t sink_get_free_frames(struct sink __sparse_cache *sink);

/**
 * Get a circular buffer to operate on (to write).
 *
 * Retrieves a fragment of circular data to be used by the caller
 * After calling get_buffer, the space for data is guaranteed to be available
 * for exclusive use on the caller core through cached pointer
 * The caller MUST take care of data circularity based on provided pointers
 *
 * @param sink a handler to sink
 * @param [in] req_size requested size of space
 * @param [out] data_ptr a pointer to the space will be provided there
 * @param [out] buffer_start pointer to circular buffer start
 * @param [out] buffer_size size of circular buffer
 *
 * @retval -ENODATA if req_size is bigger than free space
 *
 */
int sink_get_buffer(struct sink __sparse_cache *sink, size_t req_size,
		    void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
		    size_t *buffer_size);

/**
 * Commits that the buffer previously obtained by get_buffer is filled with data
 * and ready to be used
 *
 * @param sink a handler to sink
 * @param commit_size amount of data that the caller declares as valid
 *	  if commit_size is bigger than the amount of data obtained before by get_buffer(), only
 *	  the amount obtained before will be committed. That means - if somebody obtained a buffer,
 *	  filled it with data and wants to commit it in whole, it may simple call
 *	  commit_buffer with commit_size==MAXINT
 * @return proper error code (0  on success)
 */
int sink_commit_buffer(struct sink __sparse_cache *sink, size_t commit_size);

/**
 * Get total number of bytes processed by the sink (meaning - committed by sink_commit_buffer())
 *
 * @param sink a handler to sink
 * @return total number of processed data
 */
size_t sink_get_num_of_processed_bytes(struct sink __sparse_cache *sink);

/**
 * sets counter of total number of bytes processed  to zero
 */
void sink_reset_num_of_processed_bytes(struct sink __sparse_cache *sink);

/** get size of a single audio frame (in bytes) */
size_t sink_get_frame_bytes(struct sink __sparse_cache *sink);

/** set of functions for retrieve audio parameters */
enum sof_ipc_frame sink_get_frm_fmt(struct sink __sparse_cache *sink);
enum sof_ipc_frame sink_get_valid_fmt(struct sink __sparse_cache *sink);
uint32_t sink_get_rate(struct sink __sparse_cache *sink);
uint32_t sink_get_channels(struct sink __sparse_cache *sink);
bool sink_get_overrun(struct sink __sparse_cache *sink);

/** set of functions for setting audio parameters */
void sink_set_frm_fmt(struct sink __sparse_cache *sink, enum sof_ipc_frame frame_fmt);
void sink_set_valid_fmt(struct sink __sparse_cache *sink, enum sof_ipc_frame valid_sample_fmt);
void sink_set_rate(struct sink __sparse_cache *sink, unsigned int rate);
void sink_set_channels(struct sink __sparse_cache *sink, unsigned int channels);
void sink_set_overrun(struct sink __sparse_cache *sink, bool overrun_permitted);

#endif /* __SOF_SINK_API_H__ */
