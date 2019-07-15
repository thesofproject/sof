/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

const int16_t src_int16_3_2_1814_5000_fir[48] = {
	-6,
	58,
	-28,
	-397,
	1043,
	-520,
	-2957,
	10538,
	19370,
	4527,
	-3350,
	777,
	447,
	-390,
	80,
	16,
	-6,
	95,
	-216,
	-112,
	1275,
	-2191,
	-307,
	16062,
	16062,
	-307,
	-2191,
	1275,
	-112,
	-216,
	95,
	-6,
	16,
	80,
	-390,
	447,
	777,
	-3350,
	4526,
	19370,
	10539,
	-2957,
	-520,
	1043,
	-397,
	-28,
	58,
	-6

};

struct src_stage src_int16_3_2_1814_5000 = {
	1, 2, 3, 16, 48, 2, 3, 0, 0,
	src_int16_3_2_1814_5000_fir};
