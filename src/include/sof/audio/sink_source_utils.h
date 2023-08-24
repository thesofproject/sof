/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef SINK_SOURCE_UTILS_H
#define SINK_SOURCE_UTILS_H

#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>

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

#endif /* SINK_SOURCE_UTILS_H */
