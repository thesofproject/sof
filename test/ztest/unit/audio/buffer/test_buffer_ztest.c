// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/buffer/:
// buffer_new.c, buffer_wrap.c, buffer_write.c and buffer_copy.c
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <rtos/string.h>

#include <stdint.h>
#include <string.h>

#define TEST_BUFFER_SIZE       256
#define TEST_SMALL_BUFFER_SIZE 10

/**
 * @brief Allocate a non shared component buffer of the given size.
 * @param[in] size Buffer size in bytes.
 * @return The new buffer, never NULL.
 */
static struct comp_buffer *test_buffer_new(uint32_t size)
{
	struct sof_ipc_buffer desc = {.size = size};
	struct comp_buffer *buf = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buf, "Failed to allocate a %u byte buffer.", size);

	return buf;
}

/**
 * @brief Check that a buffer is empty and its pointers are coincident.
 * @param[in] buf Buffer to check.
 * @param[in] size Expected free space in bytes.
 */
static void assert_buffer_empty(struct comp_buffer *buf, uint32_t size)
{
	zassert_equal(audio_stream_get_avail_bytes(&buf->stream), 0);
	zassert_equal(audio_stream_get_free_bytes(&buf->stream), size);
	zassert_equal_ptr(buf->stream.w_ptr, buf->stream.r_ptr);
}

/**
 * @brief Verify that a new buffer is empty and has the requested free space
 */
ZTEST(audio_buffer_suite, test_audio_buffer_new)
{
	struct comp_buffer *buf = test_buffer_new(TEST_BUFFER_SIZE);

	assert_buffer_empty(buf, TEST_BUFFER_SIZE);

	buffer_free(buf);
}

/**
 * @brief Verify a write and read back of 10 bytes in a 256 byte buffer
 */
ZTEST(audio_buffer_suite, test_audio_buffer_write_10_bytes_out_of_256_and_read_back)
{
	uint8_t bytes[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	struct comp_buffer *buf = test_buffer_new(TEST_BUFFER_SIZE);

	assert_buffer_empty(buf, TEST_BUFFER_SIZE);

	memcpy_s(buf->stream.w_ptr, TEST_BUFFER_SIZE, &bytes, sizeof(bytes));
	comp_update_buffer_produce(buf, sizeof(bytes));

	zassert_equal(audio_stream_get_avail_bytes(&buf->stream), sizeof(bytes));
	zassert_equal(audio_stream_get_free_bytes(&buf->stream), TEST_BUFFER_SIZE - sizeof(bytes));
	zassert_equal_ptr(buf->stream.w_ptr, (char *)buf->stream.r_ptr + sizeof(bytes));
	zassert_mem_equal(buf->stream.r_ptr, &bytes, sizeof(bytes));

	comp_update_buffer_consume(buf, sizeof(bytes));

	assert_buffer_empty(buf, TEST_BUFFER_SIZE);

	buffer_free(buf);
}

/**
 * @brief Verify that filling a 10 byte buffer leaves no free space
 */
ZTEST(audio_buffer_suite, test_audio_buffer_fill_10_bytes)
{
	uint8_t bytes[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	struct comp_buffer *buf = test_buffer_new(TEST_SMALL_BUFFER_SIZE);

	assert_buffer_empty(buf, TEST_SMALL_BUFFER_SIZE);

	memcpy_s(buf->stream.w_ptr, TEST_SMALL_BUFFER_SIZE, &bytes, sizeof(bytes));
	comp_update_buffer_produce(buf, sizeof(bytes));

	zassert_equal(audio_stream_get_avail_bytes(&buf->stream), sizeof(bytes));
	zassert_equal(audio_stream_get_free_bytes(&buf->stream), 0);
	zassert_equal_ptr(buf->stream.w_ptr, buf->stream.r_ptr);

	buffer_free(buf);
}

/**
 * @brief Verify that writing past the end of a full buffer wraps around
 *
 * The first ten bytes fill the buffer, the next five overwrite its head, and
 * the oldest five bytes must then read back before the five new ones.
 */
ZTEST(audio_buffer_suite, test_audio_buffer_write_fill_10_bytes_and_write_5)
{
	uint8_t bytes[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	uint8_t more_bytes[5] = {10, 11, 12, 13, 14};
	uint8_t ref_1[5] = {5, 6, 7, 8, 9};
	uint8_t ref_2[5] = {10, 11, 12, 13, 14};
	struct comp_buffer *buf = test_buffer_new(TEST_SMALL_BUFFER_SIZE);
	uint8_t *ptr;
	int i;

	assert_buffer_empty(buf, TEST_SMALL_BUFFER_SIZE);

	for (i = 0; i < ARRAY_SIZE(bytes); i++) {
		ptr = audio_stream_write_frag(&buf->stream, i, sizeof(uint8_t));
		*ptr = bytes[i];
	}
	comp_update_buffer_produce(buf, sizeof(bytes));

	zassert_equal(audio_stream_get_avail_bytes(&buf->stream), sizeof(bytes));
	zassert_equal(audio_stream_get_free_bytes(&buf->stream), 0);
	zassert_equal_ptr(buf->stream.w_ptr, buf->stream.r_ptr);

	for (i = 0; i < ARRAY_SIZE(more_bytes); i++) {
		ptr = audio_stream_write_frag(&buf->stream, i, sizeof(uint8_t));
		*ptr = more_bytes[i];
	}
	comp_update_buffer_produce(buf, sizeof(more_bytes));

	zassert_equal(audio_stream_get_avail_bytes(&buf->stream), TEST_SMALL_BUFFER_SIZE);
	zassert_equal(audio_stream_get_free_bytes(&buf->stream), 0);
	zassert_equal_ptr(buf->stream.w_ptr, buf->stream.r_ptr);

	for (i = 0; i < ARRAY_SIZE(ref_1); i++) {
		ptr = audio_stream_read_frag(&buf->stream, i, sizeof(uint8_t));
		zassert_equal(*ptr, ref_1[i], "Byte %d of the first half is %u, expected %u", i,
			      *ptr, ref_1[i]);
	}

	comp_update_buffer_consume(buf, sizeof(ref_1));

	for (i = 0; i < ARRAY_SIZE(ref_2); i++) {
		ptr = audio_stream_read_frag(&buf->stream, i, sizeof(uint8_t));
		zassert_equal(*ptr, ref_2[i], "Byte %d of the second half is %u, expected %u", i,
			      *ptr, ref_2[i]);
	}

	buffer_free(buf);
}

/**
 * @brief Verify that a source with too few bytes to copy reports underrun
 */
ZTEST(audio_buffer_suite, test_audio_buffer_copy_underrun)
{
	struct comp_buffer *src = test_buffer_new(TEST_BUFFER_SIZE);
	struct comp_buffer *snk = test_buffer_new(TEST_BUFFER_SIZE);
	int copy_bytes;

	comp_update_buffer_produce(src, 10);
	copy_bytes = audio_stream_can_copy_bytes(&src->stream, &snk->stream, 16);

	zassert_equal(audio_stream_get_avail_bytes(&src->stream), 10);
	zassert_equal(copy_bytes, -1);

	buffer_free(src);
	buffer_free(snk);
}

/**
 * @brief Verify that a sink with too little free space reports overrun
 */
ZTEST(audio_buffer_suite, test_audio_buffer_copy_overrun)
{
	struct comp_buffer *src = test_buffer_new(TEST_BUFFER_SIZE);
	struct comp_buffer *snk = test_buffer_new(TEST_BUFFER_SIZE);
	int copy_bytes;

	comp_update_buffer_produce(src, 16);
	comp_update_buffer_produce(snk, 246);
	copy_bytes = audio_stream_can_copy_bytes(&src->stream, &snk->stream, 16);

	zassert_equal(audio_stream_get_avail_bytes(&src->stream), 16);
	zassert_equal(audio_stream_get_free_bytes(&snk->stream), 10);
	zassert_equal(copy_bytes, 1);

	buffer_free(src);
	buffer_free(snk);
}

/**
 * @brief Verify that a copy of zero bytes is always possible
 */
ZTEST(audio_buffer_suite, test_audio_buffer_copy_success)
{
	struct comp_buffer *src = test_buffer_new(TEST_BUFFER_SIZE);
	struct comp_buffer *snk = test_buffer_new(TEST_BUFFER_SIZE);
	int copy_bytes;

	comp_update_buffer_produce(src, 10);
	copy_bytes = audio_stream_can_copy_bytes(&src->stream, &snk->stream, 0);

	zassert_equal(audio_stream_get_avail_bytes(&src->stream), 10);
	zassert_equal(copy_bytes, 0);

	buffer_free(src);
	buffer_free(snk);
}

/**
 * @brief Verify that the copy size is limited by the free space of the sink
 */
ZTEST(audio_buffer_suite, test_audio_buffer_copy_fit_space_constraint)
{
	struct comp_buffer *src = test_buffer_new(TEST_BUFFER_SIZE);
	struct comp_buffer *snk = test_buffer_new(TEST_BUFFER_SIZE);
	int copy_bytes;

	comp_update_buffer_produce(src, 16);
	comp_update_buffer_produce(snk, 246);
	copy_bytes = audio_stream_get_copy_bytes(&src->stream, &snk->stream);

	zassert_equal(audio_stream_get_avail_bytes(&src->stream), 16);
	zassert_equal(audio_stream_get_free_bytes(&snk->stream), 10);
	zassert_equal(copy_bytes, 10);

	buffer_free(src);
	buffer_free(snk);
}

/**
 * @brief Verify that the copy size is limited by the data available in the
 *        source when the sink is empty
 */
ZTEST(audio_buffer_suite, test_audio_buffer_copy_fit_no_space_constraint)
{
	struct comp_buffer *src = test_buffer_new(TEST_BUFFER_SIZE);
	struct comp_buffer *snk = test_buffer_new(TEST_BUFFER_SIZE);
	int copy_bytes;

	comp_update_buffer_produce(src, 16);
	copy_bytes = audio_stream_get_copy_bytes(&src->stream, &snk->stream);

	zassert_equal(audio_stream_get_avail_bytes(&src->stream), 16);
	zassert_equal(copy_bytes, 16);

	buffer_free(src);
	buffer_free(snk);
}

/**
 * @brief Define and initialize the audio buffer test suite
 */
ZTEST_SUITE(audio_buffer_suite, NULL, NULL, NULL, NULL, NULL);
