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
