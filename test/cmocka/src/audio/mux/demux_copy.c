// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include "util.h"

#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/mux.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

struct test_data {
	uint32_t format;
	uint8_t mask[MUX_MAX_STREAMS][PLATFORM_MAX_CHANNELS];
	void *outputs[MUX_MAX_STREAMS];
	struct comp_dev *dev;
	struct comp_buffer *source;
	struct comp_buffer *sinks[MUX_MAX_STREAMS];
};

static int16_t input_16b[PLATFORM_MAX_CHANNELS] = {
	0x101, 0x102, 0x104, 0x108,
	0x111, 0x112, 0x114, 0x118,
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
	{ { 0x01, },
	  { 0x01, },
	  { 0x01, },
	  { 0x01, }, },
	{ { 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x01, },
	  { 0x01, }, },
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x01, }, },
	{ { 0x02, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, },
	  { 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, },
	  { 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x3f, },
	  { 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0xff, }, },
	{ { 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, },
	  { 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, },
	  { 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, },
	  { 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, }, },
	{ { 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, },
	  { 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, },
	  { 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, },
	  { 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, }, },
	{ { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, },
	  { 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, },
	  { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, },
	  { 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, }, },
	{ { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
	  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
	  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, },
	  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, }, },
};

static int setup_group(void **state)
{
	sys_comp_init();
	sys_comp_mux_init();

	return 0;
}

static struct sof_ipc_comp_process *create_demux_comp_ipc(struct test_data *td)
{
	size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	size_t mux_size = get_stream_map_size(td->mask);
	struct sof_ipc_comp_process *ipc = calloc(1, ipc_size + mux_size);
	struct sof_ipc_stream_map *stream_map = (struct sof_ipc_stream_map *)&ipc->data;
	struct sof_ipc_channel_map *chmap;
	char *w_ptr = (char *)&stream_map->ch_map;
	int i, j;

	stream_map->num_ch_map = 0;

	/* for all streams */
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		/* generate a channel map for each channel */
		for (j = 0; j < PLATFORM_MAX_CHANNELS; ++j) {
			chmap = (struct sof_ipc_channel_map *)w_ptr;

			chmap->ch_index = j;
			chmap->ext_id = i;
			chmap->ch_mask = td->mask[i][j];

			stream_map->num_ch_map++;
			w_ptr += sizeof(*chmap) + (popcount(chmap->ch_mask) *
				 sizeof(*chmap->ch_coeffs));
		}
	}

	ipc->comp.hdr.size = sizeof(struct sof_ipc_comp_process);
	ipc->comp.type = SOF_COMP_DEMUX;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = w_ptr - (char *)stream_map;

	return ipc;
}

static void prepare_sinks(struct test_data *td, size_t sample_size)
{
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		td->sinks[i] = create_test_sink(td->dev,
						i,
						td->format,
						PLATFORM_MAX_CHANNELS);
		td->sinks[i]->free = sample_size * PLATFORM_MAX_CHANNELS;
		td->outputs[i] = malloc(sample_size * PLATFORM_MAX_CHANNELS);
		td->sinks[i]->w_ptr = td->outputs[i];
	}
}

static void prepare_source(struct test_data *td, size_t sample_size)
{
	td->source = create_test_source(td->dev,
					MUX_MAX_STREAMS + 1,
					td->format,
					PLATFORM_MAX_CHANNELS);
	td->source->avail = sample_size * PLATFORM_MAX_CHANNELS;

	if (td->format == SOF_IPC_FRAME_S16_LE)
		td->source->r_ptr = input_16b;
	else
		td->source->r_ptr = input_32b;
}

static int setup_test_case(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	struct sof_ipc_comp_process *ipc = create_demux_comp_ipc(td);
	size_t sample_size = td->format == SOF_IPC_FRAME_S16_LE ?
			     sizeof(int16_t) : sizeof(int32_t);
	int ret = 0;

	td->dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);

	if (!td->dev)
		return -EINVAL;

	prepare_sinks(td, sample_size);

	prepare_source(td, sample_size);

	ret = comp_prepare(td->dev);
	if (ret)
		return ret;

	return 0;
}

static int teardown_test_case(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	int i;

	free_test_source(td->source);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		free(td->outputs[i]);
		free_test_sink(td->sinks[i]);
	}

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
					sample += input_16b[k];

			expected_results[i][j] = sat_int16(sample);
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
			int64_t sample = 0;

			for (k = 0; k < PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample += sign_extend_s24(input_32b[k]);

			expected_results[i][j] = sat_int24(sample);
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
			int64_t sample = 0;

			for (k = 0; k < PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample += input_32b[k];

			expected_results[i][j] = sat_int32(sample);
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
	int i, j;
	struct CMUnitTest tests[ARRAY_SIZE(valid_formats) * ARRAY_SIZE(masks)];

	for (i = 0; i < ARRAY_SIZE(valid_formats); ++i) {
		for (j = 0; j < ARRAY_SIZE(masks); ++j) {
			int ti = i * ARRAY_SIZE(masks) + j;
			struct test_data *td = malloc(sizeof(struct test_data));

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
			default:
				return -EINVAL;
			}

			tests[ti].initial_state = td;
			tests[ti].setup_func = setup_test_case;
			tests[ti].teardown_func = teardown_test_case;
		}
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup_group, NULL);
}
