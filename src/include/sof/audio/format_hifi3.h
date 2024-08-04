/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 */

#ifndef __SOF_AUDIO_FORMAT_HIFI3_H__
#define __SOF_AUDIO_FORMAT_HIFI3_H__

#include <stdint.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>

/* Saturation inline functions */

/**
 * @brief Saturate a 64-bit integer to 32-bit.
 *
 * @param x 64-bit integer.
 * @return 32-bit saturated integer.
 *
 * This function takes a 64-bit integer, performs bitwise shifting,
 * and returns a saturated 32-bit integer.
 */
static inline int32_t sat_int32(int64_t x)
{
	/* Shift left by 32 bits with saturation */
	ae_f64 shifted = AE_SLAI64S(x, 32);

	/* Shift right by 32 bits */
	return (int32_t)AE_MOVINT32_FROMINT64(AE_SRAI64(shifted, 32));
}

/**
 * @brief Saturate and round two 64-bit integers to 32-bit packed into a 32x2 vector.
 *
 * @param x 64-bit integer.
 * @param y 64-bit integer.
 * @return Packed 32-bit saturated integers.
 *
 * This function takes two 64-bit integers, converts them to ae_f64 format,
 * and returns them as a packed ae_int32x2 vector after saturation.
 */
static inline ae_int32x2 vec_sat_int32x2(int64_t x, int64_t y)
{
	ae_f64 d0, d1;

	/* Convert 64-bit integers to ae_f64 format */
	d0 = (ae_f64)x;
	d1 = (ae_f64)y;

	/* Round and saturate both 64-bit values to 32-bit and pack them */
	return (ae_int32x2)AE_ROUND32X2F64SSYM(d0, d1);
}
static inline int32_t sat_int24(int32_t x)
{
	return AE_SRAI32(AE_SLAI32S(x, 8), 8);
}

static inline int16_t sat_int16(int32_t x)
{
	return AE_SAT16X4(x, x);
}

static inline int8_t sat_int8(int32_t x)
{
	if (x > INT8_MAX)
		return INT8_MAX;
	else if (x < INT8_MIN)
		return INT8_MIN;
	else
		return (int8_t)x;
}

#endif /* __SOF_AUDIO_FORMAT_HIFI3_H__ */
