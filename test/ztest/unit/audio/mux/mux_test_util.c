// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original helpers from test/cmocka/src/util.h and from the
// create_mux_comp_ipc() and create_demux_comp_ipc() functions of the tests in
// test/cmocka/src/audio/mux/
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/list.h>
#include <rtos/idc_msg.h>
#include <rtos/string.h>
#include <ipc/topology.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mux_test_util.h"

/**
 * @brief Allocate a component buffer.
 * @param pipeline_id Pipeline ID of the buffer.
 * @param frame_fmt Frame format of the buffer.
 * @param channels Number of channels of the buffer.
 * @param buffer_size Buffer size in bytes.
 * @return The new buffer with zeroed data, never NULL.
 */
static struct comp_buffer *mux_test_create_buffer(uint32_t pipeline_id, uint32_t frame_fmt,
						  uint16_t channels, uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.comp = {
			.pipeline_id = pipeline_id,
		},
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buffer, "Failed to allocate a %u byte buffer", buffer_size);
	memset(buffer->stream.addr, 0, buffer_size);
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

/**
 * @brief Allocate the dummy component on the far end of a test buffer.
 * @return A zeroed component in the PREPARE state, never NULL.
 */
static struct comp_dev *mux_test_create_far_end(void)
{
	struct comp_dev *dev = calloc(1, sizeof(*dev));

	zassert_not_null(dev);
	dev->state = COMP_STATE_PREPARE;

	return dev;
}

struct comp_buffer *mux_test_create_sink(struct comp_dev *dev, uint32_t pipeline_id,
					 uint32_t frame_fmt, uint16_t channels,
					 uint16_t buffer_size)
{
	struct comp_buffer *buffer;

	buffer = mux_test_create_buffer(pipeline_id, frame_fmt, channels, buffer_size);
	buffer->sink = mux_test_create_far_end();
	if (dev)
		list_item_append(&buffer->source_list, &dev->bsink_list);

	return buffer;
}

void mux_test_free_sink(struct comp_buffer *buffer)
{
	if (!buffer)
		return;

	free(comp_buffer_get_sink_component(buffer));
	buffer_free(buffer);
}

struct comp_buffer *mux_test_create_source(struct comp_dev *dev, uint32_t pipeline_id,
					   uint32_t frame_fmt, uint16_t channels,
					   uint16_t buffer_size)
{
	struct comp_buffer *buffer;

	buffer = mux_test_create_buffer(pipeline_id, frame_fmt, channels, buffer_size);
	buffer->source = mux_test_create_far_end();
	if (dev)
		list_item_append(&buffer->sink_list, &dev->bsource_list);

	return buffer;
}

void mux_test_free_source(struct comp_buffer *buffer)
{
	if (!buffer)
		return;

	free(comp_buffer_get_source_component(buffer));
	buffer_free(buffer);
}

struct comp_dev *mux_test_comp_new(const struct sof_uuid *uuid,
				   const uint8_t mask[MUX_MAX_STREAMS]
						     [CONFIG_PLATFORM_MAX_CHANNELS])
{
	struct sof_ipc_comp_process *ipc;
	struct sof_mux_config *mux;
	struct comp_dev *dev;
	const size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	const size_t mux_size = sizeof(struct sof_mux_config) +
		MUX_MAX_STREAMS * sizeof(struct mux_stream_data);
	int i, j;

	ipc = calloc(1, ipc_size + mux_size + SOF_UUID_SIZE);
	zassert_not_null(ipc);

	memcpy_s(ipc + 1, SOF_UUID_SIZE, uuid, SOF_UUID_SIZE);
	mux = (struct sof_mux_config *)((char *)(ipc + 1) + SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_MODULE_ADAPTER;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = mux_size;
	ipc->comp.ext_data_length = SOF_UUID_SIZE;

	mux->num_streams = MUX_MAX_STREAMS;
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		mux->streams[i].pipeline_id = i;
		for (j = 0; j < CONFIG_PLATFORM_MAX_CHANNELS; ++j)
			mux->streams[i].mask[j] = mask ? mask[i][j] : 0;
	}

	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);

	return dev;
}

/**
 * @brief IDC stub for the remote branch of comp_prepare().
 *
 * comp_prepare() is an inline function that forwards the call to another core
 * for shared components. The test components are never shared, so the branch
 * is not taken, but builds without optimization (for example with --coverage)
 * still reference idc_send_msg().
 */
int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	(void)msg;
	(void)mode;

	zassert_unreachable("Unexpected IDC message");

	return -ENOTSUP;
}
