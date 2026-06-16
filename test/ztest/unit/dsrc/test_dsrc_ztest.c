// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2026 Intel Corporation. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zephyr/ztest.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/dsrc.h>

/* 200 seconds stereo */
#define NUM_SAMPLES (48000 * 2 * 200)
static int32_t in_buf[NUM_SAMPLES], out_buf[NUM_SAMPLES];

/**
 * @brief Read int32_t values from a text file, one number per line.
 *
 * @param path   Path to the input text file.
 * @param buf    Destination array for the read values.
 * @param max    Maximum number of elements that fit in @a buf.
 * @return       Number of values actually read, or negative on error.
 */
static int read_data_from_file(const char *path, int32_t *buf, size_t max)
{
	FILE *f = fopen(path, "r");

	if (!f)
		return -1;

	size_t count = 0;
	char line[64];

	while (count < max && fgets(line, sizeof(line), f)) {
		char *end;
		long val = strtol(line, &end, 10);

		if (end == line)	/* skip blank / non-numeric lines */
			continue;
		buf[count++] = (int32_t)val;
	}

	fclose(f);
	return (int)count;
}

/**
 * @brief Write int32_t values to a text file, one number per line.
 *
 * @param path   Path to the output text file.
 * @param buf    Source array of values to write.
 * @param count  Number of elements to write.
 * @return       0 on success, negative on error.
 */
static int write_data_to_file(const char *path, const int32_t *buf, size_t count)
{
	FILE *f = fopen(path, "w");

	if (!f)
		return -1;

	for (size_t i = 0; i < count; i++)
		fprintf(f, "%d\n", buf[i]);

	fclose(f);
	return 0;
}

/**
 * @brief Compare two text files line by line for equality.
 *
 * @param path_a  Path to the first file.
 * @param path_b  Path to the second file (expected / reference).
 * @return        0 when the files are identical, or the 1-based line number
 *                of the first mismatch.  Returns -1 on open/read error.
 */
static int compare_data_files(const char *path_a, const char *path_b)
{
	FILE *fa = fopen(path_a, "r");
	FILE *fb = fopen(path_b, "r");

	if (!fa || !fb) {
		if (fa)
			fclose(fa);
		if (fb)
			fclose(fb);
		return -1;
	}

	char line_a[64];
	char line_b[64];
	int line = 1;

	while (true) {
		char *a = fgets(line_a, sizeof(line_a), fa);
		char *b = fgets(line_b, sizeof(line_b), fb);

		if (!a && !b)		/* both reached EOF at the same time */
			break;

		if (!a || !b || strcmp(line_a, line_b) != 0) {
			fclose(fa);
			fclose(fb);
			return line;	/* first mismatching line (1-based) */
		}
		line++;
	}

	fclose(fa);
	fclose(fb);
	return 0;
}

/* Just to avoid dependency on components.c, copy-paste impl here */
void cir_buf_copy(void *src, void *src_addr, void *src_end, void *dst,
	  void *dst_addr, void *dst_end, size_t byte_size)
{
	size_t bytes = byte_size;
	size_t bytes_src;
	size_t bytes_dst;
	size_t bytes_copied;
	uint8_t *in = (uint8_t *)src;
	uint8_t *out = (uint8_t *)dst;

	while (bytes) {
		bytes_src = cir_buf_bytes_without_wrap(in, src_end);
		bytes_dst = cir_buf_bytes_without_wrap(out, dst_end);
		bytes_copied = MIN(bytes_src, bytes_dst);
		bytes_copied = MIN(bytes, bytes_copied);
		memcpy_s(out, bytes_copied, in, bytes_copied);
		bytes -= bytes_copied;
		in = cir_buf_wrap(in + bytes_copied, src_addr, src_end);
		out = cir_buf_wrap(out + bytes_copied, dst_addr, dst_end);
	}
}

/**************************** Test cases *******************************/

ZTEST(dsrc_suite, test_48000_48000)
{
	struct dsrc dsrc;
	dsrc_init(&dsrc, 2);
	dsrc_set_rate(&dsrc, 48000, 48000);

	struct cir_buf_ptr in = { in_buf, &in_buf[NUM_SAMPLES], in_buf };
	struct cir_buf_ptr out = { out_buf, &out_buf[NUM_SAMPLES], out_buf };
	size_t added_frames = dsrc_process(&dsrc, &in, &out, 48000 * 100);

	/* same frequencies: zero added frames expected */
	zassert_equal(added_frames, 0);
}

ZTEST(dsrc_suite, test_48000_48007)
{
	struct dsrc dsrc;
	dsrc_init(&dsrc, 2);
	dsrc_set_rate(&dsrc, 48000, 48007);

	struct cir_buf_ptr in = { in_buf, &in_buf[NUM_SAMPLES], in_buf };
	struct cir_buf_ptr out = { out_buf, &out_buf[NUM_SAMPLES], out_buf };
	size_t added_frames = dsrc_process(&dsrc, &in, &out, 48000 * 100);

	/* 7 added frames per each input second are expected */
	zassert_equal(added_frames, 7 * 100);
}

ZTEST(dsrc_suite, test_48000_48007_1ms_chunks)
{
	struct dsrc dsrc;
	dsrc_init(&dsrc, 2);
	dsrc_set_rate(&dsrc, 48000, 48007);

	size_t frames = 48000 * 100;
	size_t total_added_frames = 0;

	struct cir_buf_ptr in = { in_buf, &in_buf[48 * 2], in_buf };
	struct cir_buf_ptr out = { out_buf, &out_buf[48 * 2 + 2], out_buf };

	while (frames) {
		size_t added_frames = dsrc_process(&dsrc, &in, &out, 48);
		/* with 1 ms chunks and small freq diff, periodically one extra frame is added */
		zassert_true(added_frames == 0 || added_frames == 1);
		total_added_frames += added_frames;
		frames -= 48;
	}

	/* 7 added frames per each input second are expected */
	zassert_equal(total_added_frames, 7 * 100);
}

ZTEST(dsrc_suite, test_file_48000_48000)
{
	int n = read_data_from_file(TEST_DATA_DIR "/test_input.txt", in_buf, NUM_SAMPLES);
	zassert_true(n > 0, "failed to read test input data");

	struct dsrc dsrc;
	dsrc_init(&dsrc, 2);
	dsrc_set_rate(&dsrc, 48000, 48000);

	struct cir_buf_ptr in = { in_buf, &in_buf[NUM_SAMPLES], in_buf };
	struct cir_buf_ptr out = { out_buf, &out_buf[NUM_SAMPLES], out_buf };
	size_t added_frames = dsrc_process(&dsrc, &in, &out, n / 2);

	/* same frequencies: zero added frames expected */
	zassert_equal(added_frames, 0);

	int ret = write_data_to_file(TEST_DATA_DIR "/test_output_1.txt", out_buf, n + added_frames * 2);
	zassert_equal(ret, 0, "failed to write test output data");

	/* no resampling: output should match input */
	int diff = compare_data_files(TEST_DATA_DIR "/test_input.txt", TEST_DATA_DIR "/test_output_1.txt");
	zassert_equal(diff, 0, "test output differs from expected at line %d", diff);
}

ZTEST(dsrc_suite, test_file_100_102)
{
	int n = read_data_from_file(TEST_DATA_DIR "/test_input.txt", in_buf, NUM_SAMPLES);
	zassert_true(n > 0, "failed to read test input data");

	struct dsrc dsrc;
	dsrc_init(&dsrc, 1);
	dsrc_set_rate(&dsrc, 100, 102);

	struct cir_buf_ptr in = { in_buf, &in_buf[NUM_SAMPLES], in_buf };
	struct cir_buf_ptr out = { out_buf, &out_buf[NUM_SAMPLES], out_buf };
	size_t added_frames = dsrc_process(&dsrc, &in, &out, n);

	/* 2 added frames per each input second are expected */
	zassert_equal(added_frames, (double)n / 100 * 2);

	int ret = write_data_to_file(TEST_DATA_DIR "/test_output_2.txt", out_buf, n + added_frames);
	zassert_equal(ret, 0, "failed to write test output data");

	/* compare output with expected data */
	int diff = compare_data_files(TEST_DATA_DIR "/test_output_2.txt", TEST_DATA_DIR "/test_expected_2.txt");
	zassert_equal(diff, 0, "test output differs from expected at line %d", diff);
}

ZTEST_SUITE(dsrc_suite, NULL, NULL, NULL, NULL, NULL);
