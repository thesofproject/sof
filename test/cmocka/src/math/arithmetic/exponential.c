// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>
#include <string.h>

#include <sof/math/exp_fcn.h>
#include <sof/audio/format.h>
#include <rtos/string.h>
#include <sof/common.h>

#define ULP_TOLERANCE 5.60032793
#define ULP_SCALE 0.0000999999999749
/* testvector in Q4.28, -5 to +5 */
       static const int32_t iv[128] = {
	    -1342177280, -1321040630, -1299903980, -1278767330, -1257630680, -1236494030,
	    -1215357380, -1194220729, -1173084079, -1151947429, -1130810779, -1109674129,
	    -1088537479, -1067400829, -1046264179, -1025127529, -1003990879, -982854229,
	    -961717579,	 -940580929,  -919444278,  -898307628,	-877170978,  -856034328,
	    -834897678,	 -813761028,  -792624378,  -771487728,	-750351078,  -729214428,
	    -708077778,	 -686941128,  -665804477,  -644667827,	-623531177,  -602394527,
	    -581257877,	 -560121227,  -538984577,  -517847927,	-496711277,  -475574627,
	    -454437977,	 -433301327,  -412164677,  -391028026,	-369891376,  -348754726,
	    -327618076,	 -306481426,  -285344776,  -264208126,	-243071476,  -221934826,
	    -200798176,	 -179661526,  -158524876,  -137388226,	-116251575,  -95114925,
	    -73978275,	 -52841625,   -31704975,   -10568325,	10568325,    31704975,
	    52841625,	 73978275,    95114925,	   116251575,	137388226,   158524876,
	    179661526,	 200798176,   221934826,   243071476,	264208126,   285344776,
	    306481426,	 327618076,   348754726,   369891376,	391028026,   412164677,
	    433301327,	 454437977,   475574627,   496711277,	517847927,   538984577,
	    560121227,	 581257877,   602394527,   623531177,	644667827,   665804477,
	    686941128,	 708077778,   729214428,   750351078,	771487728,   792624378,
	    813761028,	 834897678,   856034328,   877170978,	898307628,   919444278,
	    940580929,	 961717579,   982854229,   1003990879,	1025127529,  1046264179,
	    1067400829,	 1088537479,  1109674129,  1130810779,	1151947429,  1173084079,
	    1194220729,	 1215357380,  1236494030,  1257630680,	1278767330,  1299903980,
	    1321040630,	 1342177280};


static void test_math_arithmetic_exponential_fixed(void **state)
{
	(void)state;

	int32_t iv[128];
	int32_t accum;
	int i;
	double a_i;
	double max_ulp;

	for (i = 0; i < ARRAY_SIZE(iv); i++) {
		a_i = (double)iv[i] / (1 << 28);
		accum = exp_int32(iv[i]);
		max_ulp = fabs(exp(a_i) - (double)accum / (1 << 23)) / ULP_SCALE;

		if (max_ulp > ULP_TOLERANCE) {
			printf("%s: ULP for %.16f: value = %.16f, Exp = %.16f\n", __func__,
			       max_ulp, (double)iv[i] / (1 << 28), (double)accum / (1 << 23));
			assert_true(max_ulp <= ULP_TOLERANCE);
		}
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_arithmetic_exponential_fixed)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
