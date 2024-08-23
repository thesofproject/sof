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
		list_item_append(&buffer->Xsource_list, &dev->bsink_list);

	/* alloc sink and set default parameters */
	buffer->Xsink = calloc(1, sizeof(struct comp_dev));
	buffer->Xsink->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

static inline void free_test_sink(struct comp_buffer *buffer)
{
	free(buffer->Xsink);
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
		list_item_append(&buffer->Xsink_list, &dev->bsource_list);

	/* alloc source and set default parameters */
	buffer->Xsource = calloc(1, sizeof(struct comp_dev));
	buffer->Xsource->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

static inline void free_test_source(struct comp_buffer *buffer)
{
	free(buffer->Xsource);
	buffer_free(buffer);
}
