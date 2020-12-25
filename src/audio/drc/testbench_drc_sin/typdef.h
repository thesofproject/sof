// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
#pragma once
#include <stdint.h>
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;

#define SINE_C_Q20 341782638 /* 2*SINE_NQUART/pi in Q12.20 */
#define SINE_NQUART 512 /* Must be 2^N */
#define SINE_TABLE_SIZE (SINE_NQUART + 1)

/*define macros*/
#define Q_CONVERT_FLOAT(f, qy) \
	((int32_t)(((const double)f) * ((int64_t)1 << (const int)qy) + 0.5))
#define Q_MULTSR_32X32(px, py, qx, qy, qp) \
	((((px) * (py) >> ((qx) + (qy) - (qp) - 1)) + 1) >> 1)
#define q_mult(a, b, qa, qb, qy) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, qa, qb, qy))
#define Q_SHIFT_RND(x, src_q, dst_q) \
	((((x) >> ((src_q) - (dst_q) - 1)) + 1) >> 1)
/* Alternative version since compiler does not allow (x >> -1) */
#define Q_SHIFT_LEFT(x, src_q, dst_q) ((x) << ((dst_q) - (src_q)))
/* Compute the number of shifts
 * This will result in a compiler overflow error if shift bits are out of
 * range as INT64_MAX/MIN is greater than 32 bit Q shift parameter
 */
#define Q_SHIFT_BITS_64(qx, qy, qz) \
	((qx + qy - qz) <= 63 ? (((qx + qy - qz) >= 0) ? \
	 (qx + qy - qz) : INT64_MIN) : INT64_MAX)

#define TEST_VECTOR 21
static inline int64_t q_mults_32x32(int32_t x, int32_t y, const int shift_bits)
{
	return ((int64_t)x * y) >> shift_bits;
}
/*
 * File trailer for typdef.h
 *
 * [EOF]
 */
