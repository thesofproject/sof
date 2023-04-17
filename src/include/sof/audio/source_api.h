/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SOURCE_API_H__
#define __SOF_SOURCE_API_H__

#include <sof/common.h>
#include <sof/audio/format.h>
#include <errno.h>

/**
 * this is a definition of API to source of audio data
 *
 * THE SOURCE is any component in the system that have data stored somehow and can give the
 * data outside at request. The source API does not define who and how has produced the data
 *
 * The user - a module - sees this as a producer that PROVIDES data for processing
 * The IMPLEMENTATION - audio_stream, DP Queue - sees this API as a destination it must send data to
 * *
 * Examples of components that should expose source API:
 *  - DMIC
 *	Data are coming from the outside world, stores in tmp buffer and can be presented
 *	to the rest of the system using source_api
 *  - a memory ring buffer
 *	Data are coming from other module (usually using sink_api, but it does not matter in fact)
 *
 *  The main advantage of using source API instead of just taking pointers to the data is that
 *  the data may be prepared at the moment the data receiver is requesting it. i.e.
 *   - cache may be written back/invalidated if necessary
 *   - data may be moved from circular to linear space
 *   - part of the buffer may be locked to prevent writing
 *   etc.etc. it depends on implementation of the data source
 *
 * Data in general are provided as a circular buffer and the data receiver should be able to
 * deal with it. Of course if needed an implementation of source providing linear data can be
 * implemented and used as a mid-layer for modules needing it.
 *
 * NOTE: the module should get a complete portion of data it needs for processing, process it
 *       than release. The reason is - the depending on the implementation, the calls may be
 *       expensive - may involve some data moving in memory, cache writebacks, etc.
 *
 * Multicore:
 *  single source is intended to be used by a single component only, so all calls to it
 *  may safely be performed using a cached pointer alias. All data modified by source helper are
 *  local and specific to a single source only
 *  The implementation, however, must take into account all cache coherence precautions
 */

/** definition of obfsfucated handler of source API */
struct source;

/**
 * Retrieves size of available data (in bytes)
 * return number of bytes that are available for immediate use
 */
size_t source_get_data_available(struct source __sparse_cache *source);

/**
 * Retrieves size of available data (in frames)
 * return number of bytes that are available for immediate use
 */
size_t source_get_data_frames_available(struct source __sparse_cache *source);

/**
 * Retrieves a fragment of circular data to be used by the caller (to read)
 * After calling get_data, the data are guaranteed to be available
 * for exclusive use (read only) on the caller core through cached pointer
 * The caller MUST take care of data circularity based on provided pointers
 *
 * Depending on implementation - there may be a way to have several receivers of the same
 * data, as long as the receiver respects that data are read-only and won'do anything
 * fancy with cache handling itself
 *
 * some implementation data may be stored in linear buffer
 * in that case:
 *  data_ptr = buffer_start
 *  buffer_end = data_ptr + req_size
 *  buffer_size = req_size
 *
 *  and the data receiver may use it as usual, rollover will simple never occur
 *  NOTE! the caller MUST NOT assume that pointers to start/end of the circular buffer
 *	  are constant. They may change between calls
 *
 * @param source a handler to source
 * @param [in] req_size requested size of data.
 * @param [out] data_ptr a pointer to data will be provided there
 * @param [out] buffer_start pointer to circular buffer start
 * @param [out] buffer_size size of circular buffer
 *
 * @retval -ENODATA if req_size is bigger than available data
 */
int source_get_data(struct source __sparse_cache *source, size_t req_size,
		    void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
		    size_t *buffer_size);

/**
 * Releases fragment previously obtained by source_get_data()
 * Once called, the data are no longer available for the caller
 *
 * @param source a handler to source
 * @param free_size amount of data that the caller declares as "never needed again"
 *	  if free_size == 0 the source implementation MUST keep all data in memory and make them
 *	  available again at next get_data() call
 *	  if free_size is bigger than the amount of data obtained before by get_data(), only
 *	  the amount obtained before will be freed. That means - if somebody obtained some data,
 *	  processed it and won't need it again, it may simple call put_data with free_size==MAXINT
 *
 * @return proper error code (0  on success)
 */
int source_put_data(struct source __sparse_cache *source, size_t free_size);

/**
 * Get total number of bytes processed by the source (meaning - freed by source_put_data())
 */
size_t source_get_num_of_processed_bytes(struct source __sparse_cache *source);

/**
 * sets counter of total number of bytes processed  to zero
 */
void source_reset_num_of_processed_bytes(struct source __sparse_cache *source);

/** get size of a single audio frame (in bytes) */
size_t source_get_frame_bytes(struct source __sparse_cache *source);

/** set of functions for retrieve audio parameters */
enum sof_ipc_frame source_get_frm_fmt(struct source __sparse_cache *source);
enum sof_ipc_frame source_get_valid_fmt(struct source __sparse_cache *source);
unsigned int source_get_rate(struct source __sparse_cache *source);
unsigned int source_get_channels(struct source __sparse_cache *source);
bool source_get_underrun(struct source __sparse_cache *source);

/** set of functions for setting audio parameters */
void source_set_valid_fmt(struct source __sparse_cache *source,
			  enum sof_ipc_frame valid_sample_fmt);
void source_set_rate(struct source __sparse_cache *source, unsigned int rate);
void source_set_channels(struct source __sparse_cache *source, unsigned int channels);
void source_set_underrun(struct source __sparse_cache *source, bool underrun_permitted);

#endif /* __SOF_SOURCE_API_H__ */
