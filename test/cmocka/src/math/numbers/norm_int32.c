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
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <sof/math/numbers.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_math_numbers_norm_int32_for_0_equals_31(void **state)
{
	(void)state;

	int r = norm_int32(0);

	assert_int_equal(r, 31);
}

static void test_math_numbers_norm_int32_for_35_equals_10(void **state)
{
	(void)state;

	int r = norm_int32(35);

	assert_int_equal(r, 25);
}

static void test_math_numbers_norm_int32_for_2147483647_equals_0(void **state)
{
	(void)state;

	int r = norm_int32(2147483647);

	assert_int_equal(r, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_numbers_norm_int32_for_0_equals_31),
		cmocka_unit_test
			(test_math_numbers_norm_int32_for_35_equals_10),
		cmocka_unit_test
			(test_math_numbers_norm_int32_for_2147483647_equals_0)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
