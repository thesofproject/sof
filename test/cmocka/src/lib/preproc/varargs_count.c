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

#include <test_simple_macro.h>

#include <sof/alloc.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sof/preproc.h>
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

#define META_NAME META_COUNT_VARAGS_BEFORE_COMPILE
TEST_HERE_DECLARE(META_NAME, 0, 0,        )
TEST_HERE_DECLARE(META_NAME, 1, 1, A      )
TEST_HERE_DECLARE(META_NAME, 3, 3, A, B, C)
TEST_HERE_DECLARE(PP_NARG  , 0, 0,        )
TEST_HERE_DECLARE(PP_NARG  , 1, 1, A      )
TEST_HERE_DECLARE(PP_NARG  , 3, 3, A, B, C)

#undef C
#undef B
#undef A
#undef TEST_FUNC

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST_HERE_USE(META_NAME, 0),
		TEST_HERE_USE(META_NAME, 1),
		TEST_HERE_USE(META_NAME, 3),
		TEST_HERE_USE(PP_NARG  , 0),
		TEST_HERE_USE(PP_NARG  , 1),
		TEST_HERE_USE(PP_NARG  , 3),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}

#undef META_NAME 
#undef TEST_PREFIX
