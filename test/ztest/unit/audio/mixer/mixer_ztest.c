// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/mixer/mixer_test.c:
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/list.h>

#include "mixer/mixer.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* math.h's M_PI is only exposed under glibc's GNU/POSIX feature test macros,
 * which Zephyr's strict -std=c17 build does not define.
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TEST_FRAMES		48
#define TEST_BUFFER_SIZE_MS	1
#define TEST_MAX_SOURCES	8

struct mix_test_state {
	struct comp_dev *dev;
	struct comp_buffer *sources[TEST_MAX_SOURCES];
	struct comp_buffer *sink;
	struct mixer_data md;
	int num_sources;
	int num_chans;
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

	zassert_not_null(buffer, "Failed to allocate a source buffer");
	memset(buffer->stream.addr, 0, buffer_size);

	list_item_append(&buffer->sink_list, &dev->bsource_list);

	buffer->source = calloc(1, sizeof(*buffer->source));
	zassert_not_null(buffer->source);
	buffer->source->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

/**
 * @brief Set up a component with S32_LE source buffers and one sink buffer.
 * @param[out] state Test state to fill.
 * @param[in] num_sources Number of mixer sources.
 * @param[in] num_chans Number of channels of every stream.
 */
static void setup(struct mix_test_state *state, int num_sources, int num_chans)
{
	uint32_t size;
	int i;

	memset(state, 0, sizeof(*state));
	state->num_sources = num_sources;
	state->num_chans = num_chans;

	state->dev = calloc(1, sizeof(*state->dev));
	zassert_not_null(state->dev);
	state->dev->frames = TEST_FRAMES;
	list_init(&state->dev->bsource_list);
	list_init(&state->dev->bsink_list);

	size = TEST_FRAMES * get_frame_bytes(SOF_IPC_FRAME_S32_LE, num_chans) *
	       TEST_BUFFER_SIZE_MS;
	state->sink = create_test_sink(state->dev, SOF_IPC_FRAME_S32_LE, num_chans, size);
	for (i = 0; i < num_sources; i++)
		state->sources[i] = create_test_source(state->dev, SOF_IPC_FRAME_S32_LE,
						       num_chans, size);

	state->md.mix_func = mixer_get_processing_function(state->dev,
			audio_stream_get_frm_fmt(&state->sink->stream));
	zassert_not_null(state->md.mix_func, "No S32_LE mixer processing function");
}

static void teardown(struct mix_test_state *state)
{
	int i;

	free(state->sink->sink);
	buffer_free(state->sink);
	for (i = 0; i < state->num_sources; i++) {
		free(state->sources[i]->source);
		buffer_free(state->sources[i]);
	}
	free(state->dev);
}

/**
 * @brief Mix sine waves from num_sources S32_LE sources and check that every
 *        output sample is the saturated sum of the input samples.
 * @param[in] num_sources Number of mixer sources.
 * @param[in] num_chans Number of channels of every stream.
 */
static void test_audio_mixer_copy(int num_sources, int num_chans)
{
	struct mix_test_state tc;
	struct cir_buf_source sources_buf[CONFIG_PLATFORM_MAX_STREAMS];
	struct cir_buf_sink sink_buf;
	int src_idx;
	int smp;

	setup(&tc, num_sources, num_chans);

	for (src_idx = 0; src_idx < tc.num_sources; ++src_idx) {
		uint32_t *samples = tc.sources[src_idx]->stream.addr;

		for (smp = 0; smp < tc.sources[src_idx]->stream.size / sizeof(int32_t); ++smp) {
			double rad = M_PI / (180.0 / (smp * (src_idx + 1)));

			samples[smp] = ((sin(rad) + 1) / 2) * (0xFFFFFFFF / 2);
		}

		audio_stream_produce(&tc.sources[src_idx]->stream,
				     tc.sources[src_idx]->stream.size / sizeof(int32_t));

		sources_buf[src_idx].buf_start =
			audio_stream_get_addr(&tc.sources[src_idx]->stream);
		sources_buf[src_idx].buf_end =
			audio_stream_get_end_addr(&tc.sources[src_idx]->stream);
		sources_buf[src_idx].ptr =
			audio_stream_get_rptr(&tc.sources[src_idx]->stream);
	}

	sink_buf.buf_start = audio_stream_get_addr(&tc.sink->stream);
	sink_buf.buf_end = audio_stream_get_end_addr(&tc.sink->stream);
	sink_buf.ptr = audio_stream_get_wptr(&tc.sink->stream);

	tc.md.mix_func(&sink_buf, sources_buf, tc.num_sources,
		       tc.dev->frames * tc.num_chans);

	for (smp = 0; smp < tc.sink->stream.size / sizeof(int32_t); ++smp) {
		uint64_t sum = 0;

		for (src_idx = 0; src_idx < tc.num_sources; ++src_idx) {
			zassert_not_null(tc.sources[src_idx]);

			uint32_t *samples = tc.sources[src_idx]->stream.addr;

			sum += samples[smp];
		}

		sum = sat_int32(sum);

		uint32_t *out_samples = tc.sink->stream.addr;

		zassert_equal(out_samples[smp], sum, "sample %d: output %u, expected %llu",
			      smp, out_samples[smp], (unsigned long long)sum);
	}

	teardown(&tc);
}

#define MIXER_TEST_CASE(_num_sources, _num_chans)					\
	ZTEST(audio_mixer_suite,							\
	      test_audio_mixer_copy_##_num_sources##_srcs_##_num_chans##ch)		\
	{										\
		test_audio_mixer_copy(_num_sources, _num_chans);			\
	}

MIXER_TEST_CASE(1, 2)
MIXER_TEST_CASE(1, 4)
MIXER_TEST_CASE(1, 8)
MIXER_TEST_CASE(2, 2)
MIXER_TEST_CASE(2, 4)
MIXER_TEST_CASE(2, 8)
MIXER_TEST_CASE(3, 2)
MIXER_TEST_CASE(4, 2)
MIXER_TEST_CASE(6, 2)
MIXER_TEST_CASE(8, 2)

ZTEST_SUITE(audio_mixer_suite, NULL, NULL, NULL, NULL, NULL);
