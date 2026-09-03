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

static void test_lib_lib_rstrlen_for_empty_str_equals_0(void **state)
{
	int r;
	const char *str = "";

	(void)state;

	r = rstrlen(str);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrlen_for_null_str_equals_0(void **state)
{
	int r;
	const char *str = "\0";

	(void)state;

	r = rstrlen(str);
	assert_int_equal(r, 0);
}

static void test_lib_lib_rstrlen_for_a_equals_1(void **state)
{
	int r;
	const char *str = "a";

	(void)state;

	r = rstrlen(str);
	assert_int_equal(r, 1);
}

static void test_lib_lib_rstrlen_for_abcd_equals_4(void **state)
{
	int r;
	const char *str = "abcd";

	(void)state;

	r = rstrlen(str);
	assert_int_equal(r, 4);
}

static void test_lib_lib_rstrlen_for_verylongstr(void **state)
{
	int r;
	int i;
	const int size = 2048;
	char str[size + 1];

	for (i = 0; i < size; ++i)
		str[i] = 'a';
	str[size] = 0;

	(void)state;

	r = rstrlen(str);
	assert_int_equal(r, size);
}

static void test_lib_lib_rstrlen_for_multiple_nulls_equals_5(void **state)
{
	int r;
	const char *str = "Lorem\0Ipsum\0";

	(void)state;

	r = rstrlen(str);
	assert_int_equal(r, 5);
}

int main(void)
{
	#define c_u_t(x) cmocka_unit_test(x)
	// CHECK: Avoid CamelCase: <CMUnitTest>
	#define c_m_unit_test CMUnitTest
	const struct c_m_unit_test tests[] = {
		c_u_t(test_lib_lib_rstrlen_for_empty_str_equals_0),
		c_u_t(test_lib_lib_rstrlen_for_null_str_equals_0),
		c_u_t(test_lib_lib_rstrlen_for_a_equals_1),
		c_u_t(test_lib_lib_rstrlen_for_abcd_equals_4),
		c_u_t(test_lib_lib_rstrlen_for_verylongstr),
		c_u_t(test_lib_lib_rstrlen_for_multiple_nulls_equals_5),
	};
	#undef c_m_unit_test
	#undef c_u_t

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
