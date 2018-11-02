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
