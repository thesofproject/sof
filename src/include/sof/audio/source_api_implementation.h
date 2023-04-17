/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SOURCE_API_IMPLEMENTATION_H__
#define __SOF_SOURCE_API_IMPLEMENTATION_H__

#include <sof/common.h>
#include <sof/audio/format.h>
#include <errno.h>

/**
 * this is a definition of internals of source API
 *
 * this file should be included by the implementations of source API
 *
 * The clients of stream API should use functions provided in source_api.h ONLY
 *
 */

struct source_ops {
	/**
	 * see comment of source_get_data_available()
	 */
	size_t (*get_data_available)(struct source __sparse_cache *source);

	/**
	 * see comment of source_get_data_available()
	 */
	int (*get_data)(struct source __sparse_cache *source, size_t req_size,
			void __sparse_cache **data_ptr, void __sparse_cache **buffer_start,
			size_t *buffer_size);

	/**
	 * see comment of source_put_data()
	 */
	int (*put_data)(struct source __sparse_cache *source, size_t free_size);

	/**
	 * OPTIONAL: Notification to the source implementation about changes in audio format
	 *
	 * Once any of *audio_stream_params elements changes, the implementation of
	 * source may need to perform some extra operations.
	 * This callback will be called immediately after any change
	 */
	void (*on_audio_format_set)(struct source __sparse_cache *source);
};

/** internals of source API. NOT TO BE MODIFIED OUTSIDE OF source_api_helper.h */
struct source {
	const struct source_ops *ops;
	size_t locked_read_frag_size;	/** keeps size of data obtained by get_data() */
	size_t num_of_bytes_processed;	/** processed bytes counter */
	struct sof_audio_stream_params *audio_stream_params; /** pointer to audio params */
};

/**
 * Init of the API, must be called before any operation
 *
 * @param source pointer to the structure
 * @param ops pointer to API operations
 * @param audio_stream_params pointer to structure with audio parameters
 */
void source_init(struct source __sparse_cache *source, const struct source_ops *ops,
		 struct sof_audio_stream_params *audio_stream_params);

#endif /* __SOF_SOURCE_API_IMPLEMENTATION_H__ */
