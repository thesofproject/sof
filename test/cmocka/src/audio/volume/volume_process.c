// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include "../../util.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <sof/audio/component.h>
#include <sof/audio/volume.h>

/* Add macro for a volume test level. The levels to test with this code
 * are:
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

struct vol_test_state {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *sink, struct comp_buffer *source);
};

struct vol_test_parameters {
	int32_t volume;
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_ms;
	uint32_t source_format;
	uint32_t sink_format;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *sink, struct comp_buffer *source);
};

static void set_volume(int32_t *vol, int32_t value, uint32_t channels)
{
	int i;

	for (i = 0; i < channels; i++)
		vol[i] = value;
}

static int setup(void **state)
{
	struct vol_test_parameters *parameters = *state;
	struct vol_test_state *vol_state;
	struct vol_data *cd;
	uint32_t size = 0;

	/* allocate new state */
	vol_state = test_malloc(sizeof(*vol_state));

	/* allocate and set new device */
	vol_state->dev = test_malloc(sizeof(struct comp_dev));
	vol_state->dev->frames = parameters->frames;

	/* allocate and set new data */
	cd = test_malloc(sizeof(*cd));
	comp_set_drvdata(vol_state->dev, cd);

	list_init(&vol_state->dev->bsource_list);
	list_init(&vol_state->dev->bsink_list);

	/* allocate new sink buffer */
	size = parameters->frames * get_frame_bytes(parameters->sink_format, parameters->channels) *
	       parameters->buffer_size_ms;

	vol_state->sink = create_test_sink(vol_state->dev, 0, parameters->sink_format,
					   parameters->channels, size);

	/* allocate new source buffer */
	size = parameters->frames * get_frame_bytes(parameters->source_format,
	       parameters->channels) * parameters->buffer_size_ms;

	vol_state->source = create_test_source(vol_state->dev, 0, parameters->source_format,
					       parameters->channels, size);

	/* set processing function and volume */
	cd->scale_vol = vol_get_processing_function(vol_state->dev);
	set_volume(cd->volume, parameters->volume, parameters->channels);

	/* assigns verification function */
	vol_state->verify = parameters->verify;

	/* assign test state */
	*state = vol_state;

	return 0;
}

static int teardown(void **state)
{
	struct vol_test_state *vol_state = *state;
	struct vol_data *cd = comp_get_drvdata(vol_state->dev);

	/* free everything */
	test_free(cd);
	test_free(vol_state->dev);
	free_test_sink(vol_state->sink);
	free_test_source(vol_state->source);
	test_free(vol_state);

	return 0;
}

#if CONFIG_FORMAT_S16LE
static void fill_source_s16(struct vol_test_state *vol_state)
{
	int64_t val;
	int16_t *src = (int16_t *)vol_state->source->stream.r_ptr;
	int i;
	int sign = 1;

	for (i = 0; i < vol_state->source->stream.size / sizeof(int16_t); i++) {
		val = (INT16_MIN + (i >> 1)) * sign;
		val = (val > INT16_MAX) ? INT16_MAX : val;
		src[i] = (int16_t)val;
		sign = -sign;
	}
}

static void verify_s16_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	const int16_t *src = (int16_t *)source->stream.r_ptr;
	const int16_t *dst = (int16_t *)sink->stream.w_ptr;
	double processed;
	int channels = sink->stream.channels;
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
				assert_int_equal(dst[i + channel], sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void fill_source_s24(struct vol_test_state *vol_state)
{
	int64_t val;
	int32_t *src = (int32_t *)vol_state->source->stream.r_ptr;
	int i;
	int sign = 1;

	for (i = 0; i < vol_state->source->stream.size / sizeof(int32_t); i++) {
		val = (INT24_MIN + (i >> 1)) * sign;
		val = (val > INT24_MAX) ? INT24_MAX : val;
		src[i] = (int32_t)val;
		sign = -sign;
	}
}

static void verify_s24_to_s24_s32(struct comp_dev *dev,
				  struct comp_buffer *sink,
				  struct comp_buffer *source)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	const int32_t *src = (int32_t *)source->stream.r_ptr;
	const int32_t *dst = (int32_t *)sink->stream.w_ptr;
	double processed;
	int32_t dst_sample;
	int32_t sample;
	int channels = sink->stream.channels;
	int channel;
	int delta;
	int i;
	int shift = 8;

	for (i = 0; i < sink->stream.size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = (src[i + channel] << 8) *
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
				assert_int_equal(dst_sample, sample);

			if (shift && (dst_sample < INT24_MIN ||
				      dst_sample > INT24_MAX))
				assert_int_equal(dst_sample, sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void fill_source_s32(struct vol_test_state *vol_state)
{
	int64_t val;
	int32_t *src = (int32_t *)vol_state->source->stream.r_ptr;
	int i;
	int sign = 1;

	for (i = 0; i < vol_state->source->stream.size / sizeof(int32_t); i++) {
		val = (INT32_MIN + (i >> 1)) * sign;
		val = (val > INT32_MAX) ? INT32_MAX : val;
		src[i] = (int32_t)val;
		sign = -sign;
	}
}

static void verify_s32_to_s24_s32(struct comp_dev *dev,
				  struct comp_buffer *sink,
				  struct comp_buffer *source)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	double processed;
	const int32_t *src = (int32_t *)source->stream.r_ptr;
	const int32_t *dst = (int32_t *)sink->stream.w_ptr;
	int32_t dst_sample;
	int32_t sample;
	int channels = sink->stream.channels;
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
				assert_int_equal(dst_sample, sample);

			if (shift && (dst_sample < INT24_MIN ||
				      dst_sample > INT24_MAX))
				assert_int_equal(dst_sample, sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if 0

#if CONFIG_FORMAT_S16LE && (CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE)
static void verify_s16_to_sX(struct comp_dev *dev, struct comp_buffer *sink,
			     struct comp_buffer *source)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	const int16_t *src = (int16_t *)source->r_ptr;
	const int32_t *dst = (int32_t *)sink->w_ptr;
	double processed;
	int32_t dst_sample;
	int32_t sample;
	int channels = sink->channels;
	int channel;
	int delta;
	int i;
	int shift = 0;

	/* get shift value */
	if (cd->sink_format == SOF_IPC_FRAME_S24_4LE)
		shift = 8;
	else if (cd->sink_format == SOF_IPC_FRAME_S32_LE)
		shift = 0;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = 65536.0 * (double)src[i + channel] *
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
				assert_int_equal(dst_sample, sample);

			if (shift && (dst_sample < INT24_MIN ||
				      dst_sample > INT24_MAX))
				assert_int_equal(dst_sample, sample);
		}
	}
}

static void verify_sX_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			     struct comp_buffer *source)
{
	struct vol_data *cd = comp_get_drvdata(dev);
	const int32_t *src = (int32_t *)source->r_ptr;
	const int16_t *dst = (int16_t *)sink->w_ptr;
	double processed;
	int channels = dev->params.channels;
	int channel;
	int delta;
	int i;
	int shift = 0;
	int16_t sample;

	/* get shift value */
	if (cd->source_format == SOF_IPC_FRAME_S24_4LE)
		shift = 8;

	for (i = 0; i < sink->size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = (double)(src[i + channel] << shift) *
				(double)cd->volume[channel] /
				(double)VOL_ZERO_DB;
			processed = processed / 65536.0 + 0.5;
			if (processed > INT16_MAX)
				processed = INT16_MAX;

			if (processed < INT16_MIN)
				processed = INT16_MIN;

			sample = (int16_t)processed;
			delta = dst[i + channel] - sample;
			if (delta > 1 || delta < -1)
				assert_int_equal(dst[i + channel], sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE && (CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE) */

#endif

static void test_audio_vol(void **state)
{
	struct vol_test_state *vol_state = *state;
	struct vol_data *cd = comp_get_drvdata(vol_state->dev);

	switch (vol_state->sink->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(vol_state);
		break;
	case SOF_IPC_FRAME_S24_4LE:
		fill_source_s24(vol_state);
		break;
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		fill_source_s32(vol_state);
		break;
	case SOF_IPC_FRAME_S24_3LE:
		/* TODO: add 3LE support */
		break;
	}

	cd->scale_vol(vol_state->dev, &vol_state->sink->stream,
		      &vol_state->source->stream, vol_state->dev->frames);

	vol_state->verify(vol_state->dev, vol_state->sink, vol_state->source);
}

static struct vol_test_parameters parameters[] = {
#if CONFIG_FORMAT_S16LE
	{ VOL_MAX,        2, 48, 1, SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S16_LE,   verify_s16_to_s16 }, /* 1 */
	{ VOL_ZERO_DB,    2, 48, 1, SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S16_LE,   verify_s16_to_s16 }, /* 2 */
	{ VOL_MINUS_80DB, 2, 48, 1, SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S16_LE,   verify_s16_to_s16 }, /* 3 */
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ VOL_MAX,        2, 48, 1, SOF_IPC_FRAME_S24_4LE,
		SOF_IPC_FRAME_S24_4LE, verify_s24_to_s24_s32 }, /* 4 */
	{ VOL_ZERO_DB,    2, 48, 1, SOF_IPC_FRAME_S24_4LE,
		SOF_IPC_FRAME_S24_4LE, verify_s24_to_s24_s32 }, /* 5 */
	{ VOL_MINUS_80DB, 2, 48, 1, SOF_IPC_FRAME_S24_4LE,
		SOF_IPC_FRAME_S24_4LE, verify_s24_to_s24_s32 }, /* 6 */
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ VOL_MAX,        2, 48, 1, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S32_LE,   verify_s32_to_s24_s32 }, /* 7 */
	{ VOL_ZERO_DB,    2, 48, 1, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S32_LE,   verify_s32_to_s24_s32 }, /* 8 */
	{ VOL_MINUS_80DB, 2, 48, 1, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S32_LE,   verify_s32_to_s24_s32 }, /* 9 */
#endif /* CONFIG_FORMAT_S32LE */
};

int main(void)
{
	int i;

	struct CMUnitTest tests[ARRAY_SIZE(parameters)];

	for (i = 0; i < ARRAY_SIZE(parameters); i++) {
		tests[i].name = "test_audio_vol";
		tests[i].test_func = test_audio_vol;
		tests[i].setup_func = setup;
		tests[i].teardown_func = teardown;
		tests[i].initial_state = &parameters[i];
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
