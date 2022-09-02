// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>

#include <rtos/alloc.h>
#include <sof/lib/dma.h>

#include <stdarg.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include <test_group_generator.h>

struct test_data_t {
	size_t len;
	char *before;
	char *after;
	char *func_ret;
};

struct test_data_change_t {
	size_t beg;
	size_t end;
	size_t len;
};

static struct test_data_t               *test_data;
static struct test_data_change_t *test_data_change;
static char default_char = 'a';
static char change_char  = 'b';

/* SETUP */

static int setup_change(void **state,
	const size_t change_beg, const size_t change_end)
{
	setup_alloc(test_data_change, struct test_data_change_t, 1, 0);

	test_data_change->beg = change_beg;
	test_data_change->end = change_end;
	test_data_change->len = change_end - change_beg;

	if (test_data_change->len > test_data->len)
		return -1;

	return 0;
}

static int setup_test_data(void **state, const size_t len)
{
	setup_alloc(test_data, struct test_data_t, 1, 0);

	test_data->len = len;
	test_data->before = NULL;
	test_data->after  = NULL;

	setup_alloc(test_data->before, char, (len + 1), 0);
	setup_alloc(test_data->after,  char, (len + 1), 0);

	int counter = test_data->len;
	char *it    = test_data->before;

	while (counter--)
		*(it++) = default_char;
	*it = 0; // null terminator

	return 0;
}

static int _setup(void **state, size_t data_len,
	size_t change_beg, size_t change_end)
{
	int result = 0;

	setup_part(result, setup_test_data(state, data_len));
	setup_part(result, setup_change(state, change_beg, change_end));

	return 0;
}

static int setup(void **state)
{
	return _setup(state, 8, 3, 6);
}

static int teardown(void **state)
{
	free(test_data->before);
	free(test_data->after);
	free(test_data);
	free(test_data_change);

	return 0;
}

/* Test helper functions */

static void reset_test_arr(void)
{
	int counter = test_data->len;
	char *p_bef = test_data->before;
	char *p_aft = test_data->after;

	while (counter--)
		(*p_aft++) = (*p_bef++);

	test_data->func_ret = NULL;
}

static int check_arr(const char *arr, const size_t n, const char should_be)
{
	size_t i;
	const char *it = arr;

	for (i = 0; i < n; ++i)
		if ((*it++) != should_be)
			return i;
	return -1;
}

static int check_arrs(const char *arr1, const char *arr2, const size_t n)
{
	size_t i;
	const char *it1 = arr1;
	const char *it2 = arr2;

	for (i = 0; i < n; ++i)
		if ((*it1++) != (*it2++))
			return i;
	return -1;
}

static int check_test_arrs(void)
{
	return check_arrs(
		test_data->before,
		test_data->after,
		test_data->len
	);
}

static int check_test_arrs_with_offset(const size_t offset)
{
	return check_arrs(
		test_data->before + offset,
		test_data->after  + offset,
		test_data->len    - offset
	);
}

/* Tests */

static void test_lib_lib_strcheck_self_test_arrs_equal_neg1(void **state)
{
	const char *str1 = "aaa";
	const char *str2 = "aaa";
	const int    len = 2;

	assert_int_equal(
		check_arrs(
			str1,
			str2,
			len
		),
		-1
	);
}

static void test_lib_lib_strcheck_self_test_arrs_equal_1(void **state)
{
	const char str1[] = "aaa";
	const char str2[] = "aba";
	const int     len = 4;

	assert_int_equal(
		check_arrs(
			str1,
			str2,
			len
		),
		1
	);
}

static void test_lib_lib_strcheck_self_no_change(void **state)
{
	reset_test_arr();

	assert_int_equal(
		check_test_arrs(),
		-1
	);
}

/* don't change below function names;
 *used as literals in macros generating test groups
 */
static void test_lib_lib_memset_change_base(void **state)
{
	reset_test_arr();

	test_data->func_ret = memset(
		test_data->after + test_data_change->beg,
		change_char,
		test_data_change->len
	);
}

static void test_lib_lib_bzero_change_base(void **state)
{
	change_char = 0;

	reset_test_arr();

	bzero(
		test_data->after + test_data_change->beg,
		test_data_change->len
	);
}

static void test_lib_lib_memcpy_change_base(void **state)
{
	int counter = test_data_change->len;
	char *src = malloc(sizeof(char) * counter);
	char *ptr = src;

	reset_test_arr();

	while (counter--)
		*(ptr++) = change_char;
	*ptr = 0;

	test_data->func_ret = memcpy(
		test_data->after + test_data_change->beg,
		src,
		test_data_change->len
	);

	free(src);
}

static void test_lib_lib_strcheck_change_ref_beg(void **state)
{
	const int expected = (test_data_change->len > 0)
		? test_data_change->beg
		: -1;
	assert_int_equal(expected,
		check_test_arrs());
}

static void test_lib_lib_strcheck_change_ret_beg(void **state)
{
	const int expected = (test_data_change->len > 0) ? 0 : -1;
	// baa
	assert_int_equal(expected,
		check_arrs(test_data->before + test_data_change->beg,
			test_data->func_ret,
			test_data->len - test_data_change->beg));
}

static void test_lib_lib_strcheck_change_ref_mid(void **state)
{
	assert_int_equal(-1,
		check_arr(test_data->after + test_data_change->beg,
			test_data_change->len,
			change_char));
}

static void test_lib_lib_strcheck_change_ret_mid(void **state)
{
	assert_int_equal(-1,
		check_arr(test_data->func_ret,
			test_data_change->len,
			change_char));
}

static void test_lib_lib_strcheck_change_ref_end(void **state)
{
	assert_int_equal(-1,
		check_test_arrs_with_offset(test_data_change->end));
}

static void test_lib_lib_strcheck_change_ret_end(void **state)
{
	assert_int_equal(-1,
		check_arrs(test_data->func_ret + test_data_change->len,
			test_data->before + test_data_change->end,
			test_data->len    - test_data_change->end));
}

/* TEST GROUPS GENERATORS */

#define test_prefix_base   test_lib_lib_strcheck_change
#define test_prefix_memset test_lib_lib_memset_change
#define test_prefix_bzero  test_lib_lib_bzero_change
#define test_prefix_memcpy test_lib_lib_memcpy_change

// here used are macros from "test_group_generator.h"
// define test configuration - which functions to test with
#define bind_test_group_ref_ret(bind, ...)\
	bind(ref_beg, __VA_ARGS__)\
	bind(ret_beg, __VA_ARGS__)\
	bind(ref_mid, __VA_ARGS__)\
	bind(ret_mid, __VA_ARGS__)\
	bind(ref_end, __VA_ARGS__)\
	bind(ret_end, __VA_ARGS__)

#define bind_test_group_ref(bind, ...)\
	bind(ref_beg, __VA_ARGS__)\
	bind(ref_mid, __VA_ARGS__)\
	bind(ref_end, __VA_ARGS__)

// combine test configurations with functions to test
#define bind_test_to_prefix(name, prefix_setup, action, ...)\
	action(test_prefix_base, prefix_setup, name, __VA_ARGS__)

#define bind_test_group_memset(...)\
	bind_test_group_ref_ret(bind_test_to_prefix, \
	test_prefix_memset, __VA_ARGS__)

#define bind_test_group_bzero(...)\
	bind_test_group_ref(bind_test_to_prefix, \
	test_prefix_bzero, __VA_ARGS__)

#define bind_test_group_memcpy(...)\
	bind_test_group_ref_ret(bind_test_to_prefix, \
	test_prefix_memcpy, __VA_ARGS__)

// define with what arguments to run test groups
#define run_test_group_action(action, bind_macro) \
	action(bind_macro, 3, 2, 2) /*change none  characters*/\
	action(bind_macro, 5, 0, 1) /*change first character */\
	action(bind_macro, 7, 0, 7) /*change all   characters*/\
	action(bind_macro, 9, 3, 6) /*change some  characters*/\
	action(bind_macro, 2048, 512, 1024) /*change some character - long*/

// use macros to generate function bodies and create flags
run_test_group_action(flg_test_group, test_prefix_memset)
run_test_group_action(flg_test_group, test_prefix_bzero)
run_test_group_action(flg_test_group, test_prefix_memcpy)
run_test_group_action(gen_test_group, bind_test_group_memset)
run_test_group_action(gen_test_group, bind_test_group_bzero)
run_test_group_action(gen_test_group, bind_test_group_memcpy)

/* TEST RUNNER */

int main(void)
{
	const struct CMUnitTest tests[] = {
		c_u_t(test_lib_lib_strcheck_self_test_arrs_equal_neg1),
		c_u_t(test_lib_lib_strcheck_self_test_arrs_equal_1),
		c_u_t(test_lib_lib_strcheck_self_no_change),

		// use macros to fill array with generated function calls
		run_test_group_action(use_test_group, bind_test_group_memset)
		run_test_group_action(use_test_group, bind_test_group_bzero)
		run_test_group_action(use_test_group, bind_test_group_memcpy)
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
