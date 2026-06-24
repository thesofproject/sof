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
 * Computes maximum number of aligned frames that can be copied from source to sink.
 * Uses alignment constants set by source_set_alignment_constants() /
 * sink_set_alignment_constants() to ensure the result satisfies both sides.
 *
 * @param source the data source
 * @param sink the data sink
 * @return Number of aligned frames available for processing.
 */
static inline size_t source_sink_avail_frames_aligned(struct sof_source *source,
						      struct sof_sink *sink)
{
	return MIN(source_get_aligned_frames_available(source), sink_get_free_frames_aligned(sink));
}

#endif /* SINK_SOURCE_UTILS_H */
