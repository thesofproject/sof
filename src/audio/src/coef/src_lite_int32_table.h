/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 */

/** \cond GENERATED_BY_TOOLS_TUNE_SRC */

#ifndef __SOF_AUDIO_COEFFICIENTS_SRC_SRC_LITE_INT32_TABLE_H__
#define __SOF_AUDIO_COEFFICIENTS_SRC_SRC_LITE_INT32_TABLE_H__

/* SRC conversions */
#include "src_lite_int32_1_2_4535_5000.h"
#include "src_lite_int32_1_3_4535_5000.h"
#include "src_lite_int32_3_2_4535_5000.h"
#include "src_lite_int32_8_7_4535_5000.h"
#include "src_lite_int32_10_21_3455_5000.h"
#include "src_lite_int32_16_21_4535_5000.h"
#include "src_lite_int32_20_21_4167_5000.h"
#include <stdint.h>

/* SRC table */
const int32_t fir_one = 1073741824;
const struct src_stage src_int32_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &fir_one };
const struct src_stage src_int32_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &fir_one };
const int src_in_fs[3] = { 32000, 44100, 48000};
const int src_out_fs[2] = { 16000, 48000};
const struct src_stage * const src_table1[2][3] = {
	{ &src_int32_1_2_4535_5000, &src_int32_10_21_3455_5000,
	 &src_int32_1_3_4535_5000},
	{ &src_int32_3_2_4535_5000,
	 &src_int32_8_7_4535_5000, &src_int32_1_1_0_0
	}
};

const struct src_stage * const src_table2[2][3] = {
	{ &src_int32_1_1_0_0, &src_int32_16_21_4535_5000,
	 &src_int32_1_1_0_0},
	{ &src_int32_1_1_0_0,
	 &src_int32_20_21_4167_5000, &src_int32_1_1_0_0
	}
};

#endif /* __SOF_AUDIO_COEFFICIENTS_SRC_SRC_LITE_INT32_TABLE_H__ */

/** \endcond */
