/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __IIR_DF2T_HIFI3_H__
#define __IIR_DF2T_HIFI3_H__

#include <xtensa/tie/xt_hifi3.h>
#include <stdint.h>

static inline int16_t iir_df2t_s16(struct iir_state_df2t *iir, int16_t x)
{
	ae_f32x2 y = iir_df2t(iir, ((int32_t)x) << 16);

	return AE_ROUND16X4F32SSYM(y, y);
}

static inline int32_t iir_df2t_s24(struct iir_state_df2t *iir, int32_t x)
{
	ae_f32x2 y = iir_df2t(iir, x << 8);

	return AE_SRAI32(AE_SLAI32S(AE_SRAI32R(y, 8), 8), 8);
}

static inline int16_t iir_df2t_s32_s16(struct iir_state_df2t *iir, int32_t x)
{
	ae_f32x2 y = iir_df2t(iir, x);

	return AE_ROUND16X4F32SSYM(y, y);
}

static inline int32_t iir_df2t_s32_s24(struct iir_state_df2t *iir, int32_t x)
{
	ae_f32x2 y = iir_df2t(iir, x);

	return AE_SRAI32(AE_SLAI32S(AE_SRAI32R(y, 8), 8), 8);
}

#endif /* __IIR_DF2T_HIFI3_H__ */

