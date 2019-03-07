/*
 * Copyright (c) 2019, Intel Corporation
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
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <sof/audio/component.h>
#include "selector.h"


struct sel_test_state {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *sink,
		       struct comp_buffer *source);
};

struct sel_test_parameters {
	uint32_t in_channels;
	uint32_t out_channels;
	uint32_t sel_channel;
	uint32_t frames;
	uint32_t buffer_size_ms;
	uint32_t source_format;
	uint32_t sink_format;
	void (*verify)(struct comp_dev *dev, struct comp_buffer *sink,
		       struct comp_buffer *source);
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
	sel_state->dev->params.channels = parameters->in_channels;
	sel_state->dev->frames = parameters->frames;

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
	sel_state->sink = test_malloc(sizeof(*sel_state->sink));
	sel_state->dev->params.frame_fmt = parameters->sink_format;
	size = parameters->frames * comp_frame_bytes(sel_state->dev);
	if (cd->config.out_channels_count == SEL_SINK_1CH){
		size = size / sel_state->dev->params.channels;
	}
	sel_state->sink->w_ptr = test_calloc(parameters->buffer_size_ms,
					     size);
	sel_state->sink->size = parameters->buffer_size_ms * size;

	/* allocate new source buffer */
	sel_state->source = test_malloc(sizeof(*sel_state->source));
	sel_state->dev->params.frame_fmt = parameters->source_format;
	size = parameters->frames * comp_frame_bytes(sel_state->dev);
	sel_state->source->r_ptr = test_calloc(parameters->buffer_size_ms,
					       size);
	sel_state->source->size = parameters->buffer_size_ms * size;

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
	test_free(sel_state->sink->w_ptr);
	test_free(sel_state->sink);
	test_free(sel_state->source->r_ptr);
	test_free(sel_state->source);
	test_free(sel_state);

	return 0;
}

static void fill_source_s16(struct sel_test_state *sel_state)
{
	int16_t *src = (int16_t *)sel_state->source->r_ptr;
	int i;

	for (i = 0; i < sel_state->source->size / sizeof(int16_t); i++)
		src[i] = i;

}

static void fill_source_s32(struct sel_test_state *sel_state)
{
	int32_t *src = (int32_t *)sel_state->source->r_ptr;
	int i;

	for (i = 0; i < sel_state->source->size / sizeof(int32_t); i++)
		src[i] = i << 16;
}

static void verify_s16le_Xch_to_1ch(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
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

static void verify_s32le_Xch_to_1ch(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
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


static void verify_s16le_2ch_to_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	uint32_t channels = dev->params.channels;
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

static void verify_s16le_4ch_to_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint16_t *src = (uint16_t *)source->r_ptr;
	const uint16_t *dst = (uint16_t *)sink->w_ptr;
	uint32_t channels = dev->params.channels;
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

static void verify_s32le_2ch_to_2ch(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t channels = dev->params.channels;
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

static void verify_s32le_4ch_to_4ch(struct comp_dev *dev, struct comp_buffer *sink,
			      struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	const uint32_t *src = (uint32_t *)source->r_ptr;
	const uint32_t *dst = (uint32_t *)sink->w_ptr;
	uint32_t channels = dev->params.channels;
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

static void test_audio_sel(void **state)
{
	struct sel_test_state *sel_state = *state;
	struct comp_data *cd = comp_get_drvdata(sel_state->dev);

	switch (cd->source_format) {
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(sel_state);
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		fill_source_s32(sel_state);
		break;
	}

	cd->sel_func(sel_state->dev, sel_state->sink, sel_state->source,
		     sel_state->dev->frames);

	sel_state->verify(sel_state->dev, sel_state->sink, sel_state->source);
}


static struct sel_test_parameters parameters[] = {
	{ 2, 1, 0, 16, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 1, 1, 16, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 1, 1, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 2, 2, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_2ch_to_2ch },
	{ 4, 4, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_4ch_to_4ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },
	{ 4, 1, 0, 48, 1, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, verify_s16le_Xch_to_1ch },

	{ 2, 1, 0, 16, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 1, 1, 16, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 1, 1, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 2, 2, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_2ch_to_2ch },
	{ 4, 4, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_4ch_to_4ch },
	{ 2, 1, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
	{ 4, 1, 0, 48, 1, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, verify_s32le_Xch_to_1ch },
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
