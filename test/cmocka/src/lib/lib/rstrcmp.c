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

static void test_lib_lib_rstrcmp_for_a_and_a_equals_0(void **state)
{
	int r;
	const char *str1 = "a";
	const char *str2 = "a";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrcmp_for_a_and_b_is_negative(void **state)
{
	int r;
	const char *str1 = "a";
	const char *str2 = "b";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_true(r < 0);
}

static void test_lib_lib_rstrcmp_for_b_and_a_is_positive(void **state)
{
	int r;
	const char *str1 = "b";
	const char *str2 = "a";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_true(r > 0);
}

static void test_lib_lib_rstrcmp_for_empty_and_null_str_equals_0(void **state)
{
	int r;
	const char *str1 = "";
	const char *str2 = "\0";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrcmp_for_abc_and_abcd_is_negative(void **state)
{
	int r;
	const char *str1 = "abc";
	const char *str2 = "abcd";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_true(r < 0);
}

static void test_lib_lib_rstrcmp_for_abcd_and_abc_is_positive(void **state)
{
	int r;
	const char *str1 = "abcd";
	const char *str2 = "abc";

	(void) state;

	r = rstrcmp(str1, str2);

	assert_true(r > 0);
}

static void test_lib_lib_rstrcmp_for_abc_and_aBc_is_positive(void **state)
{
	int r;
	const char *str1 = "abc";
	const char *str2 = "aBc";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_true(r > 0);
}

static void test_lib_lib_rstrcmp_for_same_multinull_equals_0(void **state)
{
	int r;
	const char *str1 = "Lorem\0Ipsum\0";
	const char *str2 = "Lorem\0Ipsum\0";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrcmp_for_diff_after_null_equals_0(void **state)
{
	int r;
	const char *str1 = "Lorem\0Ipsum\0";
	const char *str2 = "Lorem\0IPzum\0";

	(void) state;

	r = rstrcmp(str1, str2);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrcmp_for_verylongstrings_equals_0(void **state)
{
	int r;
	int i;
	const int size = 2048;
	char str1[size+1];
	char str2[size+1];

	for (i = 0; i < size; ++i) {
		str1[i] = 'a';
		str2[i] = 'a';
	}
	str1[size] = 0;
	str2[size] = 0;

	(void) state;

	r = rstrcmp(str1, str2);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrcmp_for_verylongstrings_is_positive(void **state)
{
	int r;
	int i;
	const int size = 2048;
	char str1[size+1];
	char str2[size+1];

	for (i = 0; i < size-1; ++i) {
		str1[i] = 'a';
		str2[i] = 'a';
	}
	str1[size-1] = 'a';
	str2[size-1] = 'A';
	str1[size] = 0;
	str2[size] = 0;

	(void) state;

	r = rstrcmp(str1, str2);
	assert_true(r > 0);
}

static void test_lib_lib_rstrcmp_for_verylongstrings_is_negative(void **state)
{
	int r;
	int i;
	const int size = 2048;
	char str1[size+1];
	char str2[size+1];

	for (i = 0; i < size-1; ++i) {
		str1[i] = 'a';
		str2[i] = 'a';
	}
	str1[size-1] = 'A';
	str2[size-1] = 'a';
	str1[size] = 0;
	str2[size] = 0;

	(void) state;

	r = rstrcmp(str1, str2);
	assert_true(r < 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_a_and_a_equals_0),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_a_and_b_is_negative),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_b_and_a_is_positive),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_empty_and_null_str_equals_0),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_abc_and_abcd_is_negative),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_abcd_and_abc_is_positive),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_abc_and_aBc_is_positive),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_same_multinull_equals_0),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_diff_after_null_equals_0),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_verylongstrings_equals_0),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_verylongstrings_is_positive),
		cmocka_unit_test(
			test_lib_lib_rstrcmp_for_verylongstrings_is_negative),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
