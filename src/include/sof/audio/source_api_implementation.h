/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SOURCE_API_IMPLEMENTATION_H__
#define __SOF_SOURCE_API_IMPLEMENTATION_H__

#include <sof/audio/stream_common.h>
#include <sof/common.h>
#include <stdint.h>
#include <stdbool.h>

/* forward def */

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
	size_t (*get_data_available)(struct sof_source __sparse_cache *source);

	/**
	 * see comment of source_get_data_available()
	 */
	int (*get_data)(struct sof_source __sparse_cache *source, size_t req_size,
			void **data_ptr, void **buffer_start, size_t *buffer_size);

	/**
	 * see comment of source_release_data()
	 */
	int (*release_data)(struct sof_source __sparse_cache *source, size_t free_size);

	/**
	 * get a pointer to runtime parameters
	 */
	struct sof_audio_stream_params __sparse_cache *(*audio_stream_params)(
		struct sof_source __sparse_cache *source);

	/**
	 * OPTIONAL: Notification to the source implementation about changes in audio format
	 *
	 * Once any of *audio_stream_params elements changes, the implementation of
	 * source may need to perform some extra operations.
	 * This callback will be called immediately after any change
	 *
	 * @retval 0 if success, negative if new parameteres are not supported
	 */
	int (*on_audio_format_set)(struct sof_source __sparse_cache *source);

	/**
	 * OPTIONAL
	 * see source_set_params comments
	 */
	int (*audio_set_ipc_params)(struct sof_source __sparse_cache *source,
				    struct sof_ipc_stream_params *params, bool force_update);

	/**
	 * OPTIONAL
	 * see comment for source_set_alignment_constants
	 */
	int (*set_alignment_constants)(struct sof_source __sparse_cache *source,
				       const uint32_t byte_align,
				       const uint32_t frame_align_req);
};

/** internals of source API. NOT TO BE MODIFIED OUTSIDE OF source_api_helper.h */
struct sof_source {
	const struct source_ops *ops;
	size_t requested_read_frag_size;	/** keeps size of data obtained by get_data() */
	size_t num_of_bytes_processed;	/** processed bytes counter */
	struct sof_audio_stream_params audio_stream_params;
};

/**
 * Init of the API, must be called before any operation
 *
 * @param source pointer to the structure
 * @param ops pointer to API operations
 */
void source_init(struct sof_source __sparse_cache *source, const struct source_ops *ops);

#endif /* __SOF_SOURCE_API_IMPLEMENTATION_H__ */
