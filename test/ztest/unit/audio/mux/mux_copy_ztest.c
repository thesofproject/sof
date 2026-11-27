// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/mux/mux_copy.c:
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

struct mux_copy_test_data {
	struct comp_dev *dev;
	struct comp_buffer *sources[MUX_MAX_STREAMS];
	struct comp_buffer *sink;
	void *output;
	uint32_t format;
	const uint8_t (*mask)[CONFIG_PLATFORM_MAX_CHANNELS];
};

static struct mux_copy_test_data mux_copy_td;

static const int16_t input_16b[MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS] = {
	{ 0x101, 0x102, 0x104, 0x108,
	  0x111, 0x112, 0x114, 0x118, },
	{ 0x201, 0x202, 0x204, 0x208,
	  0x211, 0x212, 0x214, 0x218, },
	{ 0x301, 0x302, 0x304, 0x308,
	  0x311, 0x312, 0x314, 0x318, },
	{ 0x401, 0x402, 0x404, 0x408,
	  0x411, 0x412, 0x414, 0x418, },
};

static const int32_t input_24b[MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS] = {
	{ 0x1a1001, 0x2a2002, 0x4a4004, 0x8a8008,
	  0x1b1011, 0x2b2012, 0x4b4014, 0x8b8018, },
	{ 0x1a1101, 0x2a2102, 0x4a4104, 0x8a8108,
	  0x1b1111, 0x2b2112, 0x4b4114, 0x8b8118, },
	{ 0x1a1201, 0x2a2202, 0x4a4204, 0x8a8208,
	  0x1b1211, 0x2b2212, 0x4b4214, 0x8b8218, },
	{ 0x1a1401, 0x2a2402, 0x4a4404, 0x8a8408,
	  0x1b1411, 0x2b2412, 0x4b4414, 0x8b8418, },
};

static const int32_t input_32b[MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS] = {
	{ 0xd1a1001, 0xd2a2002, 0xd4a4004, 0xd8a8008,
	  0xe1b1011, 0xe2b2012, 0xe4b4014, 0xe8b8018, },
	{ 0xd1a1101, 0xd2a2102, 0xd4a4104, 0xd8a8108,
	  0xe1b1111, 0xe2b2112, 0xe4b4114, 0xe8b8118, },
	{ 0xd1a1201, 0xd2a2202, 0xd4a4204, 0xd8a8208,
	  0xe1b1211, 0xe2b2212, 0xe4b4214, 0xe8b8218, },
	{ 0xd1a1401, 0xd2a2402, 0xd4a4404, 0xd8a8408,
	  0xe1b1411, 0xe2b2412, 0xe4b4414, 0xe8b8418, },
};

static const uint8_t masks[][MUX_MAX_STREAMS][CONFIG_PLATFORM_MAX_CHANNELS] = {
	{ { 0x01, }, },
	{ { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80 }, },
	{ { },
	  { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80 }, },
	{ { },
	  { },
	  { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80 }, },
	{ { },
	  { },
	  { },
	  { 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x80 }, },
	{ { 0x01, },
	  { 0x00, 0x01, },
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10 }, },
	{ { 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10 },
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x01, },
	  { 0x01, }, },
	{ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, },
	  { 0x10, 0x08, 0x04, 0x02, 0x01, }, },
	{ { 0x01, },
	  { 0x00, 0x01, },
	  { 0x00, 0x00, 0x01, },
	  { 0x00, 0x00, 0x00, 0x01, }, },
};

/**
 * @brief Create the mux sink buffer on pipeline MUX_MAX_STREAMS + 1.
 */
static void prepare_sink(struct mux_copy_test_data *td, size_t sample_size)
{
	uint16_t buffer_size = sample_size * CONFIG_PLATFORM_MAX_CHANNELS;

	td->sink = mux_test_create_sink(td->dev, MUX_MAX_STREAMS + 1, td->format,
					CONFIG_PLATFORM_MAX_CHANNELS, buffer_size);
	td->output = td->sink->stream.addr;

	zassert_equal(audio_stream_get_free_bytes(&td->sink->stream), buffer_size);
}

/**
 * @brief Create one source buffer per stream, on pipeline i, holding one frame
 *	  of the input of stream i.
 */
static void prepare_sources(struct mux_copy_test_data *td, size_t sample_size)
{
	uint16_t buffer_size = sample_size * CONFIG_PLATFORM_MAX_CHANNELS;
	int i;

	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		td->sources[i] = mux_test_create_source(td->dev, i, td->format,
							CONFIG_PLATFORM_MAX_CHANNELS, buffer_size);

		if (td->format == SOF_IPC_FRAME_S16_LE)
			memcpy_s(td->sources[i]->stream.addr, buffer_size, &input_16b[i],
				 buffer_size);
		else if (td->format == SOF_IPC_FRAME_S24_4LE)
			memcpy_s(td->sources[i]->stream.addr, buffer_size, &input_24b[i],
				 buffer_size);
		else
			memcpy_s(td->sources[i]->stream.addr, buffer_size, &input_32b[i],
				 buffer_size);

		audio_stream_produce(&td->sources[i]->stream, buffer_size);
		zassert_equal(audio_stream_get_avail_bytes(&td->sources[i]->stream),
			      buffer_size);
	}
}

/**
 * @brief Create and prepare a mux with the given format and routing masks.
 */
static void setup_test_case(struct mux_copy_test_data *td, uint32_t format, int mask_index)
{
	const struct sof_uuid uuid = SOF_REG_UUID(mux);
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

	prepare_sink(td, sample_size);
	prepare_sources(td, sample_size);

	zassert_ok(comp_prepare(td->dev));
}

/**
 * @brief Free the mux and its buffers, also after a failed test case.
 */
static void mux_copy_after(void *fixture)
{
	struct mux_copy_test_data *td = &mux_copy_td;
	int i;

	(void)fixture;

	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		mux_test_free_source(td->sources[i]);

	mux_test_free_sink(td->sink);

	if (td->dev) {
		free(td->dev->pipeline);
		td->dev->pipeline = NULL;
		comp_free(td->dev);
	}

	memset(td, 0, sizeof(*td));
}

/**
 * @brief Copy one frame from every source and check the mux output.
 *
 * Output channel i takes the sample of the last stream and input channel
 * whose mask has bit i set, or 0 when no mask routes to it.
 */
static void test_mux_copy_proc_16(struct mux_copy_test_data *td)
{
	int16_t expected_result[CONFIG_PLATFORM_MAX_CHANNELS];
	int i, j, k;

	zassert_equal(comp_copy(td->dev), 0);

	for (i = 0; i < CONFIG_PLATFORM_MAX_CHANNELS; ++i) {
		int32_t sample = 0;

		for (j = 0; j < MUX_MAX_STREAMS; ++j) {
			for (k = 0; k < CONFIG_PLATFORM_MAX_CHANNELS; ++k) {
				if (td->mask[j][k] & BIT(i))
					sample = input_16b[j][k];
			}
		}
		expected_result[i] = sample;
	}

	zassert_mem_equal(td->output, expected_result, sizeof(expected_result));
}

/** @brief S24_4LE variant of test_mux_copy_proc_16(). */
static void test_mux_copy_proc_24(struct mux_copy_test_data *td)
{
	int32_t expected_result[CONFIG_PLATFORM_MAX_CHANNELS];
	int i, j, k;

	zassert_equal(comp_copy(td->dev), 0);

	for (i = 0; i < CONFIG_PLATFORM_MAX_CHANNELS; ++i) {
		int32_t sample = 0;

		for (j = 0; j < MUX_MAX_STREAMS; ++j) {
			for (k = 0; k < CONFIG_PLATFORM_MAX_CHANNELS; ++k) {
				if (td->mask[j][k] & BIT(i))
					sample = input_24b[j][k];
			}
		}
		expected_result[i] = sample;
	}

	zassert_mem_equal(td->output, expected_result, sizeof(expected_result));
}

/** @brief S32_LE variant of test_mux_copy_proc_16(). */
static void test_mux_copy_proc_32(struct mux_copy_test_data *td)
{
	int32_t expected_result[CONFIG_PLATFORM_MAX_CHANNELS];
	int i, j, k;

	zassert_equal(comp_copy(td->dev), 0);

	for (i = 0; i < CONFIG_PLATFORM_MAX_CHANNELS; ++i) {
		int32_t sample = 0;

		for (j = 0; j < MUX_MAX_STREAMS; ++j) {
			for (k = 0; k < CONFIG_PLATFORM_MAX_CHANNELS; ++k) {
				if (td->mask[j][k] & BIT(i))
					sample = input_32b[j][k];
			}
		}
		expected_result[i] = sample;
	}

	zassert_mem_equal(td->output, expected_result, sizeof(expected_result));
}

/*
 * One test case per format and mask, named like the CMocka cases
 * test_mux_copy_<format>_mask_<index>.
 */
#define MUX_COPY_TEST(fmt_name, fmt, proc, mask_index)					\
	ZTEST(mux_copy, test_mux_copy_##fmt_name##_mask_##mask_index)			\
	{										\
		BUILD_ASSERT(mask_index < ARRAY_SIZE(masks));				\
		setup_test_case(&mux_copy_td, fmt, mask_index);				\
		proc(&mux_copy_td);							\
	}

#define MUX_COPY_TESTS(fmt_name, fmt, proc)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 0)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 1)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 2)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 3)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 4)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 5)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 6)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 7)						\
	MUX_COPY_TEST(fmt_name, fmt, proc, 8)

BUILD_ASSERT(ARRAY_SIZE(masks) == 9, "MUX_COPY_TESTS() must cover every mask");

#if CONFIG_FORMAT_S16LE
MUX_COPY_TESTS(s16le, SOF_IPC_FRAME_S16_LE, test_mux_copy_proc_16)
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
MUX_COPY_TESTS(s24_4le, SOF_IPC_FRAME_S24_4LE, test_mux_copy_proc_24)
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
MUX_COPY_TESTS(s32le, SOF_IPC_FRAME_S32_LE, test_mux_copy_proc_32)
#endif /* CONFIG_FORMAT_S32LE */

/**
 * @brief Register the mux driver for the suite.
 */
static void *mux_copy_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_mux_interface_init();

	return NULL;
}

ZTEST_SUITE(mux_copy, NULL, mux_copy_setup, NULL, mux_copy_after, NULL);
