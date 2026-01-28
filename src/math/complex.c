// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//

#include <rtos/symbol.h>
#include <sof/audio/format.h>
#include <sof/math/icomplex32.h>
#include <sof/math/sqrt.h>
#include <stdint.h>

/* sofm_icomplex32_to_polar() - Convert (re, im) complex number to polar. */
void sofm_icomplex32_to_polar(struct icomplex32 *complex, struct ipolar32 *polar)
{
	struct icomplex32 c = *complex;
	int64_t squares_sum;
	int32_t sqrt_arg;
	int32_t acos_arg;
	int32_t acos_val;

	/* Calculate square of magnitudes Q1.31, result is Q2.62 */
	squares_sum = (int64_t)c.real * c.real + (int64_t)c.imag * c.imag;

	/* Square root */
	sqrt_arg = Q_SHIFT_RND(squares_sum, 62, 30);
	polar->magnitude = sofm_sqrt_int32(sqrt_arg); /* Q2.30 */

	/* Avoid divide by zero and ambiguous angle for a zero vector. */
	if (polar->magnitude == 0) {
		polar->angle = 0;
		return;
	}

	/* Calculate phase angle with acos( complex->real / polar->magnitude) */
	acos_arg = sat_int32((((int64_t)c.real) << 29) / polar->magnitude); /* Q2.30 */
	acos_val = acos_fixed_32b(acos_arg);				    /* Q3.29 */
	polar->angle = (c.imag < 0) ? -acos_val : acos_val;
}
EXPORT_SYMBOL(sofm_icomplex32_to_polar);

/* sofm_ipolar32_to_complex() - Convert complex number from polar to normal (re, im) format. */
void sofm_ipolar32_to_complex(struct ipolar32 *polar, struct icomplex32 *complex)
{
	struct cordic_cmpx cexp;
	int32_t phase;
	int32_t magnitude;

	/* The conversion can happen in-place, so load copies of the values first */
	magnitude = polar->magnitude;
	phase = Q_SHIFT_RND(polar->angle, 29, 28); /* Q3.29 to Q2.28 */
	cmpx_exp_32b(phase, &cexp);		   /* Q2.30 */
	complex->real = sat_int32(Q_MULTSR_32X32((int64_t)magnitude, cexp.re, 30, 30, 31));
	complex->imag = sat_int32(Q_MULTSR_32X32((int64_t)magnitude, cexp.im, 30, 30, 31));
}
EXPORT_SYMBOL(sofm_ipolar32_to_complex);
