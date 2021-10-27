/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __IIR_DF2T_GENERIC_H__
#define __IIR_DF2T_GENERIC_H__

#include <stdint.h>

static inline int16_t iir_df2t_s16(struct iir_state_df2t *iir, int16_t x)
{
	return sat_int16(Q_SHIFT_RND(iir_df2t(iir, ((int32_t)x) << 16), 31, 15));
}

static inline int32_t iir_df2t_s24(struct iir_state_df2t *iir, int32_t x)
{
	return sat_int24(Q_SHIFT_RND(iir_df2t(iir, x << 8), 31, 23));
}

static inline int16_t iir_df2t_s32_s16(struct iir_state_df2t *iir, int32_t x)
{
	return sat_int16(Q_SHIFT_RND(iir_df2t(iir, x), 31, 15));
}

static inline int32_t iir_df2t_s32_s24(struct iir_state_df2t *iir, int32_t x)
{
	return sat_int24(Q_SHIFT_RND(iir_df2t(iir, x), 31, 23));
}

#endif /* __IIR_DF2T_GENERIC_H__ */

