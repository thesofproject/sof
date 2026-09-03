// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>

#include <rtos/alloc.h>

#include <stdarg.h>
#include <setjmp.h>
#include <stdint.h>
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
