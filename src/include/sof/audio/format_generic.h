/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 */

#ifndef __SOF_AUDIO_FORMAT_GENERIC_H__
#define __SOF_AUDIO_FORMAT_GENERIC_H__

#include <stdint.h>

/* Saturation inline functions */
/**
 * @brief Saturate an int64_t value to fit within the range of int32_t.
 *
 * This function ensures that the input value 'x' fits within the range of
 * int32_t. If 'x' exceeds this range, it will be bound to INT32_MAX or INT32_MIN
 * accordingly.
 *
 * @param x The input value to saturate.
 * @return int32_t The saturated int32_t value.
 */
static inline int32_t sat_int32(int64_t x)
{
	int64_t mask_overflow = (INT32_MAX - x) >> 63;
	int64_t mask_underflow = (x - INT32_MIN) >> 63;

	x = (x & ~mask_overflow) | (INT32_MAX & mask_overflow);
	x = (x & ~mask_underflow) | (INT32_MIN & mask_underflow);
	return (int32_t)x;

}

/**
 * @brief Saturate an int32_t value to fit within the range of a 24-bit integer.
 *
 * This function ensures that the input value 'x' fits within the range of a
 * 24-bit integer. If 'x' exceeds this range, it will be bound to INT24_MAXVALUE
 * or INT24_MINVALUE accordingly.
 *
 * @param x The input value to saturate.
 * @return int32_t The saturated int32_t value.
 */
static inline int32_t sat_int24(int32_t x)
{
	int32_t mask_overflow = (INT24_MAXVALUE - x) >> 31;
	int32_t mask_underflow = (x - INT24_MINVALUE) >> 31;

	x = (x & ~mask_overflow) | (INT24_MAXVALUE & mask_overflow);
	x = (x & ~mask_underflow) | (INT24_MINVALUE & mask_underflow);
	return x;
}

/**
 * @brief Saturate an int32_t value to fit within the range of a 16-bit integer.
 *
 * This function ensures that the input value 'x' fits within the range of a
 * 16-bit integer. If 'x' exceeds this range, it will be bound to INT16_MAX or INT16_MIN
 * accordingly.
 *
 * @param x The input value to saturate.
 * @return int16_t The saturated int16_t value.
 */
static inline int16_t sat_int16(int32_t x)
{
	int32_t mask_overflow = (INT16_MAX - x) >> 31;
	int32_t mask_underflow = (x - INT16_MIN) >> 31;

	x = (x & ~mask_overflow) | (INT16_MAX & mask_overflow);
	x = (x & ~mask_underflow) | (INT16_MIN & mask_underflow);
	return (int16_t)x;
}

/**
 * @brief Saturate an int32_t value to fit within the range of an 8-bit integer.
 *
 * This function ensures that the input value 'x' fits within the range of an
 * 8-bit integer. If 'x' exceeds this range, it will be bound to INT8_MAX or INT8_MIN
 * accordingly.
 *
 * @param x The input value to saturate.
 * @return int8_t The saturated int8_t value.
 */
static inline int8_t sat_int8(int32_t x)
{
	int32_t mask_overflow = (INT8_MAX - x) >> 31;
	int32_t mask_underflow = (x - INT8_MIN) >> 31;

	x = (x & ~mask_overflow) | (INT8_MAX & mask_overflow);
	x = (x & ~mask_underflow) | (INT8_MIN & mask_underflow);
	return (int8_t)x;
}

#endif /* __SOF_AUDIO_FORMAT_GENERIC_H__ */
