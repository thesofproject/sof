/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef SINK_SOURCE_UTILS_H
#define SINK_SOURCE_UTILS_H

#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/math/numbers.h>

/**
 * copy bytes from source to sink
 *
 * @param source the data source
 * @param sink the data target
 * @param free if true, data from source will be freed
 *	       if false, data will remain in the source
 * @param size number of bytes to be copied
 */
int source_to_sink_copy(struct sof_source *source,
			struct sof_sink *sink, bool free, size_t size);

/**
 * fill sink with silence (zeros)
 *
 * @param sink the target to be filled with silence
 * @param size number of bytes to be filled
 */
int sink_fill_with_silence(struct sof_sink *sink, size_t size);

/**
 * drop data from source
 *
 * @param source the source of data to be dropped
 * @param size number of bytes to be dropped
 */
int source_drop_data(struct sof_source *source, size_t size);

/**
 * Computes maximum number of frames aligned with the source align criteria
 * that can be copied from source to sink, verifying the number of available
 * source frames vs. free space available in sink.
 *
 * @param source the data source
 * @param sink the data sink
 * @return Number of aligned frames available for processing.
 */
static inline size_t source_sink_avail_frames_aligned(struct sof_source *source,
						      struct sof_sink *sink)
{
	size_t src_frames = source_get_data_frames_available(source);
	size_t sink_frames = sink_get_free_frames(sink);
	size_t n = MIN(src_frames, sink_frames);

	return source_align_frames_round_down(source, n);
}

#endif /* SINK_SOURCE_UTILS_H */
