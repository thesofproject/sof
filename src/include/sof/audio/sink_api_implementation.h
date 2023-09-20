/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SINK_API_IMPLEMENTATION_H__
#define __SOF_SINK_API_IMPLEMENTATION_H__

#include <sof/common.h>
#include <stdint.h>
#include <stdbool.h>

/* forward def */
struct sof_audio_stream_params;

/**
 * this is a definition of internals of sink API
 *
 * this file should be included by the implementations of sink API
 *
 * The clients of stream API should use functions provided in sink_api.h ONLY
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

/** internals of sink API. NOT TO BE MODIFIED OUTSIDE OF sink_api_helper.h */
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
 * Init of the API, must be called before any operation
 *
 * @param sink pointer to the structure
 * @param ops pointer to API operations
 * @param audio_stream_params pointer to structure with audio parameters
 *	  note that the audio_stream_params must be accessible by the caller core
 *	  the implementation must ensure coherent access to the parameteres
 *	  in case of i.e. cross core shared queue, it must be located in non-cached memory
 */
void sink_init(struct sof_sink *sink, const struct sink_ops *ops,
	       struct sof_audio_stream_params *audio_stream_params);

#endif /* __SOF_SINK_API_IMPLEMENTATION_H__ */
