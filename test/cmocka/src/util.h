// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <rtos/sof.h>
#include <sof/audio/component.h>
#include <rtos/alloc.h>

#include <stdlib.h>

static inline struct comp_buffer *create_test_sink(struct comp_dev *dev,
						   uint32_t pipeline_id,
						   uint32_t frame_fmt,
						   uint16_t channels,
						   uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.comp = {
			.pipeline_id = pipeline_id,
		},
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(&desc, false);

	memset(buffer->stream.addr, 0, buffer_size);

	/* set bsink list */
	if (dev)
		list_item_append(&buffer->source_list, &dev->bsink_list);

	/* alloc sink and set default parameters */
	buffer->sink = calloc(1, sizeof(struct comp_dev));
	buffer->sink->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

static inline void free_test_sink(struct comp_buffer *buffer)
{
	free(comp_buffer_get_sink_component(buffer));
	buffer_free(buffer);
}

static inline struct comp_buffer *create_test_source(struct comp_dev *dev,
						     uint32_t pipeline_id,
						     uint32_t frame_fmt,
						     uint16_t channels,
						     uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.comp = {
			.pipeline_id = pipeline_id,
		},
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(&desc, false);

	memset(buffer->stream.addr, 0, buffer_size);

	/*set bsource list */
	if (dev)
		list_item_append(&buffer->sink_list, &dev->bsource_list);

	/* alloc source and set default parameters */
	buffer->source = calloc(1, sizeof(struct comp_dev));
	buffer->source->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

static inline void free_test_source(struct comp_buffer *buffer)
{
	free(buffer->source);
	buffer_free(buffer);
}
