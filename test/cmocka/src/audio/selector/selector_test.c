// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Lech Betlej <lech.betlej@linux.intel.com>

#include "../../util.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/selector.h>

struct sel_test_state {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	void (*verify)(struct comp_dev *dev, struct audio_stream *sink,
		       struct audio_stream *source);
};

struct sel_test_parameters {
	uint32_t in_channels;
	uint32_t out_channels;
	uint32_t sel_channel;
	uint32_t frames;
	uint32_t buffer_size_ms;
	uint32_t source_format;
	uint32_t sink_format;
	void (*verify)(struct comp_dev *dev, struct audio_stream *sink,
		       struct audio_stream *source);
};

static int setup(void **state)
{
	struct sel_test_parameters *parameters = *state;
	struct sel_test_state *sel_state;
	struct comp_data *cd;
	uint32_t size = 0;

	/* allocate new state */
	sel_state = test_malloc(sizeof(*sel_state));

	/* allocate and set new device */
	sel_state->dev = test_malloc(COMP_SIZE(struct sof_ipc_comp_volume));
	sel_state->dev->frames = parameters->frames;

	list_init(&sel_state->dev->bsink_list);
	list_init(&sel_state->dev->bsource_list);

	/* allocate and set new data */
	cd = test_malloc(sizeof(*cd));
	comp_set_drvdata(sel_state->dev, cd);
	cd->source_format = parameters->source_format;
	cd->sink_format = parameters->sink_format;
	
	/* prepare paramters that will be used by sel_get_processing_function */
	cd->config.in_channels_count = parameters->in_channels;
	cd->config.out_channels_count = parameters->out_channels;
	cd->config.sel_channel = parameters->sel_channel;

	cd->sel_func = sel_get_processing_function(sel_state->dev);

	/* allocate new sink buffer */
	size = parameters->frames * get_frame_bytes(parameters->sink_format,
	       parameters->out_channels) * parameters->buffer_size_ms;

	sel_state->sink = create_test_sink(sel_state->dev, 0, parameters->sink_format,
					   parameters->out_channels, size);

	/* allocate new source buffer */
	size = parameters->frames * get_frame_bytes(parameters->source_format,
	       parameters->in_channels) * parameters->buffer_size_ms;

	sel_state->source = create_test_source(sel_state->dev, 0, parameters->source_format,
					       parameters->in_channels, size);

	/* assigns verification function */
	sel_state->verify = parameters->verify;

	/* assign test state */
	*state = sel_state;

	return 0;
}

static int teardown(void **state)
{
	struct sel_test_state *sel_state = *state;
	struct comp_data *cd = comp_get_drvdata(sel_state->dev);

	/* free everything */
	test_free(cd);
	test_free(sel_state->dev);
	free_test_sink(sel_state->sink);
	free_test_source(sel_state->source);
	test_free(sel_state);

	return 0;
}

#if CONFIG_FORMAT_S16LE
static void fill_source_s16(struct sel_test_state *sel_state)
{
	struct audio_stream *stream = &sel_state->source->stream;
	int16_t *w_ptr;
	int i;

	for (i = 0; i < audio_stream_get_free_samples(stream); i++) {
		w_ptr = audio_stream_write_frag_s16(stream, i);
		*w_ptr = i;
	}

	audio_stream_produce(stream, audio_stream_get_free_bytes(stream));
}

static void verify_s16le_Xch_to_1ch(struct comp_dev *dev,
				    struct audio_stream *sink,
				    struct audio_stream *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	uint32_t in_channels = cd->config.in_channels_count;
	uint32_t channel;
	uint32_t i;
	uint32_t j = 0;
	uint16_t source_in;
	uint16_t destination;

	for (i = 0; i < source->size / sizeof(uint16_t); i += in_channels) {
		for (channel = 0; channel < in_channels; channel++) {
			if (channel == cd->config.sel_channel) {
				source_in = src[i + cd->config.sel_channel];
				destination = dst[j++];
				assert_int_equal(source_in, destination);
			}
		}
	}
}

static void verify_s16le_2ch_to_2ch(struct comp_dev *dev,
				    struct audio_stream *sink,
				    struct audio_stream *source)
{
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	uint32_t channels = source->channels;
	uint32_t channel;
	uint32_t i;
	double processed;

	for (i = 0; i < sink->size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel];
			assert_int_equal(dst[i + channel], processed);
		}
	}
}

static void verify_s16le_4ch_to_4ch(struct comp_dev *dev,
				    struct audio_stream *sink,
				    struct audio_stream *source)
{
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	uint32_t channels = source->channels;
	uint32_t channel;
	uint32_t i;
	double processed;

	for (i = 0; i < sink->size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel];
			assert_int_equal(dst[i + channel], processed);
		}
	}
}

#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void fill_source_s32(struct sel_test_state *sel_state)
{
	struct audio_stream *stream = &sel_state->source->stream;
	int32_t *w_ptr;
	int i;

	for (i = 0; i < audio_stream_get_free_samples(stream); i++) {
		w_ptr = audio_stream_write_frag_s32(stream, i);
		*w_ptr = i << 16;
	}

	audio_stream_produce(stream, audio_stream_get_free_bytes(stream));
}

static void verify_s32le_Xch_to_1ch(struct comp_dev *dev,
				    struct audio_stream *sink,
				    struct audio_stream *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t in_channels = cd->config.in_channels_count;
	uint32_t channel;
	uint32_t i;
	uint32_t j = 0;
	uint16_t source_in;
	uint16_t destination;

	for (i = 0; i < source->size / sizeof(uint32_t); i += in_channels) {
		for (channel = 0; channel < in_channels; channel++) {
			if (channel == cd->config.sel_channel) {
				source_in = src[i + cd->config.sel_channel];
				destination = dst[j++];
				assert_int_equal(source_in, destination);
			}
		}
	}
}

static void verify_s32le_2ch_to_2ch(struct comp_dev *dev,
				    struct audio_stream *sink,
				    struct audio_stream *source)
{
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t channels = source->channels;
	uint32_t channel;
	uint32_t i;
	uint32_t processed;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel];
			assert_int_equal(dst[i + channel], processed);
		}
	}
}

static void verify_s32le_4ch_to_4ch(struct comp_dev *dev,
				    struct audio_stream *sink,
				    struct audio_stream *source)
{
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t channels = source->channels;
	uint32_t channel;
	uint32_t i;
	uint32_t processed;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel];
			assert_int_equal(dst[i + channel], processed);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

static void test_audio_sel(void **state)
{
	struct sel_test_state *sel_state = *state;
	struct comp_data *cd = comp_get_drvdata(sel_state->dev);

	switch (cd->source_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(sel_state);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		fill_source_s32(sel_state);
		break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
	}

	cd->sel_func(sel_state->dev, &sel_state->sink->stream, &sel_state->source->stream,
		     sel_state->dev->frames);

	sel_state->verify(sel_state->dev, &sel_state->sink->stream, &sel_state->source->stream);
}

static struct sel_test_parameters parameters[] = {
#if CONFIG_FORMAT_S16LE
	{ 2, 1, 0, 16, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 1, 1, 16, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 1, 1, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 2, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_2ch_to_2ch },
	{ 4, 4, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_4ch_to_4ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 4, 1, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	{ 2, 1, 0, 16, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 1, 1, 16, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 1, 1, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 2, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_2ch_to_2ch },
	{ 4, 4, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_4ch_to_4ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 4, 1, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
};

int main(void)
{
	int i;

	struct CMUnitTest tests[ARRAY_SIZE(parameters)];

	for (i = 0; i < ARRAY_SIZE(parameters); i++) {
		tests[i].name = "test_audio_sel";
		tests[i].test_func = test_audio_sel;
		tests[i].setup_func = setup;
		tests[i].teardown_func = teardown;
		tests[i].initial_state = &parameters[i];
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
