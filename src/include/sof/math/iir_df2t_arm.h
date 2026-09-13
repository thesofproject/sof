/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Sound Open Firmware. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __IIR_DF2T_ARM_H__
#define __IIR_DF2T_ARM_H__

#include <stdint.h>
#include <sof/audio/format.h>
#include <sof/audio/arm_simd.h>

static inline int16_t iir_df2t_s16(struct iir_state_df2t *iir, int16_t x)
{
	return arm_sat_s16(Q_SHIFT_RND(iir_df2t(iir, ((int32_t)x) << 16), 31, 15));
}

static inline int32_t iir_df2t_s24(struct iir_state_df2t *iir, int32_t x)
{
	return arm_sat_s24(Q_SHIFT_RND(iir_df2t(iir, x << 8), 31, 23));
}

static inline int16_t iir_df2t_s32_s16(struct iir_state_df2t *iir, int32_t x)
{
	return arm_sat_s16(Q_SHIFT_RND(iir_df2t(iir, x), 31, 15));
}

static inline int32_t iir_df2t_s32_s24(struct iir_state_df2t *iir, int32_t x)
{
	return arm_sat_s24(Q_SHIFT_RND(iir_df2t(iir, x), 31, 23));
}

#endif /* __IIR_DF2T_ARM_H__ */
