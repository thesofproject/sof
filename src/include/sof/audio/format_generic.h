/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_FORMAT_GENERIC_H__
#define __SOF_AUDIO_FORMAT_GENERIC_H__

#include <stdint.h>

/* Saturation inline functions */

static inline int32_t sat_generic(int64_t x, int64_t max_val, int64_t min_val);

static inline int32_t sat_int32(int64_t x)
{
	return sat_generic(x, INT32_MAX, INT32_MIN);
}

static inline int32_t sat_int24(int32_t x)
{
	return sat_generic(x, INT24_MAXVALUE, INT24_MINVALUE);
}

static inline int16_t sat_int16(int32_t x)
{
	return (int16_t)sat_generic(x, INT16_MAX, INT16_MIN);
}

static inline int8_t sat_int8(int32_t x)
{
	return (int8_t)sat_generic(x, INT8_MAX, INT8_MIN);
}

static inline int32_t sat_generic(int64_t x, int64_t max_val, int64_t min_val)
{
	int64_t mask_overflow = (max_val - x) >> 63;
	int64_t mask_underflow = (x - min_val) >> 63;

	/* Apply masks to selectively replace x with min or max values, or keep x as is. */
	x = (x & ~mask_overflow) | (max_val & mask_overflow);
	x = (x & ~mask_underflow) | (min_val & mask_underflow);
	return (int32_t)x;
}

#endif /* __SOF_AUDIO_FORMAT_GENERIC_H__ */
