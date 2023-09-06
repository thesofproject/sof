/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_COEFFICIENTS_SRC_SRC_STD_INT32_TABLE_H__
#define __SOF_AUDIO_COEFFICIENTS_SRC_SRC_STD_INT32_TABLE_H__

/* SRC conversions */
#include "src_std_int32_1_2_2268_5000.h"
#include "src_std_int32_1_2_4535_5000.h"
#include "src_std_int32_1_3_2268_5000.h"
#include "src_std_int32_1_3_4535_5000.h"
#include "src_std_int32_2_1_2268_5000.h"
#include "src_std_int32_2_1_4535_5000.h"
#include "src_std_int32_2_3_4535_5000.h"
#include "src_std_int32_3_1_2268_5000.h"
#include "src_std_int32_3_1_4535_5000.h"
#include "src_std_int32_3_2_4535_5000.h"
#include "src_std_int32_3_4_4535_5000.h"
#include "src_std_int32_4_3_4535_5000.h"
#include "src_std_int32_4_5_4535_5000.h"
#include "src_std_int32_5_4_4535_5000.h"
#include "src_std_int32_5_6_4354_5000.h"
#include "src_std_int32_5_7_4535_5000.h"
#include "src_std_int32_6_5_4354_5000.h"
#include "src_std_int32_7_8_4535_5000.h"
#include "src_std_int32_8_7_2468_5000.h"
#include "src_std_int32_8_7_4535_5000.h"
#include "src_std_int32_8_21_3239_5000.h"
#include "src_std_int32_10_21_4535_5000.h"
#include "src_std_int32_20_7_2976_5000.h"
#include "src_std_int32_20_21_4167_5000.h"
#include "src_std_int32_21_20_4167_5000.h"
#include "src_std_int32_21_40_3968_5000.h"
#include "src_std_int32_21_80_3968_5000.h"
#include "src_std_int32_32_21_4535_5000.h"
#include "src_std_int32_40_21_3968_5000.h"
#include "../src.h"
#include <stdint.h>

/* SRC table */
int32_t fir_one = 1073741824;
struct src_stage src_int32_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
struct src_stage src_int32_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
int src_in_fs[15] = { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100,
	 48000, 50000, 64000, 88200, 96000, 176400, 192000};
int src_out_fs[10] = { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100,
	 48000, 50000};
struct src_stage *src_table1[10][15] = {
	{ &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_2_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_1_3_4535_5000,
	 &src_int32_1_2_2268_5000, &src_int32_0_0_0_0,
	 &src_int32_1_3_2268_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_21_80_3968_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_2_2268_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_2_1_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_2_3_4535_5000, &src_int32_1_2_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_1_3_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_21_40_3968_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_3_1_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_3_2_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_3_4_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_1_2_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_2_1_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_2_1_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_4_3_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_2_3_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_21_20_4167_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_2_1_4535_5000, &src_int32_32_21_4535_5000,
	 &src_int32_2_1_4535_5000, &src_int32_3_1_4535_5000,
	 &src_int32_8_7_4535_5000, &src_int32_2_1_4535_5000,
	 &src_int32_3_2_4535_5000, &src_int32_8_7_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_6_5_4354_5000,
	 &src_int32_3_4_4535_5000, &src_int32_8_7_2468_5000,
	 &src_int32_1_2_4535_5000, &src_int32_8_21_3239_5000,
	 &src_int32_1_2_2268_5000},
	{ &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_5_4_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	}
};
struct src_stage *src_table2[10][15] = {
	{ &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_2_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_1_2_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_7_8_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_2_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_7_8_4535_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_2_1_2268_5000, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_0_0_0_0, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0},
	{ &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_1_1_0_0, &src_int32_7_8_4535_5000,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	},
	{ &src_int32_3_1_2268_5000, &src_int32_20_7_2976_5000,
	 &src_int32_2_1_2268_5000, &src_int32_1_1_0_0,
	 &src_int32_40_21_3968_5000, &src_int32_1_1_0_0,
	 &src_int32_1_1_0_0, &src_int32_20_21_4167_5000,
	 &src_int32_1_1_0_0, &src_int32_4_5_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_10_21_4535_5000,
	 &src_int32_1_1_0_0, &src_int32_5_7_4535_5000,
	 &src_int32_1_2_4535_5000},
	{ &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_5_6_4354_5000,
	 &src_int32_1_1_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0,
	 &src_int32_0_0_0_0, &src_int32_0_0_0_0
	}
};

#endif /* __SOF_AUDIO_COEFFICIENTS_SRC_SRC_STD_INT32_TABLE_H__ */
