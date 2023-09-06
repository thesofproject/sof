/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

const int16_t src_int16_1_3_1814_5000_fir[48] = {
	-9,
	-7,
	22,
	77,
	126,
	106,
	-37,
	-288,
	-520,
	-530,
	-149,
	596,
	1390,
	1701,
	1037,
	-693,
	-2922,
	-4467,
	-3943,
	-410,
	6035,
	14053,
	21417,
	25826,
	25826,
	21417,
	14053,
	6035,
	-410,
	-3943,
	-4467,
	-2922,
	-693,
	1037,
	1701,
	1390,
	596,
	-149,
	-530,
	-520,
	-288,
	-37,
	106,
	126,
	77,
	22,
	-7,
	-9

};

struct src_stage src_int16_1_3_1814_5000 = {
	1, 0, 1, 48, 48, 3, 1, 0, 2,
	src_int16_1_3_1814_5000_fir};
