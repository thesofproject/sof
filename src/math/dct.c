// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/matrix.h>
#include <sof/math/dct.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <errno.h>
#include <stdint.h>

#define PI_Q29 Q_CONVERT_FLOAT(3.1415926536, 29)
#define TWO_PI_Q24 Q_CONVERT_FLOAT(6.2831853072, 24)
#define ONE_OVER_SQRT_TWO Q_CONVERT_FLOAT(0.7071067812, 31)
#define HALF_Q1 Q_CONVERT_FLOAT(0.5, 1)
#define TWO_Q29 Q_CONVERT_FLOAT(2, 29)

/* Return a matrix for discrete cosine transform (DCT). The DCT type is hard
 * coded to DCT-II with orthogonal matrix. See
 * https://en.wikipedia.org/wiki/Discrete_cosine_transform#DCT-II
 *
 * The size is (num_in, num_out). Currently there are not other input parameters.
 * TODO: Add option for no ortho norm.
 */

/**
 * \brief Initialize a 16 bit DCT matrix. The actual DCT transform is a matrix
 * multiply with the returned matrix.
 * \param[in,out]  dct  In input provide DCT type and size, in output the DCT matrix
 */
int dct_initialize_16(struct dct_plan_16 *dct)
{
	int16_t dct_val;
	int32_t arg;
	int32_t cos;
	int32_t c1;
	int16_t c2;
	int16_t nk;
	int n;
	int k;

	if (dct->type != DCT_II || dct->ortho != true)
		return -EINVAL;

	if (dct->num_in < 1 || dct->num_out < 1)
		return -EINVAL;

	if (dct->num_in > DCT_MATRIX_SIZE_MAX || dct->num_out > DCT_MATRIX_SIZE_MAX)
		return -EINVAL;

	dct->matrix = mat_matrix_alloc_16b(dct->num_in, dct->num_out, 15);
	if (!dct->matrix)
		return -ENOMEM;

	c1 = PI_Q29 / dct->num_in;
	arg = Q_SHIFT_RND(TWO_Q29 / dct->num_in, 29, 12);
	c2 = sqrt_int16(arg); /* Q4.12 */
	for (n = 0; n < dct->num_in; n++) {
		for (k = 0; k < dct->num_out; k++) {
			/* Note: Current int16_t nk works up to DCT_MATRIX_SIZE_MAX = 91 */
			nk = (Q_SHIFT_LEFT(n, 0, 1) + HALF_Q1) * Q_SHIFT_LEFT(k, 0, 1); /*Q14.2 */
			arg = Q_MULTSR_32X32((int64_t)c1, nk, 29, 2, 24); /* Q8.24 */
			/* Note: Q8.24 works up to DCT_MATRIX_SIZE_MAX = 42 */
			arg %= TWO_PI_Q24;
			cos = cos_fixed_32b(Q_SHIFT_LEFT(arg, 24, 28)); /* Q1.31 */
			dct_val = Q_MULTSR_32X32((int64_t)cos, c2, 31, 12, 15); /* Q1.15 */
			if (k == 0)
				dct_val = Q_MULTSR_32X32((int64_t)dct_val,
							 ONE_OVER_SQRT_TWO, 15, 31, 15);

			mat_set_scalar_16b(dct->matrix, n, k, dct_val);
		}
	}

	return 0;
}
