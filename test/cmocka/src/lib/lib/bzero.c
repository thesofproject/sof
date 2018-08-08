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
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>
 */

#include <sof/alloc.h>

#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

struct test_data_t {
	size_t len;
	char str[0];
};

struct test_data_zero_middle_t {
	int beg;
	int end;
	int len;
};

static struct test_data_t *test_data;
static struct test_data_zero_middle_t *test_data_zero_middle;
static const char default_char = 'a';

static void reset_test_arr(void)
{
	int i;

	for (i = 0; i < test_data->len-1; ++i)
		test_data->str[i] = default_char;
	test_data->str[test_data->len] = 0;
}

static int setup(void **state)
{
	const int len = 6;

	test_data = malloc(sizeof(struct test_data_t)
		+ len * sizeof(test_data[0]));
	test_data->len = len;

	const int zero_beg = 2;
	const int zero_end = 4;
	// aa000aa... - pattern of zeroing

	test_data_zero_middle = malloc(sizeof(struct test_data_zero_middle_t));
	test_data_zero_middle->beg = zero_beg;
	test_data_zero_middle->end = zero_end;
	test_data_zero_middle->len = zero_end - zero_beg;

	return 0;
}

static int teardown(void **state)
{
	free(test_data);

	return 0;
}

static int check_arr(char *arr, size_t n, char should_be)
{
	size_t i;

	for (i = 0; i < n; ++i)
		if (arr[i] != should_be)
			return i;
	return -1;
}

static int check_test_arr(char should_be)
{
	return check_arr(test_data->str, test_data->len-1, should_be);
}

static int check_test_arr_with_offset(
	int offset_beg, int offset_end, char should_be)
{
	return check_arr(
		test_data->str+offset_beg,
		test_data->len+offset_end-1,
		should_be
	);
}

/* Tests*/
static void test_lib_lib_bzero_check_test_arr(void **state)
{
	reset_test_arr();

	(void) state;

	assert_int_equal(check_test_arr(default_char), -1);
}

static void test_lib_lib_bzero_check_test_arr_with_offset(void **state)
{
	const int it = 3;

	reset_test_arr();
	test_data->str[it] = default_char+1;

	(void) state;

	assert_int_equal(check_test_arr(default_char), it);
}

static void test_lib_lib_bzero_char_zero_none(void **state)
{
	reset_test_arr();

	(void) state;

	bzero(test_data->str, 0);
	assert_int_equal(check_test_arr(default_char), -1);
}

static void test_lib_lib_bzero_char_zero_all(void **state)
{
	reset_test_arr();

	(void) state;

	bzero(test_data->str, test_data->len);
	assert_int_equal(check_test_arr(0), -1);
}

static void test_lib_lib_bzero_char_zero_middle_beg(void **state)
{
	reset_test_arr();

	(void) state;

	bzero(test_data->str+test_data_zero_middle->beg,
		test_data_zero_middle->len);
	assert_int_equal(check_test_arr(default_char),
		test_data_zero_middle->beg);
}

static void test_lib_lib_bzero_char_zero_middle_mid(void **state)
{
	(void) state;

	assert_int_equal(check_test_arr_with_offset(
		test_data_zero_middle->beg, -test_data_zero_middle->beg,
		0), test_data_zero_middle->len);
}

static void test_lib_lib_bzero_char_zero_middle_end(void **state)
{
	(void) state;

	assert_int_equal(check_test_arr_with_offset(
		test_data_zero_middle->end, -test_data_zero_middle->end,
		default_char), -1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lib_lib_bzero_check_test_arr),
		cmocka_unit_test(test_lib_lib_bzero_check_test_arr_with_offset),
		cmocka_unit_test(test_lib_lib_bzero_char_zero_none),
		cmocka_unit_test(test_lib_lib_bzero_char_zero_all),
		cmocka_unit_test(test_lib_lib_bzero_char_zero_middle_beg),
		cmocka_unit_test(test_lib_lib_bzero_char_zero_middle_mid),
		cmocka_unit_test(test_lib_lib_bzero_char_zero_middle_end),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
