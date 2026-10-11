/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/**
 * @file sof/math/dct.h
 * @brief Discrete Cosine Transform (DCT) matrix generation and planning.
 * @ingroup dsp_math_api
 */

#ifndef __SOF_MATH_DCT_H__
#define __SOF_MATH_DCT_H__

#include <sof/math/matrix.h>
#include <stdint.h>
#include <string.h>

#define DCT_MATRIX_SIZE_MAX 42 /* Max matrix size where cos() argument fits Q7.24 */

enum dct_type {
	DCT_I = 0,
	DCT_II,
};

struct dct_plan_16 {
	struct mat_matrix_16b *matrix;
	int num_in;
	int num_out;
	enum dct_type type;
	bool ortho;
};

/**
 * @brief Initialize a 16-bit Discrete Cosine Transform plan and compute transform matrix.
 * @param mod Processing module requesting the plan (for memory allocation).
 * @param dct Pointer to DCT plan configuration structure.
 * @return 0 on success, negative error code on failure.
 */
int mod_dct_initialize_16(struct processing_module *mod, struct dct_plan_16 *dct);

/**
 * @brief Free memory allocated for a 16-bit DCT plan matrix.
 * @param mod Processing module owning the plan.
 * @param dct Pointer to DCT plan structure to free.
 * @return 0 on success, negative error code on failure.
 */
int mod_dct_free_16(struct processing_module *mod, struct dct_plan_16 *dct);

#endif /* __SOF_MATH_DCT_H__ */
