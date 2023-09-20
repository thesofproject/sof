/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SOURCE_API_H__
#define __SOF_SOURCE_API_H__

#include <sof/common.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

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
 */

/** definition of obfsfucated handler of source API */
struct sof_source;

/* forward def */
struct sof_ipc_stream_params;

/**
 * Retrieves size of available data (in bytes)
 * return number of bytes that are available for immediate use
 */
size_t source_get_data_available(struct sof_source *source);

/**
 * Retrieves size of available data (in frames)
 * return number of bytes that are available for immediate use
 */
size_t source_get_data_frames_available(struct sof_source *source);

/**
 * Retrieves a fragment of circular data to be used by the caller (to read)
 * After calling get_data, the data are guaranteed to be available
 * for exclusive use (read only)
 * if the provided pointers are cached, it is guaranteed that the caller may safely use it without
 * any additional cache operations
 *
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
int source_get_data(struct sof_source *source, size_t req_size,
		    void const **data_ptr, void const **buffer_start, size_t *buffer_size);

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
int source_release_data(struct sof_source *source, size_t free_size);

/**
 * Get total number of bytes processed by the source (meaning - freed by source_release_data())
 */
size_t source_get_num_of_processed_bytes(struct sof_source *source);

/**
 * sets counter of total number of bytes processed  to zero
 */
void source_reset_num_of_processed_bytes(struct sof_source *source);

/** get size of a single audio frame (in bytes) */
size_t source_get_frame_bytes(struct sof_source *source);

/** set of functions for retrieve audio parameters */
enum sof_ipc_frame source_get_frm_fmt(struct sof_source *source);
enum sof_ipc_frame source_get_valid_fmt(struct sof_source *source);
unsigned int source_get_rate(struct sof_source *source);
unsigned int source_get_channels(struct sof_source *source);
uint32_t source_get_buffer_fmt(struct sof_source *source);
bool source_get_underrun(struct sof_source *source);

/** set of functions for setting audio parameters */
int source_set_valid_fmt(struct sof_source *source,
			 enum sof_ipc_frame valid_sample_fmt);
int source_set_rate(struct sof_source *source, unsigned int rate);
int source_set_channels(struct sof_source *source, unsigned int channels);
int source_set_underrun(struct sof_source *source, bool underrun_permitted);
int source_set_buffer_fmt(struct sof_source *source, uint32_t buffer_fmt);
void source_set_min_available(struct sof_source *source, size_t min_available);
size_t source_get_min_available(struct sof_source *source);

/**
 * initial set of audio parameters, provided in sof_ipc_stream_params
 *
 * @param source a handler to source
 * @param params the set of parameters
 * @param force_update tells the implementation that the params should override actual settings
 * @return 0 if success
 */
int source_set_params(struct sof_source *source,
		      struct sof_ipc_stream_params *params, bool force_update);

/**
 * Set frame_align_shift and frame_align of stream according to byte_align and
 * frame_align_req alignment requirement. Once the channel number,frame size
 * are determined, the frame_align and frame_align_shift are determined too.
 * these two feature will be used in audio_stream_get_avail_frames_aligned
 * to calculate the available frames. it should be called in component prepare
 * or param functions only once before stream copy. if someone forgets to call
 * this first, there would be unexampled error such as nothing is copied at all.
 *
 * @param source a handler to source
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 *
 * @return 0 if success
 */
int source_set_alignment_constants(struct sof_source *source,
				   const uint32_t byte_align,
				   const uint32_t frame_align_req);

#endif /* __SOF_SOURCE_API_H__ */
