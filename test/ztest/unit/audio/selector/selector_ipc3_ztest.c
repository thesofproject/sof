// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/selector/selector_test.c:
// Author: Lech Betlej <lech.betlej@linux.intel.com>
//
// The CMocka test is built for IPC3 (all the CMocka defconfigs are IPC3), so
// it tests the IPC3 channel selection functions. This file is built in the
// IPC3 scenario of this suite (prj_ipc3.conf), selector_ztest.c in the IPC4
// one.

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/selector.h>
#include <sof/list.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct sel_test_state {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct comp_data *cd;
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

/**
 * @brief Create a test sink buffer connected to a component.
 */
static struct comp_buffer *create_test_sink(struct comp_dev *dev, uint32_t frame_fmt,
					    uint16_t channels, uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buffer, "Failed to allocate the sink buffer");
	memset(buffer->stream.addr, 0, buffer_size);

	list_item_append(&buffer->source_list, &dev->bsink_list);

	buffer->sink = calloc(1, sizeof(*buffer->sink));
	zassert_not_null(buffer->sink);
	buffer->sink->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

/**
 * @brief Create a test source buffer connected to a component.
 */
static struct comp_buffer *create_test_source(struct comp_dev *dev, uint32_t frame_fmt,
					      uint16_t channels, uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buffer, "Failed to allocate the source buffer");
	memset(buffer->stream.addr, 0, buffer_size);

	list_item_append(&buffer->sink_list, &dev->bsource_list);

	buffer->source = calloc(1, sizeof(*buffer->source));
	zassert_not_null(buffer->source);
	buffer->source->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

static void setup(struct sel_test_state *sel_state, const struct sel_test_parameters *parameters)
{
	struct comp_data *cd;
	uint32_t size;

	sel_state->dev = calloc(1, sizeof(*sel_state->dev));
	zassert_not_null(sel_state->dev);
	sel_state->dev->frames = parameters->frames;

	list_init(&sel_state->dev->bsink_list);
	list_init(&sel_state->dev->bsource_list);

	cd = calloc(1, sizeof(*cd));
	zassert_not_null(cd);
	comp_set_drvdata(sel_state->dev, cd);
	cd->source_format = parameters->source_format;
	cd->sink_format = parameters->sink_format;

	/* prepare parameters that will be used by sel_get_processing_function */
	cd->config.in_channels_count = parameters->in_channels;
	cd->config.out_channels_count = parameters->out_channels;
	cd->config.sel_channel = parameters->sel_channel;

	cd->sel_func = sel_get_processing_function(sel_state->dev);
	zassert_not_null(cd->sel_func, "No processing function for format %u, %u channels",
			 parameters->source_format, parameters->out_channels);

	size = parameters->frames * get_frame_bytes(parameters->sink_format,
	       parameters->out_channels) * parameters->buffer_size_ms;
	sel_state->sink = create_test_sink(sel_state->dev, parameters->sink_format,
					   parameters->out_channels, size);

	size = parameters->frames * get_frame_bytes(parameters->source_format,
	       parameters->in_channels) * parameters->buffer_size_ms;
	sel_state->source = create_test_source(sel_state->dev, parameters->source_format,
					       parameters->in_channels, size);

	sel_state->cd = cd;
}

static void teardown(struct sel_test_state *sel_state)
{
	free(sel_state->sink->sink);
	buffer_free(sel_state->sink);
	free(sel_state->source->source);
	buffer_free(sel_state->source);
	free(sel_state->cd);
	free(sel_state->dev);
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

static void verify_s16le_Xch_to_1ch(struct comp_dev *dev, struct audio_stream *sink,
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
				zassert_equal(source_in, destination,
					      "frame %u: output %u, expected %u",
					      j - 1, destination, source_in);
			}
		}
	}
}

/* Also used for the 4 to 4 channel cases, which the CMocka test verified with
 * an identical copy of this function.
 */
static void verify_s16le_nch_to_nch(struct comp_dev *dev, struct audio_stream *sink,
				    struct audio_stream *source)
{
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	uint32_t channels = audio_stream_get_channels(source);
	uint32_t channel;
	uint32_t i;

	for (i = 0; i < sink->size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++)
			zassert_equal(dst[i + channel], src[i + channel],
				      "sample %u: output %u, expected %u", i + channel,
				      dst[i + channel], src[i + channel]);
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

static void verify_s32le_Xch_to_1ch(struct comp_dev *dev, struct audio_stream *sink,
				    struct audio_stream *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t in_channels = cd->config.in_channels_count;
	uint32_t channel;
	uint32_t i;
	uint32_t j = 0;
	/* The CMocka test compared these as uint16_t, i.e. only the low 16
	 * bits, which are always zero in this test signal.
	 */
	uint32_t source_in;
	uint32_t destination;

	for (i = 0; i < source->size / sizeof(uint32_t); i += in_channels) {
		for (channel = 0; channel < in_channels; channel++) {
			if (channel == cd->config.sel_channel) {
				source_in = src[i + cd->config.sel_channel];
				destination = dst[j++];
				zassert_equal(source_in, destination,
					      "frame %u: output 0x%x, expected 0x%x",
					      j - 1, destination, source_in);
			}
		}
	}
}

/* Also used for the 4 to 4 channel cases, which the CMocka test verified with
 * an identical copy of this function.
 */
static void verify_s32le_nch_to_nch(struct comp_dev *dev, struct audio_stream *sink,
				    struct audio_stream *source)
{
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t channels = audio_stream_get_channels(source);
	uint32_t channel;
	uint32_t i;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++)
			zassert_equal(dst[i + channel], src[i + channel],
				      "sample %u: output 0x%x, expected 0x%x", i + channel,
				      dst[i + channel], src[i + channel]);
	}
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

/**
 * @brief Run the selector on one period of a ramp and verify the output.
 * @param[in] parameters Channel counts, selected channel, frames and formats.
 */
static void test_audio_sel(const struct sel_test_parameters *parameters)
{
	struct sel_test_state sel_state;
	struct sof_source *source;
	struct sof_sink *sink;
	struct comp_data *cd;
	int ret;

	setup(&sel_state, parameters);
	cd = sel_state.cd;
	source = audio_buffer_get_source(&sel_state.source->audio_buffer);
	sink = audio_buffer_get_sink(&sel_state.sink->audio_buffer);

	switch (cd->source_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(&sel_state);
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		fill_source_s32(&sel_state);
		break;
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */
	default:
		break;
	}

	ret = cd->sel_func(sel_state.dev, sink, source, sel_state.dev->frames);
	zassert_equal(ret, 0, "Selector processing failed: %d", ret);

	parameters->verify(sel_state.dev, &sel_state.sink->stream, &sel_state.source->stream);

	teardown(&sel_state);
}

#define SEL_TEST_CASE(_name, _in, _out, _sel, _frames, _fmt, _verify)		\
	ZTEST(selector_ipc3_suite, test_audio_sel_##_name)			\
	{									\
		const struct sel_test_parameters parameters = {			\
			_in, _out, _sel, _frames, 1, _fmt, _fmt, _verify	\
		};								\
										\
		test_audio_sel(&parameters);					\
	}

/* The CMocka parameter table lists the 2 to 1 channel, channel 0, 48 frame
 * case twice for each format; both entries are kept, the second one with the
 * _repeat suffix.
 */
#if CONFIG_FORMAT_S16LE
SEL_TEST_CASE(s16le_2ch_to_1ch_sel0_16_frames, 2, 1, 0, 16, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_Xch_to_1ch)
SEL_TEST_CASE(s16le_2ch_to_1ch_sel1_16_frames, 2, 1, 1, 16, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_Xch_to_1ch)
SEL_TEST_CASE(s16le_2ch_to_1ch_sel0_48_frames, 2, 1, 0, 48, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_Xch_to_1ch)
SEL_TEST_CASE(s16le_2ch_to_1ch_sel1_48_frames, 2, 1, 1, 48, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_Xch_to_1ch)
SEL_TEST_CASE(s16le_2ch_to_2ch_48_frames, 2, 2, 0, 48, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_nch_to_nch)
SEL_TEST_CASE(s16le_4ch_to_4ch_48_frames, 4, 4, 0, 48, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_nch_to_nch)
SEL_TEST_CASE(s16le_2ch_to_1ch_sel0_48_frames_repeat, 2, 1, 0, 48, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_Xch_to_1ch)
SEL_TEST_CASE(s16le_4ch_to_1ch_sel0_48_frames, 4, 1, 0, 48, SOF_IPC_FRAME_S16_LE,
	      verify_s16le_Xch_to_1ch)
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
SEL_TEST_CASE(s24le_2ch_to_1ch_sel0_16_frames, 2, 1, 0, 16, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_Xch_to_1ch)
SEL_TEST_CASE(s24le_2ch_to_1ch_sel1_16_frames, 2, 1, 1, 16, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_Xch_to_1ch)
SEL_TEST_CASE(s24le_2ch_to_1ch_sel0_48_frames, 2, 1, 0, 48, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_Xch_to_1ch)
SEL_TEST_CASE(s24le_2ch_to_1ch_sel1_48_frames, 2, 1, 1, 48, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_Xch_to_1ch)
SEL_TEST_CASE(s24le_2ch_to_2ch_48_frames, 2, 2, 0, 48, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_nch_to_nch)
SEL_TEST_CASE(s24le_4ch_to_4ch_48_frames, 4, 4, 0, 48, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_nch_to_nch)
SEL_TEST_CASE(s24le_2ch_to_1ch_sel0_48_frames_repeat, 2, 1, 0, 48, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_Xch_to_1ch)
SEL_TEST_CASE(s24le_4ch_to_1ch_sel0_48_frames, 4, 1, 0, 48, SOF_IPC_FRAME_S24_4LE,
	      verify_s32le_Xch_to_1ch)
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

ZTEST_SUITE(selector_ipc3_suite, NULL, NULL, NULL, NULL, NULL);
