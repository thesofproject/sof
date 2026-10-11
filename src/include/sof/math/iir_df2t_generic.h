/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/**
 * @file sof/math/iir_df2t_generic.h
 * @brief Generic C implementation of Direct Form II Transposed IIR filtering.
 * @ingroup dsp_math_api
 */

#ifndef __IIR_DF2T_GENERIC_H__
#define __IIR_DF2T_GENERIC_H__

#include <stdint.h>

/**
 * @brief Filter a 16-bit PCM sample using Direct Form 2 Transposed IIR filter.
 * @param iir Pointer to DF2T filter state.
 * @param x Input 16-bit sample (Q1.15).
 * @return Filtered 16-bit sample (Q1.15).
 */
static inline int16_t iir_df2t_s16(struct iir_state_df2t *iir, int16_t x)
{
	return sat_int16(Q_SHIFT_RND(iir_df2t(iir, ((int32_t)x) << 16), 31, 15));
}

/**
 * @brief Filter a 24-bit PCM sample using Direct Form 2 Transposed IIR filter.
 * @param iir Pointer to DF2T filter state.
 * @param x Input 24-bit sample (Q1.23 in 32-bit container).
 * @return Filtered 24-bit sample (Q1.23).
 */
static inline int32_t iir_df2t_s24(struct iir_state_df2t *iir, int32_t x)
{
	return sat_int24(Q_SHIFT_RND(iir_df2t(iir, x << 8), 31, 23));
}

/**
 * @brief Filter a 32-bit PCM input sample producing a 16-bit output sample using DF2T.
 * @param iir Pointer to DF2T filter state.
 * @param x Input 32-bit sample (Q1.31).
 * @return Filtered 16-bit sample (Q1.15).
 */
static inline int16_t iir_df2t_s32_s16(struct iir_state_df2t *iir, int32_t x)
{
	return sat_int16(Q_SHIFT_RND(iir_df2t(iir, x), 31, 15));
}

/**
 * @brief Filter a 32-bit PCM input sample producing a 24-bit output sample using DF2T.
 * @param iir Pointer to DF2T filter state.
 * @param x Input 32-bit sample (Q1.31).
 * @return Filtered 24-bit sample (Q1.23).
 */
static inline int32_t iir_df2t_s32_s24(struct iir_state_df2t *iir, int32_t x)
{
	return sat_int24(Q_SHIFT_RND(iir_df2t(iir, x), 31, 23));
}

#endif /* __IIR_DF2T_GENERIC_H__ */

