/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 */

#include <stdint.h>

const int16_t src_int16_1_2_1814_5000_fir[32] = {
	-7,
	7,
	63,
	87,
	-62,
	-341,
	-351,
	274,
	1130,
	982,
	-905,
	-3147,
	-2531,
	3124,
	12030,
	18851,
	18851,
	12030,
	3124,
	-2531,
	-3147,
	-905,
	982,
	1130,
	274,
	-351,
	-341,
	-62,
	87,
	63,
	7,
	-7

};

struct src_stage src_int16_1_2_1814_5000 = {
	1, 0, 1, 32, 32, 2, 1, 0, 1,
	src_int16_1_2_1814_5000_fir};
