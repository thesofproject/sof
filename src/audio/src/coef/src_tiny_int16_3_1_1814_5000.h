/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

const int16_t src_int16_3_1_1814_5000_fir[48] = {
	-7,
	58,
	-28,
	-397,
	1042,
	-520,
	-2957,
	10540,
	19370,
	4526,
	-3350,
	777,
	446,
	-390,
	79,
	17,
	-5,
	94,
	-216,
	-112,
	1275,
	-2191,
	-307,
	16063,
	16063,
	-307,
	-2191,
	1275,
	-112,
	-216,
	94,
	-5,
	17,
	79,
	-390,
	446,
	777,
	-3350,
	4527,
	19370,
	10539,
	-2957,
	-520,
	1042,
	-397,
	-28,
	58,
	-7

};

struct src_stage src_int16_3_1_1814_5000 = {
	0, 1, 3, 16, 48, 1, 3, 0, 0,
	src_int16_3_1_1814_5000_fir};
