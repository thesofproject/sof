/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Andrula Song <andrula.song@intel.com>
 */

#ifndef __IIR_DF1_GENERIC_H__
#define __IIR_DF1_GENERIC_H__

#include <stdint.h>

static inline int16_t iir_df1_s16(struct iir_state_df1 *iir, int16_t x)
{
	return sat_int16(Q_SHIFT_RND(iir_df1(iir, ((int32_t)x) << 16), 31, 15));
}

static inline int32_t iir_df1_s24(struct iir_state_df1 *iir, int32_t x)
{
	return sat_int24(Q_SHIFT_RND(iir_df1(iir, x << 8), 31, 23));
}

static inline int16_t iir_df1_s32_s16(struct iir_state_df1 *iir, int32_t x)
{
	return sat_int16(Q_SHIFT_RND(iir_df1(iir, x), 31, 15));
}

static inline int32_t iir_df1_s32_s24(struct iir_state_df1 *iir, int32_t x)
{
	return sat_int24(Q_SHIFT_RND(iir_df1(iir, x), 31, 23));
}

#endif /* __IIR_DF1_GENERIC_H__ */

