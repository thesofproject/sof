/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __MODULE_AUDIO_SINK_API_H__
#define __MODULE_AUDIO_SINK_API_H__

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "audio_stream.h"
#include "format.h"

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
 */

/* forward def */
struct sof_sink;
struct sof_audio_stream_params;
struct sof_ipc_stream_params;

/**
 * this is a definition of internals of sink API
 *
 * The clients of stream API should use access functions provided below!
 *
 */

struct sink_ops {
	/**
	 * see comment of sink_get_free_size()
	 */
	size_t (*get_free_size)(struct sof_sink *sink);

	/**
	 * see comment of sink_get_buffer()
	 */
	int (*get_buffer)(struct sof_sink *sink, size_t req_size,
			  void **data_ptr, void **buffer_start, size_t *buffer_size);

	/**
	 * see comment of sink_commit_buffer()
	 */
	int (*commit_buffer)(struct sof_sink *sink, size_t commit_size);

	/**
	 * OPTIONAL: Notification to the sink implementation about changes in audio format
	 *
	 * Once any of *audio_stream_params elements changes, the implementation of
	 * sink may need to perform some extra operations.
	 * This callback will be called immediately after any change
	 *
	 * @retval 0 if success, negative if new parameters are not supported
	 */
	int (*on_audio_format_set)(struct sof_sink *sink);

	/**
	 * OPTIONAL
	 * see sink_set_params comments
	 */
	int (*audio_set_ipc_params)(struct sof_sink *sink,
				    struct sof_ipc_stream_params *params, bool force_update);

	/**
	 * OPTIONAL
	 * see comment for sink_set_alignment_constants
	 */
	int (*set_alignment_constants)(struct sof_sink *sink,
				       const uint32_t byte_align,
				       const uint32_t frame_align_req);
};

/** internals of sink API. NOT TO BE MODIFIED OUTSIDE OF sink_api.c */
struct sof_sink {
	const struct sink_ops *ops;	  /** operations interface */
	size_t requested_write_frag_size; /** keeps number of bytes requested by get_buffer() */
	size_t num_of_bytes_processed;	  /** processed bytes counter */
	size_t min_free_space;		  /** minimum buffer space required by the module using sink
					    *  it is module's OBS as declared in module bind IPC
					    */
	struct sof_audio_stream_params *audio_stream_params; /** pointer to audio params */
};

/**
 *
 * Public functions
 *
 */

/**
 * Retrieves size of free space available in sink (in bytes)
 * return number of free bytes in buffer available to immediate filling
 */
static inline size_t sink_get_free_size(struct sof_sink *sink)
{
	return sink->ops->get_free_size(sink);
}

static inline enum sof_ipc_frame sink_get_frm_fmt(struct sof_sink *sink)
{
	return sink->audio_stream_params->frame_fmt;
}

static inline uint32_t sink_get_channels(struct sof_sink *sink)
{
	return sink->audio_stream_params->channels;
}

/** get size of a single audio frame (in bytes) */
size_t sink_get_frame_bytes(struct sof_sink *sink);

/**
 * Retrieves size of free space available in sink (in frames)
 * return number of free frames in buffer available to immediate filling
 */
size_t sink_get_free_frames(struct sof_sink *sink);

/**
 * Get a circular buffer to operate on (to write).
 *
 * Retrieves a fragment of circular data to be used by the caller
 * After calling get_buffer, the space for data is guaranteed to be available
 * for exclusive use on the caller core through provided pointer
 * if the provided pointers are cached, it is guaranteed that the caller may safely use it without
 * any additional cache operations
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
int sink_get_buffer(struct sof_sink *sink, size_t req_size, void **data_ptr, void **buffer_start,
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
int sink_commit_buffer(struct sof_sink *sink, size_t commit_size);

/** set of functions for retrieve audio parameters */
int sink_set_frm_fmt(struct sof_sink *sink, enum sof_ipc_frame frame_fmt);

static inline enum sof_ipc_frame sink_get_valid_fmt(struct sof_sink *sink)
{
	return sink->audio_stream_params->valid_sample_fmt;
}

static inline uint32_t sink_get_rate(struct sof_sink *sink)
{
	return sink->audio_stream_params->rate;
}

static inline uint32_t sink_get_buffer_fmt(struct sof_sink *sink)
{
	return sink->audio_stream_params->buffer_fmt;
}

/**
 * initial set of audio parameters, provided in sof_ipc_stream_params
 *
 * @param sink a handler to sink
 * @param params the set of parameters
 * @param force_update tells the implementation that the params should override actual settings
 * @return 0 if success
 */
int sink_set_params(struct sof_sink *sink, struct sof_ipc_stream_params *params, bool force_update);

/**
 * Set frame_align_shift and frame_align of stream according to byte_align and
 * frame_align_req alignment requirement. Once the channel number,frame size
 * are determined, the frame_align and frame_align_shift are determined too.
 * these two feature will be used in audio_stream_get_avail_frames_aligned
 * to calculate the available frames. it should be called in component prepare
 * or param functions only once before stream copy. if someone forgets to call
 * this first, there would be unexampled error such as nothing is copied at all.
 *
 * @param sink a handler to sink
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 *
 * @return 0 if success
 */
int sink_set_alignment_constants(struct sof_sink *sink,
				 const uint32_t byte_align,
				 const uint32_t frame_align_req);

int sink_set_valid_fmt(struct sof_sink *sink, enum sof_ipc_frame valid_sample_fmt);
int sink_set_rate(struct sof_sink *sink, unsigned int rate);
int sink_set_channels(struct sof_sink *sink, unsigned int channels);
int sink_set_buffer_fmt(struct sof_sink *sink, uint32_t buffer_fmt);
int sink_set_overrun(struct sof_sink *sink, bool overrun_permitted);
int sink_set_params(struct sof_sink *sink, struct sof_ipc_stream_params *params, bool force_update);

static inline size_t sink_get_min_free_space(struct sof_sink *sink)
{
	return sink->min_free_space;
}

static inline uint32_t sink_get_id(struct sof_sink *sink)
{
	return sink->audio_stream_params->id;
}

static inline uint32_t sink_get_pipeline_id(struct sof_sink *sink)
{
	return sink->audio_stream_params->pipeline_id;
}

#endif /* __MODULE_AUDIO_SINK_API_H__ */
