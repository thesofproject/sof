/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <sof/audio/component.h>
#include "volume.h"

#define VOL_SCALE (uint32_t)((double)INT32_MAX / VOL_MAX)

struct vol_test_state {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *sink,
		       struct comp_buffer *source);
};

struct vol_test_parameters {
	uint32_t volume;
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_ms;
	uint32_t source_format;
	uint32_t sink_format;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *sink,
		       struct comp_buffer *source);
};

static void set_volume(uint32_t *vol, uint32_t value, uint32_t channels)
{
	int i;

	for (i = 0; i < channels; i++)
		vol[i] = value;
}

static int setup(void **state)
{
	struct vol_test_parameters *parameters = *state;
	struct vol_test_state *vol_state;
	struct comp_data *cd;
	uint32_t size = 0;

	/* allocate new state */
	vol_state = test_malloc(sizeof(*vol_state));

	/* allocate and set new device */
	vol_state->dev = test_malloc(COMP_SIZE(struct sof_ipc_comp_volume));
	vol_state->dev->params.channels = parameters->channels;
	vol_state->dev->frames = parameters->frames;

	/* allocate and set new data */
	cd = test_malloc(sizeof(*cd));
	comp_set_drvdata(vol_state->dev, cd);
	cd->source_format = parameters->source_format;
	cd->sink_format = parameters->sink_format;
	cd->scale_vol = vol_get_processing_function(vol_state->dev);
	set_volume(cd->volume, parameters->volume, parameters->channels);

	/* allocate new sink buffer */
	vol_state->sink = test_malloc(sizeof(*vol_state->sink));
	vol_state->dev->params.frame_fmt = parameters->sink_format;
	size = parameters->frames * comp_frame_bytes(vol_state->dev);
	vol_state->sink->w_ptr = test_calloc(parameters->buffer_size_ms,
					     size);
	vol_state->sink->size = parameters->buffer_size_ms * size;

	/* allocate new source buffer */
	vol_state->source = test_malloc(sizeof(*vol_state->source));
	vol_state->dev->params.frame_fmt = parameters->source_format;
	size = parameters->frames * comp_frame_bytes(vol_state->dev);
	vol_state->source->r_ptr = test_calloc(parameters->buffer_size_ms,
					       size);
	vol_state->source->size = parameters->buffer_size_ms * size;

	/* assigns verification function */
	vol_state->verify = parameters->verify;

	/* assign test state */
	*state = vol_state;

	return 0;
}

static int teardown(void **state)
{
	struct vol_test_state *vol_state = *state;
	struct comp_data *cd = comp_get_drvdata(vol_state->dev);

	/* free everything */
	test_free(cd);
	test_free(vol_state->dev);
	test_free(vol_state->sink->w_ptr);
	test_free(vol_state->sink);
	test_free(vol_state->source->r_ptr);
	test_free(vol_state->source);
	test_free(vol_state);

	return 0;
}

static void fill_source_s16(struct vol_test_state *vol_state)
{
	int16_t *src = (int16_t *)vol_state->source->r_ptr;
	int i;

	for (i = 0; i < vol_state->source->size / sizeof(int16_t); i++)
		src[i] = i;
}

static void fill_source_s32(struct vol_test_state *vol_state)
{
	int32_t *src = (int32_t *)vol_state->source->r_ptr;
	int i;

	for (i = 0; i < vol_state->source->size / sizeof(int32_t); i++)
		src[i] = i << 16;
}

static void verify_s16_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	int channels = dev->params.channels;
	int channel;
	int i;
	double processed;

	for (i = 0; i < sink->size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel] *
				((double)cd->volume[channel] *
				(double)VOL_SCALE / INT32_MAX);
			assert_int_equal(dst[i + channel], processed + 0.5);
		}
	}
}

static void verify_s16_to_sX(struct comp_dev *dev, struct comp_buffer *sink,
			     struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	int channels = dev->params.channels;
	int channel;
	int i;
	double processed;
	int shift = 0;

	/* get shift value */
	if (cd->sink_format == SOF_IPC_FRAME_S24_4LE)
		shift = 8;
	else if (cd->sink_format == SOF_IPC_FRAME_S32_LE)
		shift = 16;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel] *
				((double)cd->volume[channel] *
				(double)VOL_SCALE / INT32_MAX);
			assert_int_equal(dst[i + channel],
					 (uint32_t)(processed + 0.5) << shift);
		}
	}
}

static void verify_sX_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
			     struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	int channels = dev->params.channels;
	int channel;
	int i;
	double processed;
	int shift = 0;

	/* get shift value */
	if (cd->source_format == SOF_IPC_FRAME_S24_4LE)
		shift = 8;

	for (i = 0; i < sink->size / sizeof(uint16_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = (src[i + channel] << shift) *
				((double)cd->volume[channel] *
				(double)VOL_SCALE / INT32_MAX);
			assert_int_equal(dst[i + channel],
					 (uint32_t)(processed + 0.5) >> 16);
		}
	}
}

static void verify_s24_to_s24_s32(struct comp_dev *dev,
				  struct comp_buffer *sink,
				  struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	int channels = dev->params.channels;
	int channel;
	int i;
	double processed;
	int shift = 0;

	/* get shift value */
	if (cd->sink_format == SOF_IPC_FRAME_S32_LE)
		shift = 8;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = (src[i + channel] << 8) *
				((double)cd->volume[channel] *
				(double)VOL_SCALE / INT32_MAX);
			assert_int_equal(dst[i + channel],
					 ((uint32_t)(processed + 0.5) >> 8) << shift);
		}
	}
}

static void verify_s32_to_s24_s32(struct comp_dev *dev,
				  struct comp_buffer *sink,
				  struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	int channels = dev->params.channels;
	int channel;
	int i;
	double processed;
	int shift = 0;

	/* get shift value */
	if (cd->sink_format == SOF_IPC_FRAME_S24_4LE)
		shift = 8;

	for (i = 0; i < sink->size / sizeof(uint32_t); i += channels) {
		for (channel = 0; channel < channels; channel++) {
			processed = src[i + channel] *
				    ((double)cd->volume[channel] *
				    (double)VOL_SCALE / INT32_MAX);
			assert_int_equal(dst[i + channel],
					 (uint32_t)(processed + 0.5) >> shift);
		}
	}
}

static void test_audio_vol(void **state)
{
	struct vol_test_state *vol_state = *state;
	struct comp_data *cd = comp_get_drvdata(vol_state->dev);

	switch (cd->source_format) {
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(vol_state);
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		fill_source_s32(vol_state);
		break;
	}

	cd->scale_vol(vol_state->dev, vol_state->sink, vol_state->source);

	vol_state->verify(vol_state->dev, vol_state->sink, vol_state->source);
}

static struct vol_test_parameters parameters[] = {
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,   verify_s16_to_s16 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,   verify_s16_to_s16 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,   verify_s16_to_s16 },
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE,  verify_s16_to_sX },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE,  verify_s16_to_sX },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE,  verify_s16_to_sX },
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,   verify_s16_to_sX },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,   verify_s16_to_sX },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,   verify_s16_to_sX },

	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  verify_sX_to_s16 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  verify_sX_to_s16 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  verify_sX_to_s16 },
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s24_to_s24_s32 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s24_to_s24_s32 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s24_to_s24_s32 },
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  verify_s24_to_s24_s32 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  verify_s24_to_s24_s32 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  verify_s24_to_s24_s32 },

	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,   verify_sX_to_s16 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,   verify_sX_to_s16 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,   verify_sX_to_s16 },
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,  verify_s32_to_s24_s32 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,  verify_s32_to_s24_s32 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,  verify_s32_to_s24_s32 },
	{ VOL_MAX,     2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,   verify_s32_to_s24_s32 },
	{ VOL_MAX / 2, 2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,   verify_s32_to_s24_s32 },
	{ VOL_MAX / 3, 2, 48, 1, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,   verify_s32_to_s24_s32 },
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
