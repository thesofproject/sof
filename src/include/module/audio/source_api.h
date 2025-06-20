/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __MODULE_AUDIO_SOURCE_API_H__
#define __MODULE_AUDIO_SOURCE_API_H__

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "audio_stream.h"
#include "format.h"

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

 /* forward def */
struct sof_source;
struct sof_audio_stream_params;
struct sof_ipc_stream_params;
struct processing_module;

/**
 * this is a definition of internals of source API
 *
 * The clients of stream API should use access functions provided below!
 *
 */

struct source_ops {
	/**
	 * see comment of source_get_data_available()
	 */
	size_t (*get_data_available)(struct sof_source *source);

	/**
	 * see comment of source_get_data_available()
	 */
	int (*get_data)(struct sof_source *source, size_t req_size,
			void const **data_ptr, void const **buffer_start, size_t *buffer_size);

	/**
	 * see comment of source_release_data()
	 */
	int (*release_data)(struct sof_source *source, size_t free_size);

	/**
	 * OPTIONAL: Notification to the source implementation about changes in audio format
	 *
	 * Once any of *audio_stream_params elements changes, the implementation of
	 * source may need to perform some extra operations.
	 * This callback will be called immediately after any change
	 *
	 * @retval 0 if success, negative if new parameteres are not supported
	 */
	int (*on_audio_format_set)(struct sof_source *source);

	/**
	 * OPTIONAL
	 * see source_set_params comments
	 */
	int (*audio_set_ipc_params)(struct sof_source *source,
				    struct sof_ipc_stream_params *params, bool force_update);

	/**
	 * OPTIONAL
	 * see comment for source_set_alignment_constants
	 */
	int (*set_alignment_constants)(struct sof_source *source,
				       const uint32_t byte_align,
				       const uint32_t frame_align_req);

	/**
	 * OPTIONAL
	 * events called when a module is starting / finishing using of the API
	 * on the core that the module and API will executed on
	 */
	int (*on_bind)(struct sof_source *source, struct processing_module *module);
	int (*on_unbind)(struct sof_source *source);
};

/** internals of source API. NOT TO BE MODIFIED OUTSIDE OF source_api.c */
struct sof_source {
	const struct source_ops *ops;
	size_t requested_read_frag_size; /* keeps size of data obtained by get_data() */
	size_t num_of_bytes_processed;	 /* processed bytes counter */
	size_t min_available;		 /* minimum data available required by the module using
					  * source
					  * it is module's IBS as declared in module bind IPC
					  */
	struct processing_module *bound_module; /* a pointer module that is using source API */
	struct sof_audio_stream_params *audio_stream_params;
};

/**
 *
 * Public functions
 *
 */

/**
 * Retrieves size of available data (in bytes)
 * return number of bytes that are available for immediate use
 */
static inline size_t source_get_data_available(struct sof_source *source)
{
	return source->ops->get_data_available(source);
}

static inline enum sof_ipc_frame source_get_frm_fmt(struct sof_source *source)
{
	return source->audio_stream_params->frame_fmt;
}

static inline unsigned int source_get_channels(struct sof_source *source)
{
	return source->audio_stream_params->channels;
}

/** get size of a single audio frame (in bytes) */
size_t source_get_frame_bytes(struct sof_source *source);

/**
 * Retrieves size of available data (in frames)
 * return number of frames that are available for immediate use
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
int source_get_data(struct sof_source *source, size_t req_size, void const **data_ptr,
		    void const **buffer_start, size_t *buffer_size);

/**
 * Retrieves a fragment of circular data (to read)
 *
 * Same as source_get_data() except that the size of circular buffer is returned as
 * 16 bit samples count. The returned samples count simplifies pointer arithmetic in a
 * samples process function. The data pointers are int16_t type.
 *
 * @param source a handler to source
 * @param [in] req_size requested size of data.
 * @param [out] data_ptr a pointer to data will be provided there
 * @param [out] buffer_start pointer to circular buffer start
 * @param [out] buffer_samples number of 16 bit samples total in circular buffer
 *
 * @retval -ENODATA if req_size is bigger than available data
 */
int source_get_data_s16(struct sof_source *source, size_t req_size, int16_t const **data_ptr,
			int16_t const **buffer_start, int *buffer_samples);

/**
 * Retrieves a fragment of circular data (to read)
 *
 * Same as source_get_data() except that the size of circular buffer is returned as
 * 32 bit samples count. The returned samples count simplifies pointer arithmetic in a
 * samples process function. The data pointers are int32_t type.
 *
 * @param source a handler to source
 * @param [in] req_size requested size of data.
 * @param [out] data_ptr a pointer to data will be provided there
 * @param [out] buffer_start pointer to circular buffer start
 * @param [out] buffer_samples number of 32 bit samples total in circular buffer
 *
 * @retval -ENODATA if req_size is bigger than available data
 */
int source_get_data_s32(struct sof_source *source, size_t req_size, int32_t const **data_ptr,
			int32_t const **buffer_start, int *buffer_samples);

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

/** set of functions for retrieve audio parameters */
static inline enum sof_ipc_frame source_get_valid_fmt(struct sof_source *source)
{
	return source->audio_stream_params->valid_sample_fmt;
}

static inline unsigned int source_get_rate(struct sof_source *source)
{
	return source->audio_stream_params->rate;
}

static inline uint32_t source_get_buffer_fmt(struct sof_source *source)
{
	return source->audio_stream_params->buffer_fmt;
}

static inline size_t source_get_min_available(struct sof_source *source)
{
	return source->min_available;
}

static inline uint32_t source_get_id(struct sof_source *source)
{
	return source->audio_stream_params->id;
}

static inline uint32_t source_get_pipeline_id(struct sof_source *source)
{
	return source->audio_stream_params->pipeline_id;
}

/**
 * @brief hook to be called when a module connects to the API
 *
 * NOTE! it MUST be called at core that a module is bound to
 */
static inline int source_bind(struct sof_source *source, struct processing_module *module)
{
	int ret = 0;

	if (source->bound_module)
		return -EBUSY;

	if (source->ops->on_bind)
		ret = source->ops->on_bind(source, module);

	if (!ret)
		source->bound_module = module;

	return ret;
}

/**
 * @brief hook to be called when a module disconnects from the API
 *
 * NOTE! it MUST be called at core that a module is bound to
 */
static inline int source_unbind(struct sof_source *source)
{
	int ret = 0;

	if (!source->bound_module)
		return -EINVAL;

	if (source->ops->on_unbind)
		ret = source->ops->on_unbind(source);

	if (!ret)
		source->bound_module = NULL;

	return ret;
}

static inline struct processing_module *source_get_bound_module(struct sof_source *source)
{
	return source->bound_module;
}

#endif /* __MODULE_AUDIO_SOURCE_API_H__ */
