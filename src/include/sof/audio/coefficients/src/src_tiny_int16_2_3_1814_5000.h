/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

const int16_t src_int16_2_3_1814_5000_fir[48] = {
	-9,
	21,
	126,
	-37,
	-520,
	-150,
	1391,
	1036,
	-2922,
	-3942,
	6034,
	21417,
	25826,
	14053,
	-409,
	-4467,
	-693,
	1701,
	596,
	-530,
	-288,
	106,
	77,
	-8,
	-8,
	77,
	106,
	-288,
	-530,
	596,
	1701,
	-693,
	-4467,
	-409,
	14052,
	25826,
	21417,
	6035,
	-3942,
	-2922,
	1036,
	1392,
	-150,
	-520,
	-37,
	126,
	21,
	-9

};

struct src_stage src_int16_2_3_1814_5000 = {
	1, 1, 2, 24, 48, 3, 2, 0, 1,
	src_int16_2_3_1814_5000_fir};
