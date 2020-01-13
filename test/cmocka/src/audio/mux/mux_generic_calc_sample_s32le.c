// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/mux.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

struct test_data {
	const char *name;
	uint32_t channels;
	uint8_t mask;
	int32_t *input;
	struct comp_buffer *buffer;
	int64_t expected_result;
};

static int32_t input_samples[][PLATFORM_MAX_CHANNELS] = {
	{ 0x1, 0x2, 0x4, 0x8,
	  0x10, 0x20, 0x40, 0x80, },
	{ 0x10000, 0x20000, 0x40000, 0x80000,
	  0x100000, 0x200000, 0x400000, 0x800000, },
	{ 0x1000000, 0x2000000, 0x4000000, 0x8000000,
	  0x10000000, 0x20000000, 0x40000000, 0x80000000, },
	{ 0x80000000, 0xf0000000, 0x80000000, 0xf0000000,
	  0xf0000000, 0x80000000, 0xf0000000, 0x80000000, },
};

#define TEST_CASE(channels, mask, input_index) \
	{ ("test_calc_sample_s32le_ch_" #channels "_mask_" #mask \
	   "_input_" #input_index), channels, mask, \
	 input_samples[input_index], NULL, 0 }

static struct test_data test_cases[] = {
	TEST_CASE(1, 0x0, 0),
	TEST_CASE(1, 0x0, 1),
	TEST_CASE(1, 0x0, 2),
	TEST_CASE(1, 0x1, 0),
	TEST_CASE(1, 0x1, 1),
	TEST_CASE(1, 0x1, 2),
	TEST_CASE(2, 0x0, 0),
	TEST_CASE(2, 0x0, 1),
	TEST_CASE(2, 0x0, 2),
	TEST_CASE(2, 0x1, 0),
	TEST_CASE(2, 0x1, 2),
	TEST_CASE(2, 0x2, 0),
	TEST_CASE(2, 0x2, 2),
	TEST_CASE(2, 0x3, 0),
	TEST_CASE(2, 0x3, 2),
	TEST_CASE(3, 0x1, 1),
	TEST_CASE(3, 0x7, 1),
	TEST_CASE(5, 0x4, 1),
	TEST_CASE(5, 0x12, 1),
	TEST_CASE(7, 0x10, 2),
	TEST_CASE(7, 0x11, 2),
	TEST_CASE(8, 0x0f, 1),
	TEST_CASE(8, 0x0f, 3),
	TEST_CASE(8, 0x10, 0),
	TEST_CASE(8, 0x11, 0),
	TEST_CASE(8, 0xf0, 2),
	TEST_CASE(8, 0xf0, 3),
	TEST_CASE(8, 0xff, 2),
	TEST_CASE(8, 0xff, 3),
};

static void test_calc_sample(void **state)
{
	struct test_data *td = *((struct test_data **)state);

	int64_t ret =  calc_sample_s32le(&td->buffer->stream,
					 td->channels,
					 0,
					 td->mask);

	assert_int_equal(ret, td->expected_result);
}

static int setup(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	int ch;

	td->buffer = calloc(1, sizeof(struct comp_buffer));
	td->buffer->stream.r_ptr = td->input;

	td->expected_result = 0;

	for (ch = 0; ch < td->channels; ++ch) {
		if (td->mask & BIT(ch))
			td->expected_result += td->input[ch];
	}

	return 0;
}

static int teardown(void **state)
{
	struct test_data *td = *((struct test_data **)state);

	free(td->buffer);

	return 0;
}

int main(void)
{
	int i;
	struct CMUnitTest tests[ARRAY_SIZE(test_cases)];

	for (i = 0; i < ARRAY_SIZE(test_cases); ++i) {
		tests[i].name = test_cases[i].name;
		tests[i].test_func = test_calc_sample;
		tests[i].initial_state = &test_cases[i];
		tests[i].setup_func = setup;
		tests[i].teardown_func = teardown;
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
