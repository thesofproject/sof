// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef Q_FORMAT_H
#define Q_FORMAT_H

#include <stdint.h>

/* Maximum and minimum values for 24 bit */
#define INT24_MAXVALUE  8388607
#define INT24_MINVALUE -8388608

#define Q_SHIFT_BITS_64(qx, qy, qz) \
	((qx + qy - qz) <= 63 ? (((qx + qy - qz) >= 0) ? \
	 (qx + qy - qz) : INT64_MIN) : INT64_MAX)

#define Q_SHIFT_BITS_32(qx, qy, qz) \
	((qx + qy - qz) <= 31 ? (((qx + qy - qz) >= 0) ? \
	 (qx + qy - qz) : INT32_MIN) : INT32_MAX)

/* Saturation inline functions */

static inline int32_t sat_int32(int64_t x)
{
	if (x > INT32_MAX)
		return INT32_MAX;
	else if (x < INT32_MIN)
		return INT32_MIN;
	else
		return (int32_t)x;
}

static inline int32_t sat_int24(int32_t x)
{
	if (x > INT24_MAXVALUE)
		return INT24_MAXVALUE;
	else if (x < INT24_MINVALUE)
		return INT24_MINVALUE;
	else
		return x;
}

static inline int16_t sat_int16(int32_t x)
{
	if (x > INT16_MAX)
		return INT16_MAX;
	else if (x < INT16_MIN)
		return INT16_MIN;
	else
		return (int16_t)x;
}

static inline int64_t q_mults_32x32(int32_t x, int16_t y, const int shift_bits)
{
	return ((int64_t)x * y) >> shift_bits;
}

static inline int32_t q_mults_16x16(int16_t x, int16_t y, const int shift_bits)
{
	return ((int32_t)x * y) >> shift_bits;
}

static inline int32_t q_mults_sat_32x32(int32_t x, int16_t y, const int shift_bits)
{
	return sat_int32(((int64_t)x * y) >> shift_bits);
}

static inline int16_t q_mults_sat_16x16(int16_t x, int16_t y, const int shift_bits)
{
	return sat_int16(((int32_t)x * y) >> shift_bits);
}

#endif /* Q_FORMAT_H */
