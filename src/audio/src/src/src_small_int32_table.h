/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_AUDIO_COEFFICIENTS_SRC_SRC_SMALL_INT32_TABLE_H__
#define __SOF_AUDIO_COEFFICIENTS_SRC_SRC_SMALL_INT32_TABLE_H__

/* SRC conversions */
#include "src_small_int32_1_2_2268_5000.h"
#include "src_small_int32_1_2_4535_5000.h"
#include "src_small_int32_1_3_2268_5000.h"
#include "src_small_int32_1_3_4535_5000.h"
#include "src_small_int32_2_1_2268_5000.h"
#include "src_small_int32_2_1_4535_5000.h"
#include "src_small_int32_2_3_4535_5000.h"
#include "src_small_int32_3_1_2268_5000.h"
#include "src_small_int32_3_1_4535_5000.h"
#include "src_small_int32_3_2_4535_5000.h"
#include "src_small_int32_3_4_4535_5000.h"
#include "src_small_int32_4_3_4535_5000.h"
#include "src_small_int32_4_5_4535_5000.h"
#include "src_small_int32_5_4_4535_5000.h"
#include "src_small_int32_5_6_4354_5000.h"
#include "src_small_int32_6_5_4354_5000.h"
#include "src_small_int32_7_8_4535_5000.h"
#include "src_small_int32_8_7_4535_5000.h"
#include "src_small_int32_20_21_4167_5000.h"
#include "src_small_int32_21_20_4167_5000.h"
#include "../src.h"
#include <stdint.h>

/* SRC table */
int32_t fir_one = 1073741824;
struct src_stage src_int32_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int32_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[7] = { 8000, 16000, 24000, 32000, 44100, 48000, 50000};
int src_out_fs[7] = { 8000, 16000, 24000, 32000, 44100, 48000, 50000};
struct src_stage *src_table1[7][7] = {
	{ &src_int32_1_1_0_0, &src_int32_1_2_4535_5000,
	 &src_int32_1_3_4535_5000, &src_int32_1_2_2268_5000,
	 &src_int32_0_0_0_0, &src_int32_1_3_2268_5000,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_2_3_4535_5000,
	 &src_int32_1_2_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_1_3_4535_5000, &src_int32_0_0_0_0
	},
	{ &src_int32_3_1_4535_5000, &src_int32_3_2_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_3_4_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_1_2_4535_5000,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4535_5000,
	 &src_int32_2_1_4535_5000, &src_int32_4_3_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_2_3_4535_5000, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_21_20_4167_5000,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4535_5000,
	 &src_int32_3_1_4535_5000, &src_int32_2_1_4535_5000,
	 &src_int32_3_2_4535_5000, &src_int32_8_7_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_6_5_4354_5000
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_5_4_4535_5000,
	 &src_int32_1_1_0_0}
};
struct src_stage *src_table2[7][7] = {
	{ &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_2_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_1_2_4535_5000,
	 &src_int32_0_0_0_0},
	{ &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_2268_5000,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_7_8_4535_5000,
	 &src_int32_0_0_0_0},
	{ &src_int32_3_1_2268_5000,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_20_21_4167_5000,
	 &src_int32_1_1_0_0, &src_int32_4_5_4535_5000
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_5_6_4354_5000,
	 &src_int32_1_1_0_0}
};

#endif /* __SOF_AUDIO_COEFFICIENTS_SRC_SRC_SMALL_INT32_TABLE_H__ */
