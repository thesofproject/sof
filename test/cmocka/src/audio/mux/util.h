// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/sof.h>
#include <sof/audio/component.h>
#include <sof/audio/mux.h>
#include <sof/lib/alloc.h>

#include <stdlib.h>

static inline struct comp_buffer *create_test_sink(struct comp_dev *dev,
						   uint32_t pipeline_id,
						   uint32_t frame_fmt,
						   uint16_t channels)
{
	struct comp_buffer *buffer = calloc(1, sizeof(struct comp_buffer));

	/* set bsink list */
	list_item_append(&buffer->source_list, &dev->bsink_list);

	/* alloc sink and set default parameters */
	buffer->sink = calloc(1, sizeof(struct comp_dev));
	buffer->sink->state = COMP_STATE_PREPARE;
	buffer->sink->params.frame_fmt = frame_fmt;
	buffer->sink->params.channels = channels;
	buffer->free = 0;
	buffer->avail = 0;
	buffer->pipeline_id = pipeline_id;

	return buffer;
}

static inline void free_test_sink(struct comp_buffer *buffer)
{
	free(buffer->sink);
	free(buffer);
}

static inline struct comp_buffer *create_test_source(struct comp_dev *dev,
						     uint32_t pipeline_id,
						     uint32_t frame_fmt,
						     uint16_t channels)
{
	struct comp_buffer *buffer = calloc(1, sizeof(struct comp_buffer));

	/*set bsource list */
	list_item_append(&buffer->sink_list, &dev->bsource_list);

	/* alloc source and set default parameters */
	buffer->source = calloc(1, sizeof(struct comp_dev));
	buffer->source->state = COMP_STATE_PREPARE;
	buffer->source->params.frame_fmt = frame_fmt;
	buffer->source->params.channels = channels;
	buffer->free = 0;
	buffer->avail = 0;
	buffer->pipeline_id = pipeline_id;

	return buffer;
}

static inline void free_test_source(struct comp_buffer *buffer)
{
	free(buffer->source);
	free(buffer);
}
