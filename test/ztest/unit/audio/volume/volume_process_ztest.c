// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/volume/volume_process.c:
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/list.h>

#include <volume/volume.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Volume levels to test with:
 *
 * VOL_MAX		+42 dB is maximum for Q8.16 format
 * VOL_ZERO_DB		  0 dB is the unity gain gain and default volume
 * VOL_MINUS_80dB	-80 dB is a low gain or large attenuation test
 */
#define VOL_MINUS_80DB (VOL_ZERO_DB / 10000)

/* Max S24_4LE format value */
#define INT24_MAX 8388607

/* Min S24_4LE format value */
#define INT24_MIN -8388608

#define TEST_CHANNELS		2
#define TEST_FRAMES		48
#define TEST_BUFFER_SIZE_MS	1

struct vol_test_state {
	struct processing_module *mod;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct vol_data *cd;
};

typedef void (*vol_verify_func)(struct processing_module *mod, struct comp_buffer *sink,
				struct comp_buffer *source);

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

static void set_volume(int32_t *vol, int32_t value, uint32_t channels)
{
	int i;

	for (i = 0; i < channels; i++)
		vol[i] = value;
}

/**
 * @brief Set up a volume module with one source and one sink buffer.
 * @param[out] state Test state to fill.
 * @param[in] frame_fmt Source and sink frame format.
 * @param[in] volume Gain to set on all channels.
 */
static void setup(struct vol_test_state *state, uint32_t frame_fmt, int32_t volume)
{
	struct processing_module *mod;
	struct comp_dev *dev;
	struct vol_data *cd;
	uint32_t size;

	mod = calloc(1, sizeof(*mod));
	zassert_not_null(mod);
	dev = calloc(1, sizeof(*dev));
	zassert_not_null(dev);
	dev->frames = TEST_FRAMES;
	mod->dev = dev;
	dev->mod = mod;

	list_init(&dev->bsource_list);
	list_init(&dev->bsink_list);

	size = TEST_FRAMES * get_frame_bytes(frame_fmt, TEST_CHANNELS) * TEST_BUFFER_SIZE_MS;
	state->sink = create_test_sink(dev, frame_fmt, TEST_CHANNELS, size);
	state->source = create_test_source(dev, frame_fmt, TEST_CHANNELS, size);

	cd = calloc(1, sizeof(*cd));
	zassert_not_null(cd);
	mod->priv.private = cd;
	cd->is_passthrough = false;

	/* number of channels used by the processing functions */
	cd->channels = TEST_CHANNELS;

	/* set processing function and volume */
	cd->scale_vol = vol_get_processing_function(dev, state->sink, cd);
	zassert_not_null(cd->scale_vol, "No volume processing function for format %u",
			 frame_fmt);
	set_volume(cd->volume, volume, TEST_CHANNELS);

	state->mod = mod;
	state->cd = cd;
}

static void teardown(struct vol_test_state *state)
{
	free(state->sink->sink);
	buffer_free(state->sink);
	free(state->source->source);
	buffer_free(state->source);
	free(state->cd);
	free(state->mod->dev);
	free(state->mod);
}

#if CONFIG_FORMAT_S16LE
static void fill_source_s16(struct comp_buffer *source)
{
	int64_t val;
	int16_t *src = (int16_t *)source->stream.r_ptr;
	int i;
	int sign = 1;

	for (i = 0; i < source->stream.size / sizeof(int16_t); i++) {
		val = (INT16_MIN + (i >> 1)) * sign;
		val = (val > INT16_MAX) ? INT16_MAX : val;
		src[i] = (int16_t)val;
		sign = -sign;
	}
}

static void verify_s16_to_s16(struct processing_module *mod, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int16_t *src = (int16_t *)source->stream.r_ptr;
	const int16_t *dst = (int16_t *)sink->stream.w_ptr;
	double processed;
	int channels = audio_stream_get_channels(&sink->stream);
	int channel;
	int delta;
	int i;
	int16_t sample;

	for (i = 0; i < sink->stream.size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel] *
				(double)cd->volume[channel] /
				(double)VOL_ZERO_DB + 0.5;
			if (processed > INT16_MAX)
				processed = INT16_MAX;

			if (processed < INT16_MIN)
				processed = INT16_MIN;

			sample = (int16_t)processed;
			delta = dst[i + channel] - sample;
			if (delta > 1 || delta < -1)
				zassert_equal(dst[i + channel], sample,
					      "sample %d: output %d, expected %d",
					      i + channel, dst[i + channel], sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void fill_source_s24(struct comp_buffer *source)
{
	int64_t val;
	int32_t *src = (int32_t *)source->stream.r_ptr;
	int i;
	int sign = 1;

	for (i = 0; i < source->stream.size / sizeof(int32_t); i++) {
		val = (INT24_MIN + (i >> 1)) * sign;
		val = (val > INT24_MAX) ? INT24_MAX : val;
		src[i] = (int32_t)val;
		sign = -sign;
	}
}

static void verify_s24_to_s24_s32(struct processing_module *mod, struct comp_buffer *sink,
				  struct comp_buffer *source)
{
	struct vol_data *cd = module_get_private_data(mod);
	const int32_t *src = (int32_t *)source->stream.r_ptr;
	const int32_t *dst = (int32_t *)sink->stream.w_ptr;
	double processed;
	int32_t dst_sample;
	int32_t sample;
	int channels = audio_stream_get_channels(&sink->stream);
	int channel;
	int delta;
	int i;
	int shift = 8;

	for (i = 0; i < sink->stream.size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			/* src * 256 is the CMocka (src << 8) without shifting a
			 * negative value
			 */
			processed = (src[i + channel] * 256) *
				(double)cd->volume[channel] /
				(double)VOL_ZERO_DB + 0.5 * (1 << shift);
			if (processed > INT32_MAX)
				processed = INT32_MAX;

			if (processed < INT32_MIN)
				processed = INT32_MIN;

			sample = ((int32_t)processed) >> shift;
			dst_sample = dst[i + channel];
			delta = dst_sample - sample;
			if (delta > 1 || delta < -1)
				zassert_equal(dst_sample, sample,
					      "sample %d: output %d, expected %d",
					      i + channel, dst_sample, sample);

			if (shift && (dst_sample < INT24_MIN || dst_sample > INT24_MAX))
				zassert_equal(dst_sample, sample,
					      "sample %d: output %d out of the S24 range",
					      i + channel, dst_sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void fill_source_s32(struct comp_buffer *source)
{
	int64_t val;
	int32_t *src = (int32_t *)source->stream.r_ptr;
	int i;
	int sign = 1;

	for (i = 0; i < source->stream.size / sizeof(int32_t); i++) {
		val = (INT32_MIN + (i >> 1)) * sign;
		val = (val > INT32_MAX) ? INT32_MAX : val;
		src[i] = (int32_t)val;
		sign = -sign;
	}
}

static void verify_s32_to_s24_s32(struct processing_module *mod, struct comp_buffer *sink,
				  struct comp_buffer *source)
{
	struct vol_data *cd = module_get_private_data(mod);
	double processed;
	const int32_t *src = (int32_t *)source->stream.r_ptr;
	const int32_t *dst = (int32_t *)sink->stream.w_ptr;
	int32_t dst_sample;
	int32_t sample;
	int channels = audio_stream_get_channels(&sink->stream);
	int channel;
	int delta;
	int i;
	int shift = 0;

	for (i = 0; i < sink->stream.size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel] *
				    (double)cd->volume[channel] /
				    (double)VOL_ZERO_DB + 0.5 * (1 << shift);
			if (processed > INT32_MAX)
				processed = INT32_MAX;

			if (processed < INT32_MIN)
				processed = INT32_MIN;

			sample = ((int32_t)processed) >> shift;
			dst_sample = dst[i + channel];
			delta = dst_sample - sample;
			if (delta > 1 || delta < -1)
				zassert_equal(dst_sample, sample,
					      "sample %d: output %d, expected %d",
					      i + channel, dst_sample, sample);

			if (shift && (dst_sample < INT24_MIN || dst_sample > INT24_MAX))
				zassert_equal(dst_sample, sample,
					      "sample %d: output %d out of the S24 range",
					      i + channel, dst_sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

/**
 * @brief Scale one period of a test signal and verify it against a double
 *        precision reference.
 * @param[in] frame_fmt Source and sink frame format.
 * @param[in] volume Gain to set on all channels.
 * @param[in] verify Format specific verification function.
 */
static void test_audio_vol(uint32_t frame_fmt, int32_t volume, vol_verify_func verify)
{
	struct vol_test_state state;
	struct processing_module *mod;
	struct vol_data *cd;
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;

	setup(&state, frame_fmt, volume);
	mod = state.mod;
	cd = state.cd;

	switch (audio_stream_get_frm_fmt(&state.sink->stream)) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(state.source);
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		fill_source_s24(state.source);
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		fill_source_s32(state.source);
		break;
#endif
	default:
		break;
	}

	source_buf.buf_start = audio_stream_get_addr(&state.source->stream);
	source_buf.buf_end = audio_stream_get_end_addr(&state.source->stream);
	source_buf.ptr = audio_stream_get_rptr(&state.source->stream);

	sink_buf.buf_start = audio_stream_get_addr(&state.sink->stream);
	sink_buf.buf_end = audio_stream_get_end_addr(&state.sink->stream);
	sink_buf.ptr = audio_stream_get_wptr(&state.sink->stream);

	cd->scale_vol(mod, &source_buf, &sink_buf, mod->dev->frames, cd->attenuation);

	verify(mod, state.sink, state.source);

	teardown(&state);
}

#if CONFIG_FORMAT_S16LE
ZTEST(audio_volume_suite, test_audio_vol_s16_max)
{
	test_audio_vol(SOF_IPC_FRAME_S16_LE, VOL_MAX, verify_s16_to_s16);
}

ZTEST(audio_volume_suite, test_audio_vol_s16_zero_db)
{
	test_audio_vol(SOF_IPC_FRAME_S16_LE, VOL_ZERO_DB, verify_s16_to_s16);
}

ZTEST(audio_volume_suite, test_audio_vol_s16_minus_80db)
{
	test_audio_vol(SOF_IPC_FRAME_S16_LE, VOL_MINUS_80DB, verify_s16_to_s16);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
ZTEST(audio_volume_suite, test_audio_vol_s24_max)
{
	test_audio_vol(SOF_IPC_FRAME_S24_4LE, VOL_MAX, verify_s24_to_s24_s32);
}

ZTEST(audio_volume_suite, test_audio_vol_s24_zero_db)
{
	test_audio_vol(SOF_IPC_FRAME_S24_4LE, VOL_ZERO_DB, verify_s24_to_s24_s32);
}

ZTEST(audio_volume_suite, test_audio_vol_s24_minus_80db)
{
	test_audio_vol(SOF_IPC_FRAME_S24_4LE, VOL_MINUS_80DB, verify_s24_to_s24_s32);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
ZTEST(audio_volume_suite, test_audio_vol_s32_max)
{
	test_audio_vol(SOF_IPC_FRAME_S32_LE, VOL_MAX, verify_s32_to_s24_s32);
}

ZTEST(audio_volume_suite, test_audio_vol_s32_zero_db)
{
	test_audio_vol(SOF_IPC_FRAME_S32_LE, VOL_ZERO_DB, verify_s32_to_s24_s32);
}

ZTEST(audio_volume_suite, test_audio_vol_s32_minus_80db)
{
	test_audio_vol(SOF_IPC_FRAME_S32_LE, VOL_MINUS_80DB, verify_s32_to_s24_s32);
}
#endif /* CONFIG_FORMAT_S32LE */

ZTEST_SUITE(audio_volume_suite, NULL, NULL, NULL, NULL, NULL);
