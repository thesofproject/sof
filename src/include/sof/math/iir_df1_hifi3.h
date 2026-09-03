/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Andrula Song <andrula.song@intel.com>
 */

#ifndef __IIR_DF1_HIFI3_H__
#define __IIR_DF1_HIFI3_H__

#include <xtensa/tie/xt_hifi3.h>
#include <stdint.h>

static inline int16_t iir_df1_s16(struct iir_state_df1 *iir, int16_t x)
{
	ae_f32x2 y = iir_df1(iir, ((int32_t)x) << 16);

	return AE_ROUND16X4F32SSYM(y, y);
}

static inline int32_t iir_df1_s24(struct iir_state_df1 *iir, int32_t x)
{
	ae_f32x2 y = iir_df1(iir, x << 8);

	return AE_SRAI32(AE_SLAI32S(AE_SRAI32R(y, 8), 8), 8);
}

static inline int16_t iir_df1_s32_s16(struct iir_state_df1 *iir, int32_t x)
{
	ae_f32x2 y = iir_df1(iir, x);

	return AE_ROUND16X4F32SSYM(y, y);
}

static inline int32_t iir_df1_s32_s24(struct iir_state_df1 *iir, int32_t x)
{
	ae_f32x2 y = iir_df1(iir, x);

	return AE_SRAI32(AE_SLAI32S(AE_SRAI32R(y, 8), 8), 8);
}

#endif /* __IIR_DF1_HIFI3_H__ */

