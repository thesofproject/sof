// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include "../../util.h"

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <mux/mux.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

struct test_data {
	struct comp_dev *dev;
	struct processing_module *mod;
	struct comp_data *cd;
	struct comp_buffer *sinks[MUX_MAX_STREAMS];
	struct comp_buffer *source;
	void *outputs[MUX_MAX_STREAMS];
	uint32_t format;
	uint8_t mask[MUX_MAX_STREAMS][PLATFORM_MAX_CHANNELS];
};

static int16_t input_16b[PLATFORM_MAX_CHANNELS] = {
	0x101, 0x102, 0x104, 0x108,
	0x111, 0x112, 0x114, 0x118,
};

static int32_t input_24b[PLATFORM_MAX_CHANNELS] = {
	0x1a1001, 0x2a2002, 0x4a4004, 0x8a8008,
	0x1b1011, 0x2b2012, 0x4b4014, 0x8b8018,
};

static int32_t input_32b[PLATFORM_MAX_CHANNELS] = {
	0xd1a1001, 0xd2a2002, 0xd4a4004, 0xd8a8008,
	0xe1b1011, 0xe2b2012, 0xe4b4014, 0xe8b8018,
};

static uint16_t valid_formats[] = {
	SOF_IPC_FRAME_S16_LE,
	SOF_IPC_FRAME_S24_4LE,
	SOF_IPC_FRAME_S32_LE,
};

static uint8_t masks[][MUX_MAX_STREAMS][PLATFORM_MAX_CHANNELS] = {
	{ { 0x01, }, },
	{ { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80}, },
	{ { },
	  { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80}, },
	{ { },
	  { },
	  { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80}, },
	{ { },
	  { },
	  { },
	  { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80}, },
	{ { 0x01, },
	  { 0x00, 0x01, },
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10}, },
	{ { 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10},
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x01, },
	  { 0x01, }, },
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x10, 0x08, 0x04, 0x02, 0x01, },},
	{ { 0x01, },
	  { 0x00, 0x01, },
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x01, },},
};

static int setup_group(void **state)
{
	sys_comp_init(sof_get());
	sys_comp_module_demux_interface_init();
	return 0;
}

static struct sof_ipc_comp_process *create_demux_comp_ipc(struct test_data *td)
{
	struct sof_ipc_comp_process *ipc;
	struct sof_mux_config *mux;
	size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	size_t mux_size = sizeof(struct sof_mux_config) +
		MUX_MAX_STREAMS * sizeof(struct mux_stream_data);
	const struct sof_uuid uuid = {
		.a = 0xc4b26868, .b = 0x1430, .c = 0x470e,
		.d = {0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a}
	};
	int i, j;

	ipc = calloc(1, ipc_size + mux_size + SOF_UUID_SIZE);
	memcpy_s(ipc + 1, SOF_UUID_SIZE, &uuid, SOF_UUID_SIZE);
	mux = (struct sof_mux_config *)((char *)(ipc + 1) + SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_DEMUX;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = mux_size;
	ipc->comp.ext_data_length = SOF_UUID_SIZE;

	mux->num_streams = MUX_MAX_STREAMS;
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		mux->streams[i].pipeline_id = i;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; ++j)
			mux->streams[i].mask[j] = td->mask[i][j];
	}

	return ipc;
}

static void prepare_sinks(struct test_data *td, size_t sample_size)
{
	int i;
	uint16_t buffer_size = sample_size * PLATFORM_MAX_CHANNELS;

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		td->sinks[i] = create_test_sink(td->dev, i, td->format, PLATFORM_MAX_CHANNELS,
						buffer_size);

		td->outputs[i] = td->sinks[i]->stream.addr;
		assert_int_equal(audio_stream_get_free_bytes(&td->sinks[i]->stream), buffer_size);
	}
}

static void prepare_source(struct test_data *td, size_t sample_size)
{
	uint16_t buffer_size = sample_size * PLATFORM_MAX_CHANNELS;

	td->source = create_test_source(td->dev, MUX_MAX_STREAMS + 1, td->format,
					PLATFORM_MAX_CHANNELS, buffer_size);

	if (td->format == SOF_IPC_FRAME_S16_LE)
		memcpy_s(td->source->stream.addr, buffer_size, input_16b, buffer_size);
	else if (td->format == SOF_IPC_FRAME_S24_4LE)
		memcpy_s(td->source->stream.addr, buffer_size, input_24b, buffer_size);
	else
		memcpy_s(td->source->stream.addr, buffer_size, input_32b, buffer_size);

	audio_stream_produce(&td->source->stream, buffer_size);
	assert_int_equal(audio_stream_get_avail_bytes(&td->source->stream), buffer_size);
}

static int setup_test_case(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	struct comp_dev *dev;
	struct processing_module *mod;
	struct sof_ipc_comp_process *ipc;
	size_t sample_size = td->format == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t);

	ipc = create_demux_comp_ipc(td);
	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	if (!dev)
		return -EINVAL;

	mod = comp_mod(dev);
	td->dev = dev;
	td->mod = mod;
	td->cd = module_get_private_data(mod);

	prepare_sinks(td, sample_size);
	prepare_source(td, sample_size);

	return comp_prepare(td->dev);
}

static int teardown_test_case(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	int i;

	free_test_source(td->source);

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		free_test_sink(td->sinks[i]);

	comp_free(td->dev);

	return 0;
}

static void test_demux_copy_proc_16(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	int16_t expected_results[MUX_MAX_STREAMS][PLATFORM_MAX_CHANNELS];
	int i, j, k;

	assert_int_equal(comp_copy(td->dev), 0);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; ++j) {
			int64_t sample = 0;

			for (k = 0; k < PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample = input_16b[k];

			expected_results[i][j] = sample;
		}
	}

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		assert_memory_equal(td->outputs[i], expected_results[i],
				    sizeof(expected_results[0]));
}

static void test_demux_copy_proc_24(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	int32_t expected_results[MUX_MAX_STREAMS][PLATFORM_MAX_CHANNELS];
	int i, j, k;

	assert_int_equal(comp_copy(td->dev), 0);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; ++j) {
			int32_t sample = 0;

			for (k = 0; k < PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample = input_24b[k];

			expected_results[i][j] = sample;
		}
	}

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		assert_memory_equal(td->outputs[i], expected_results[i],
				    sizeof(expected_results[0]));
}

static void test_demux_copy_proc_32(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	int32_t expected_results[MUX_MAX_STREAMS][PLATFORM_MAX_CHANNELS];
	int i, j, k;

	assert_int_equal(comp_copy(td->dev), 0);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; ++j) {
			int32_t sample = 0;

			for (k = 0; k < PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample = input_32b[k];

			expected_results[i][j] = sample;
		}
	}

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		assert_memory_equal(td->outputs[i], expected_results[i],
				    sizeof(expected_results[0]));
}

static char *get_test_name(int mask_index, const char *format_name)
{
	int length = snprintf(NULL, 0, "test_demux_copy_%s_mask_%d",
			      format_name, mask_index) + 1;
	char *buffer = malloc(length);

	snprintf(buffer, length, "test_demux_copy_%s_mask_%d",
		 format_name, mask_index);

	return buffer;
}

int main(void)
{
	struct CMUnitTest tests[ARRAY_SIZE(valid_formats) * ARRAY_SIZE(masks)];
	struct test_data *td;
	int i, j, ti, ret;

	for (i = 0; i < ARRAY_SIZE(valid_formats); ++i) {
		for (j = 0; j < ARRAY_SIZE(masks); ++j) {
			ti = i * ARRAY_SIZE(masks) + j;
			td = malloc(sizeof(struct test_data));

			td->format = valid_formats[i];

			memcpy_s(td->mask, sizeof(td->mask),
				 masks[j], sizeof(masks[0]));

			switch (td->format) {
#if CONFIG_FORMAT_S16LE
			case SOF_IPC_FRAME_S16_LE:
				tests[ti].name = get_test_name(j, "s16le");
				tests[ti].test_func = test_demux_copy_proc_16;
				break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
			case SOF_IPC_FRAME_S24_4LE:
				tests[ti].name = get_test_name(j, "s24_4le");
				tests[ti].test_func = test_demux_copy_proc_24;
				break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
			case SOF_IPC_FRAME_S32_LE:
				tests[ti].name = get_test_name(j, "s32le");
				tests[ti].test_func = test_demux_copy_proc_32;
				break;
#endif /* CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S24_3LE
			case SOF_IPC_FRAME_S24_3LE:
				break;
#endif /* CONFIG_FORMAT_S24_3LE */
			default:
				return -EINVAL;
			}

			tests[ti].initial_state = td;
			tests[ti].setup_func = setup_test_case;
			tests[ti].teardown_func = teardown_test_case;
		}
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);
	ret = cmocka_run_group_tests(tests, setup_group, NULL);

	for (ti = 0; ti < ARRAY_SIZE(valid_formats) * ARRAY_SIZE(masks); ti++)
		free(tests[ti].initial_state);

	return ret;
}
