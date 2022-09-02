// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>

#include <test_simple_macro.h>

#include <rtos/alloc.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <sof/trace/preproc.h>
#include <sof/sof.h>

#define TEST_PREFIX test_lib_preproc

#define TEST_FUNC(prefix, test_func, postfix, should_be, ...)\
static void META_CONCAT_SEQ_DELIM_(prefix, test_func, postfix)(void **state)\
{\
	const char *r = META_QUOTE(test_func(__VA_ARGS__));\
	(void)state;\
\
	assert_string_equal(r, should_be);\
}
#define TEST() FINAL

TEST_HERE_DECLARE(META_DEFER, 0,\
	"TEST"\
	, 0, TEST)
TEST_HERE_DECLARE(META_DEFER, 1,\
	"TEST _META_EMPTY"\
	" ()"\
	, 1, TEST)
TEST_HERE_DECLARE(META_DEFER, 2,\
	"TEST _META_EMPTY _META_EMPTY"\
	" () ()"\
	, 2, TEST)
TEST_HERE_DECLARE(META_DEFER, 3,\
	"TEST _META_EMPTY _META_EMPTY _META_EMPTY"\
	" () () ()"\
	, 3, TEST)
TEST_HERE_DECLARE(META_DEFER, 4,\
	"TEST _META_EMPTY _META_EMPTY _META_EMPTY _META_EMPTY"\
	" () () () ()"\
	, 4, TEST)
TEST_HERE_DECLARE(META_DEFER, 5,\
	"TEST _META_EMPTY _META_EMPTY _META_EMPTY _META_EMPTY _META_EMPTY"\
	" () () () () ()"\
	, 5, TEST)

#undef TEST
#undef TEST_FUNC

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST_HERE_USE(META_DEFER, 0),
		TEST_HERE_USE(META_DEFER, 1),
		TEST_HERE_USE(META_DEFER, 2),
		TEST_HERE_USE(META_DEFER, 3),
		TEST_HERE_USE(META_DEFER, 4),
		TEST_HERE_USE(META_DEFER, 5),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}

#undef TEST_PREFIX
