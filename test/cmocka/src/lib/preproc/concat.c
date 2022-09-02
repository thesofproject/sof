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
	const int r = test_func(__VA_ARGS__);\
	(void)state;\
\
	assert_int_equal(r, should_be);\
}
#define A 1
#define B 2
#define C 3

TEST_HERE_DECLARE(META_CONCAT, 1_nothing,       1, A,     )
TEST_HERE_DECLARE(META_CONCAT, nothing_2,       2,  , B   )
TEST_HERE_DECLARE(META_CONCAT, 1_2,            12, A, B   )
TEST_HERE_DECLARE(META_CONCAT_SEQ, 1_nothing,   1, A,     )
TEST_HERE_DECLARE(META_CONCAT_SEQ, nothing_2,   2,  , B   )
TEST_HERE_DECLARE(META_CONCAT_SEQ, 1_2,        12, A, B   )
TEST_HERE_DECLARE(META_CONCAT_SEQ, 1_2_3,     123, A, B, C)

#undef C
#undef B
#undef A
#undef TEST_FUNC

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST_HERE_USE(META_CONCAT,     1_nothing),
		TEST_HERE_USE(META_CONCAT,     nothing_2),
		TEST_HERE_USE(META_CONCAT,     1_2      ),

		TEST_HERE_USE(META_CONCAT_SEQ, 1_nothing),
		TEST_HERE_USE(META_CONCAT_SEQ, nothing_2),
		TEST_HERE_USE(META_CONCAT_SEQ, 1_2      ),
		TEST_HERE_USE(META_CONCAT_SEQ, 1_2_3    ),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}

#undef TEST_PREFIX
