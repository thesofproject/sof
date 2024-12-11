/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

__cold_rodata static const int16_t src_int16_6_1_1814_5000_fir[96] = {
	-7,
	53,
	-2,
	-422,
	945,
	-191,
	-3220,
	9049,
	19615,
	6038,
	-3432,
	476,
	644,
	-424,
	57,
	31,
	-8,
	76,
	-85,
	-347,
	1176,
	-991,
	-2466,
	12047,
	18753,
	3229,
	-3189,
	954,
	327,
	-372,
	90,
	13,
	-6,
	94,
	-183,
	-193,
	1284,
	-1837,
	-1123,
	14806,
	17104,
	799,
	-2613,
	1220,
	37,
	-285,
	101,
	0,
	0,
	101,
	-285,
	37,
	1220,
	-2613,
	799,
	17104,
	14805,
	-1123,
	-1837,
	1284,
	-193,
	-183,
	94,
	-6,
	13,
	90,
	-372,
	327,
	954,
	-3189,
	3229,
	18754,
	12047,
	-2466,
	-991,
	1176,
	-347,
	-85,
	76,
	-8,
	31,
	57,
	-424,
	644,
	476,
	-3432,
	6038,
	19615,
	9049,
	-3220,
	-191,
	945,
	-422,
	-2,
	53,
	-7

};

static const struct src_stage src_int16_6_1_1814_5000 = {
	0, 1, 6, 16, 96, 1, 6, 0, 0,
	src_int16_6_1_1814_5000_fir};
