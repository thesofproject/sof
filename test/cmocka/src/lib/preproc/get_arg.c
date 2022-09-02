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

#include <sof/trace/preproc.h>
#include <sof/sof.h>

static void test_lib_preproc_get_arg_1(void **state)
{
	const char* what_we_get = _META_GET_ARG_1("a", "B", "c", "D", "e");
	const char* what_we_should_get = "a";

	(void)state;

	assert_string_equal(what_we_get, what_we_should_get);
}

static void test_lib_preproc_get_arg_2(void **state)
{
	const char* what_we_get = _META_GET_ARG_2("a", "B", "c", "D", "e");
	const char* what_we_should_get = "B";

	(void)state;

	assert_string_equal(what_we_get, what_we_should_get);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lib_preproc_get_arg_1),
		cmocka_unit_test(test_lib_preproc_get_arg_2),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
