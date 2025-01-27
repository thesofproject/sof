/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016-2023 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_MATH_LUT_TRIG_H__
#define __SOF_MATH_LUT_TRIG_H__

#include <stdint.h>

#define SOFM_LUT_SINE_C_Q20	341782638	/* 2 * SINE_NQUART / pi in Q12.20 */
#define SOFM_LUT_SINE_NQUART	512		/* Must be 2^N */
#define SOFM_LUT_SINE_SIZE	(SOFM_LUT_SINE_NQUART + 1)

extern const uint16_t sofm_lut_sine_table_s16[];

/* Sine lookup table read */
static inline int32_t sofm_sine_lookup_16b(int idx)
{
	uint16_t s;
	int i1;

	i1 = idx & (2 * SOFM_LUT_SINE_NQUART - 1);
	if (i1 > SOFM_LUT_SINE_NQUART)
		i1 = 2 * SOFM_LUT_SINE_NQUART - i1;

	s = sofm_lut_sine_table_s16[i1];
	if (idx > 2 * SOFM_LUT_SINE_NQUART)
		return -((int32_t)s);

	return (int32_t)s;
}

/**
 * Compute fixed point sine with table lookup and interpolation
 * @param w	Input angle in radians Q4.28
 * @return	Sine value Q1.15
 */
static inline int16_t sofm_lut_sin_fixed_16b(int32_t w)
{
	int64_t idx;
	int32_t sine;
	int32_t frac;
	int32_t delta;
	int32_t s0;
	int32_t s1;
	int64_t idx_tmp;

	/* Q4.28 x Q12.20 -> Q16.48 --> Q16.31*/
	idx_tmp = ((int64_t)w * SOFM_LUT_SINE_C_Q20) >> 17;
	idx = (idx_tmp >> 31); /* Shift to Q0 */
	frac = (int32_t)(idx_tmp - (idx << 31)); /* Get fraction Q1.31*/
	s0 = sofm_sine_lookup_16b(idx); /* Q1.16 */
	s1 = sofm_sine_lookup_16b(idx + 1); /* Q1.16 */
	delta = s1 - s0; /* Q1.16 */
	sine = s0 + q_mults_32x32(frac, delta, Q_SHIFT_BITS_64(31, 16, 16)); /* Q1.16 */
	return sat_int16((sine + 1) >> 1); /* Round to Q1.15 */
}

#endif /* __SOF_MATH_LUT_TRIG_H__ */
