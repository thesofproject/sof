/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 */

#include <stdint.h>

const int16_t src_int16_2_1_1814_5000_fir[32] = {
	-7,
	63,
	-62,
	-351,
	1130,
	-905,
	-2531,
	12030,
	18851,
	3124,
	-3147,
	982,
	274,
	-341,
	87,
	7,
	7,
	87,
	-341,
	274,
	982,
	-3147,
	3124,
	18851,
	12030,
	-2531,
	-905,
	1130,
	-351,
	-62,
	63,
	-7

};

struct src_stage src_int16_2_1_1814_5000 = {
	0, 1, 2, 16, 32, 1, 2, 0, 0,
	src_int16_2_1_1814_5000_fir};
