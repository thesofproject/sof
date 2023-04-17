/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SINK_API_IMPLEMENTATION_H__
#define __SOF_SINK_API_IMPLEMENTATION_H__

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
	size_t (*get_free_size)(struct sink __sparse_cache *sink);

	/**
	 * see comment of sink_get_buffer()
	 */
	int (*get_buffer)(struct sink __sparse_cache *sink, size_t req_size,
			  void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
			  size_t *buffer_size);

	/**
	 * see comment of sink_commit_buffer()
	 */
	int (*commit_buffer)(struct sink __sparse_cache *sink, size_t commit_size);

	/**
	 * OPTIONAL: Notification to the sink implementation about changes in audio format
	 *
	 * Once any of *audio_stream_params elements changes, the implementation of
	 * sink may need to perform some extra operations.
	 * This callback will be called immediately after any change
	 */
	void (*on_audio_format_set)(struct sink __sparse_cache *sink);
};

/** internals of sink API. NOT TO BE MODIFIED OUTSIDE OF sink_api_helper.h */
struct sink {
	const struct sink_ops *ops;	/** operations interface */
	size_t locked_write_frag_size; /** keeps number of bytes locked by get_buffer() */
	size_t num_of_bytes_processed; /** processed bytes counter */
	struct sof_audio_stream_params *audio_stream_params; /** pointer to audio params */
};

/**
 * Init of the API, must be called before any operation
 *
 * @param sink pointer to the structure
 * @param ops pointer to API operations
 * @param audio_stream_params pointer to structure with audio parameters
 */
void sink_init(struct sink __sparse_cache *sink, const struct sink_ops *ops,
	       struct sof_audio_stream_params *audio_stream_params);

#endif /* __SOF_SINK_API_IMPLEMENTATION_H__ */
