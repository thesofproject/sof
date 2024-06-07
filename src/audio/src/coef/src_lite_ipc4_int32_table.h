/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

/** \cond GENERATED_BY_TOOLS_TUNE_SRC */

#ifndef __SOF_AUDIO_COEFFICIENTS_SRC_src_IPC4_INT32_TABLE_H__
#define __SOF_AUDIO_COEFFICIENTS_SRC_src_IPC4_INT32_TABLE_H__

/* SRC conversions */
#include "src_ipc4_int32_1_2_4535_5000.h"
#include "src_ipc4_int32_10_21_3455_5000.h"
#include "src_ipc4_int32_1_3_4535_5000.h"
#include "src_ipc4_int32_3_2_4535_5000.h"
#include "src_ipc4_int32_8_7_4535_5000.h"
#include "src_ipc4_int32_16_21_4535_5000.h"
#include "src_ipc4_int32_20_21_4167_5000.h"

#include <stdint.h>

/* SRC table */
static const int32_t src_fir_one = Q_CONVERT_FLOAT(1, 30);
static const struct src_stage src_int32_1_1_0_0 =  { 0, 0, 1, 1, 1, 1, 1, 0, -1, &src_fir_one };
static const struct src_stage src_int32_0_0_0_0 =  { 0, 0, 0, 0, 0, 0, 0, 0,  0, &src_fir_one };
static const int src_in_fs[3] = { 32000, 44100, 48000};
static const int src_out_fs[2] = {16000, 48000};

static const struct src_stage * const src_table1[2][3] = {
	{ &src_int32_1_2_4535_5000, &src_int32_10_21_3455_5000,
	  &src_int32_1_3_4535_5000 },
	{ &src_int32_3_2_4535_5000, &src_int32_8_7_4535_5000, &src_int32_1_1_0_0 }
};

static const struct src_stage * const src_table2[2][3] = {
	{ &src_int32_1_1_0_0, &src_int32_16_21_4535_5000, &src_int32_1_1_0_0 },
	{ &src_int32_1_1_0_0, &src_int32_20_21_4167_5000, &src_int32_1_1_0_0 }
};

#endif /* __SOF_AUDIO_COEFFICIENTS_SRC_src_IPC4_INT32_TABLE_H__ */

/** \endcond */
