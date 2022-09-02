/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* Matrix calculation related functions */

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

int dct_initialize_16(struct dct_plan_16 *dct);

#endif /* __SOF_MATH_DCT_H__ */
