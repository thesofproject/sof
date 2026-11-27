// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/mux/demux_copy.c:
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <zephyr/ztest.h>

#include <rtos/sof.h>
#include <rtos/string.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/uuid.h>
#include <mux/mux.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mux_test_util.h"

struct demux_copy_test_data {
	struct comp_dev *dev;
	struct comp_buffer *sinks[MUX_MAX_STREAMS];
	struct comp_buffer *source;
	void *outputs[MUX_MAX_STREAMS];
	uint32_t format;
	const uint8_t (*mask)[CONFIG_PLATFORM_MAX_CHANNELS];
};

static struct demux_copy_test_data demux_copy_td;

static const int16_t input_16b[CONFIG_PLATFORM_MAX_CHANNELS] = {
	0x101, 0x102, 0x104, 0x108,
	0x111, 0x112, 0x114, 0x118,
};

static const int32_t input_24b[CONFIG_PLATFORM_MAX_CHANNELS] = {
	0x1a1001, 0x2a2002, 0x4a4004, 0x8a8008,
	0x1b1011, 0x2b2012, 0x4b4014, 0x8b8018,
};

static const int32_t input_32b[CONFIG_PLATFORM_MAX_CHANNELS] = {
	0xd1a1001, 0xd2a2002, 0xd4a4004, 0xd8a8008,
	0xe1b1011, 0xe2b2012, 0xe4b4014, 0xe8b8018,
};

static const uint8_t masks[][MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS] = {
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

/**
 * @brief Create one sink buffer per stream, on pipeline i.
 */
static void prepare_sinks(struct demux_copy_test_data *td, size_t sample_size)
{
	uint16_t buffer_size = sample_size * CONFIG_PLATFORM_MAX_CHANNELS;
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		td->sinks[i] = mux_test_create_sink(td->dev, i, td->format,
						    CONFIG_PLATFORM_MAX_CHANNELS, buffer_size);

		td->outputs[i] = td->sinks[i]->stream.addr;
		zassert_equal(audio_stream_get_free_bytes(&td->sinks[i]->stream), buffer_size);
	}
}

/**
 * @brief Create the demux source buffer on pipeline MUX_MAX_STREAMS + 1,
 *	  holding one frame of input.
 */
static void prepare_source(struct demux_copy_test_data *td, size_t sample_size)
{
	uint16_t buffer_size = sample_size * CONFIG_PLATFORM_MAX_CHANNELS;

	td->source = mux_test_create_source(td->dev, MUX_MAX_STREAMS + 1, td->format,
					    CONFIG_PLATFORM_MAX_CHANNELS, buffer_size);

	if (td->format == SOF_IPC_FRAME_S16_LE)
		memcpy_s(td->source->stream.addr, buffer_size, input_16b, buffer_size);
	else if (td->format == SOF_IPC_FRAME_S24_4LE)
		memcpy_s(td->source->stream.addr, buffer_size, input_24b, buffer_size);
	else
		memcpy_s(td->source->stream.addr, buffer_size, input_32b, buffer_size);

	audio_stream_produce(&td->source->stream, buffer_size);
	zassert_equal(audio_stream_get_avail_bytes(&td->source->stream), buffer_size);
}

/**
 * @brief Create and prepare a demux with the given format and routing masks.
 */
static void setup_test_case(struct demux_copy_test_data *td, uint32_t format, int mask_index)
{
	const struct sof_uuid uuid = SOF_REG_UUID(demux);
	size_t sample_size = format == SOF_IPC_FRAME_S16_LE ? sizeof(int16_t) : sizeof(int32_t);
	struct comp_dev *dev;

	td->format = format;
	td->mask = masks[mask_index];

	dev = mux_test_comp_new(&uuid, td->mask);
	zassert_not_null(dev, "comp_new() failed");
	td->dev = dev;

	/* Add dummy pipeline to bypass comp_check_eos() */
	dev->pipeline = calloc(1, sizeof(*dev->pipeline));
	zassert_not_null(dev->pipeline);
	dev->expect_eos = false;

	prepare_sinks(td, sample_size);
	prepare_source(td, sample_size);

	zassert_ok(comp_prepare(td->dev));
}

/**
 * @brief Free the demux and its buffers, also after a failed test case.
 */
static void demux_copy_after(void *fixture)
{
	struct demux_copy_test_data *td = &demux_copy_td;
	int i;

	(void)fixture;

	mux_test_free_source(td->source);

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		mux_test_free_sink(td->sinks[i]);

	if (td->dev) {
		free(td->dev->pipeline);
		td->dev->pipeline = NULL;
		comp_free(td->dev);
	}

	memset(td, 0, sizeof(*td));
}

/**
 * @brief Copy one frame from the source and check every demux output.
 *
 * Channel j of output stream i takes the input channel k of the last bit k
 * set in mask[i][j], or 0 when the mask is 0.
 */
static void test_demux_copy_proc_16(struct demux_copy_test_data *td)
{
	int16_t expected_results[MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS];
	int i, j, k;

	zassert_equal(comp_copy(td->dev), 0);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		for (j = 0; j < CONFIG_PLATFORM_MAX_CHANNELS; ++j) {
			int64_t sample = 0;

			for (k = 0; k < CONFIG_PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample = input_16b[k];

			expected_results[i][j] = sample;
		}
	}

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		zassert_mem_equal(td->outputs[i], expected_results[i],
				  sizeof(expected_results[0]), "stream %d", i);
}

/** @brief S24_4LE variant of test_demux_copy_proc_16(). */
static void test_demux_copy_proc_24(struct demux_copy_test_data *td)
{
	int32_t expected_results[MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS];
	int i, j, k;

	zassert_equal(comp_copy(td->dev), 0);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		for (j = 0; j < CONFIG_PLATFORM_MAX_CHANNELS; ++j) {
			int32_t sample = 0;

			for (k = 0; k < CONFIG_PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample = input_24b[k];

			expected_results[i][j] = sample;
		}
	}

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		zassert_mem_equal(td->outputs[i], expected_results[i],
				  sizeof(expected_results[0]), "stream %d", i);
}

/** @brief S32_LE variant of test_demux_copy_proc_16(). */
static void test_demux_copy_proc_32(struct demux_copy_test_data *td)
{
	int32_t expected_results[MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS];
	int i, j, k;

	zassert_equal(comp_copy(td->dev), 0);

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		for (j = 0; j < CONFIG_PLATFORM_MAX_CHANNELS; ++j) {
			int32_t sample = 0;

			for (k = 0; k < CONFIG_PLATFORM_MAX_CHANNELS; ++k)
				if (td->mask[i][j] & BIT(k))
					sample = input_32b[k];

			expected_results[i][j] = sample;
		}
	}

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		zassert_mem_equal(td->outputs[i], expected_results[i],
				  sizeof(expected_results[0]), "stream %d", i);
}

/*
 * One test case per format and mask, named like the CMocka cases
 * test_demux_copy_<format>_mask_<index>.
 */
#define DEMUX_COPY_TEST(fmt_name, fmt, proc, mask_index)				\
	ZTEST(demux_copy, test_demux_copy_##fmt_name##_mask_##mask_index)		\
	{										\
		BUILD_ASSERT(mask_index < ARRAY_SIZE(masks));				\
		setup_test_case(&demux_copy_td, fmt, mask_index);			\
		proc(&demux_copy_td);							\
	}

#define DEMUX_COPY_TESTS(fmt_name, fmt, proc)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 0)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 1)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 2)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 3)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 4)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 5)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 6)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 7)						\
	DEMUX_COPY_TEST(fmt_name, fmt, proc, 8)

BUILD_ASSERT(ARRAY_SIZE(masks) == 9, "DEMUX_COPY_TESTS() must cover every mask");

#if CONFIG_FORMAT_S16LE
DEMUX_COPY_TESTS(s16le, SOF_IPC_FRAME_S16_LE, test_demux_copy_proc_16)
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
DEMUX_COPY_TESTS(s24_4le, SOF_IPC_FRAME_S24_4LE, test_demux_copy_proc_24)
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
DEMUX_COPY_TESTS(s32le, SOF_IPC_FRAME_S32_LE, test_demux_copy_proc_32)
#endif /* CONFIG_FORMAT_S32LE */

/**
 * @brief Register the demux driver for the suite.
 */
static void *demux_copy_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_demux_interface_init();

	return NULL;
}

ZTEST_SUITE(demux_copy, NULL, demux_copy_setup, NULL, demux_copy_after, NULL);
