/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 */

#include <stdint.h>

static const int32_t src_int32_2_1_2268_5000_fir[40] = {
	-96873,
	2187025,
	-6715592,
	5420785,
	16241481,
	-57566298,
	78027878,
	-3669434,
	-234950829,
	789899847,
	1310707111,
	134717528,
	-209813602,
	118075701,
	-19267409,
	-25191144,
	23196614,
	-7958316,
	-281954,
	984295,
	984295,
	-281954,
	-7958316,
	23196614,
	-25191144,
	-19267409,
	118075701,
	-209813602,
	134717528,
	1310707111,
	789899847,
	-234950829,
	-3669434,
	78027878,
	-57566298,
	16241481,
	5420785,
	-6715592,
	2187025,
	-96873

};

static const struct src_stage src_int32_2_1_2268_5000 = {
	0, 1, 2, 20, 40, 1, 2, 0, 0,
	src_int32_2_1_2268_5000_fir};
