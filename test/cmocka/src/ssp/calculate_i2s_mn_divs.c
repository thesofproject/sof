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
 * Author: Kamil Kulesza <kamil.kulesza@linux.intel.com>
 */
#include <sof/ssp.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

const uint32_t M_19200_KHZ = 19200000;
const uint32_t M_24576_KHZ = 24576000;
const uint32_t F_441_KHZ = 44100;

static void test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_2ch_0sep_0fep_equals_147n_1000m(void **state)
{
	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_19200_KHZ, F_441_KHZ, 2, 32, 0, 0);
	assert_int_equal(mn->m, 147);
	assert_int_equal(mn->n, 1000);
}

static void test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_4ch_0sep_0fep_equals_147n_500m(void **state)
{
	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_19200_KHZ, F_441_KHZ, 4, 32, 0, 0);
	assert_int_equal(mn->m, 147);
	assert_int_equal(mn->n, 500);
}

static void test_ssp_calculate_i2s_mn_divs_for_mclk_24_576_mhz_44_1khz_32b_8ch_0sep_0fep_equals_441n_320m(void **state)
{
	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_24576_KHZ, F_441_KHZ, 8, 32, 0, 0);
	assert_int_equal(mn->m, 147);
	assert_int_equal(mn->n, 320);
}

static void test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_24b_2ch_8sep_0fep_equals_147n_1000m(void **state)
{
	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_19200_KHZ, F_441_KHZ, 2, 24, 8, 0);
	assert_int_equal(mn->m, 147);
	assert_int_equal(mn->n, 1000);
}

static void test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_2ch_0sep_32fep_equals_441n_2000m(void **state)
{

	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_19200_KHZ, F_441_KHZ, 2, 32, 0, 32);
	assert_int_equal(mn->m, 441);
	assert_int_equal(mn->n, 2000);
}

static void test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_8ch_0sep_0fep_assert_null(void **state)
{
	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_19200_KHZ, F_441_KHZ, 8, 32, 0, 0);
	assert_null(mn);
}

static void test_ssp_calculate_i2s_mn_divs_for_mclk_24_576_mhz_44_1khz_32b_3ch_0sep_0fep_equals_441n_2560m(void **state)
{
	struct i2s_mn_divs *mn = calculate_i2s_mn_divs(M_24576_KHZ, F_441_KHZ, 3, 32, 0, 0);
	assert_int_equal(mn->m, 441);
	assert_int_equal(mn->n, 2560);
}


int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_2ch_0sep_0fep_equals_147n_1000m),
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_4ch_0sep_0fep_equals_147n_500m),
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_24_576_mhz_44_1khz_32b_8ch_0sep_0fep_equals_441n_320m),
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_24b_2ch_8sep_0fep_equals_147n_1000m),
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_2ch_0sep_32fep_equals_441n_2000m),
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_19_2_mhz_44_1khz_32b_8ch_0sep_0fep_assert_null),
		cmocka_unit_test(test_ssp_calculate_i2s_mn_divs_for_mclk_24_576_mhz_44_1khz_32b_3ch_0sep_0fep_equals_441n_2560m),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
