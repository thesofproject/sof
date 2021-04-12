/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_FORMAT_H__
#define __SOF_AUDIO_FORMAT_H__

#include <ipc/stream.h>
#include <stdint.h>

/* Maximum and minimum values for 24 bit */
#define INT24_MAXVALUE  8388607
#define INT24_MINVALUE -8388608

/* Collection of common fractional numbers */
#define ONE_Q2_30 1073741824 /* Q2.30 1.0 */
#define ONE_Q1_31 2147483647 /* Q1.31 ~1.0 */
#define MINUS_3DB_Q1_31  1520301996 /* 10^(-3/20) */
#define MINUS_6DB_Q1_31  1076291389 /* 10^(-6/20) */
#define MINUS_10DB_Q1_31  679093957  /* 10^(-10/20) */
#define MINUS_20DB_Q1_31  214748365  /* 10^(-20/20) */
#define MINUS_30DB_Q1_31   67909396  /* 10^(-30/20) */
#define MINUS_40DB_Q1_31   21474836  /* 10^(-40/20) */
#define MINUS_50DB_Q1_31    6790940  /* 10^(-50/20) */
#define MINUS_60DB_Q1_31    2147484  /* 10^(-60/20) */
#define MINUS_70DB_Q1_31     679094  /* 10^(-70/20) */
#define MINUS_80DB_Q1_31     214748  /* 10^(-80/20) */
#define MINUS_90DB_Q1_31      67909  /* 10^(-90/20) */

/* Compute the number of shifts
 * This will result in a compiler overflow error if shift bits are out of
 * range as INT64_MAX/MIN is greater than 32 bit Q shift parameter
 */
#define Q_SHIFT_BITS_64(qx, qy, qz) \
	((qx + qy - qz) <= 63 ? (((qx + qy - qz) >= 0) ? \
	 (qx + qy - qz) : INT64_MIN) : INT64_MAX)

#define Q_SHIFT_BITS_32(qx, qy, qz) \
	((qx + qy - qz) <= 31 ? (((qx + qy - qz) >= 0) ? \
	 (qx + qy - qz) : INT32_MIN) : INT32_MAX)

/* Convert a float number to fractional Qnx.ny format. Note that there is no
 * check for nx+ny number of bits to fit the word length of int. The parameter
 * qy must be 31 or less.
 */
#define Q_CONVERT_FLOAT(f, qy) \
	((int32_t)(((const double)f) * ((int64_t)1 << (const int)qy) + 0.5))

/* Convert fractional Qnx.ny number x to float */
#define Q_CONVERT_QTOF(x, ny) ((float)(x) / ((int64_t)1 << (ny)))

/* A more clever macro for Q-shifts */
#define Q_SHIFT(x, src_q, dst_q) ((x) >> ((src_q) - (dst_q)))
#define Q_SHIFT_RND(x, src_q, dst_q) \
	((((x) >> ((src_q) - (dst_q) - 1)) + 1) >> 1)

/* Alternative version since compiler does not allow (x >> -1) */
#define Q_SHIFT_LEFT(x, src_q, dst_q) ((x) << ((dst_q) - (src_q)))

/* Fractional multiplication with shift
 * Note that the parameters px and py must be cast to (int64_t) if other type.
 */
#define Q_MULTS_32X32(px, py, qx, qy, qp) \
	((px) * (py) >> (((qx) + (qy) - (qp))))

/* Fractional multiplication with shift and round
 * Note that the parameters px and py must be cast to (int64_t) if other type.
 */
#define Q_MULTSR_32X32(px, py, qx, qy, qp) \
	((((px) * (py) >> ((qx) + (qy) - (qp) - 1)) + 1) >> 1)

/* Saturation */
#define SATP_INT32(x) (((x) > INT32_MAX) ? INT32_MAX : (x))
#define SATM_INT32(x) (((x) < INT32_MIN) ? INT32_MIN : (x))

static inline int64_t q_mults_32x32(int32_t x, int32_t y, const int shift_bits)
{
	return ((int64_t)x * y) >> shift_bits;
}

static inline int64_t q_multsr_32x32(int32_t x, int32_t y, const int shift_bits)
{
	return ((((int64_t)x * y) >> (shift_bits - 1)) + 1) >> 1;
}

static inline int32_t q_mults_16x16(int16_t x, int32_t y, const int shift_bits)
{
	return ((int32_t)x * y) >> shift_bits;
}

static inline int16_t q_multsr_16x16(int16_t x, int32_t y, const int shift_bits)
{
	return ((((int32_t)x * y) >> (shift_bits - 1)) + 1) >> 1;
}

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

/* Fractional multiplication with shift and saturation */
static inline int32_t q_multsr_sat_32x32(int32_t x, int32_t y,
					 const int shift_bits)
{
	return sat_int32(((((int64_t)x * y) >> (shift_bits - 1)) + 1) >> 1);
}

static inline int32_t q_multsr_sat_32x32_24(int32_t x, int32_t y,
					    const int shift_bits)
{
	return sat_int24(((((int64_t)x * y) >> (shift_bits - 1)) + 1) >> 1);
}

static inline int32_t q_multsr_sat_32x32_16(int32_t x, int32_t y,
					    const int shift_bits)
{
	return sat_int16(((((int64_t)x * y) >> (shift_bits - 1)) + 1) >> 1);
}

static inline int16_t q_multsr_sat_16x16(int16_t x, int32_t y,
					 const int shift_bits)
{
	return sat_int16(((((int32_t)x * y) >> (shift_bits - 1)) + 1) >> 1);
}

static inline int32_t sign_extend_s24(int32_t x)
{
	return (x << 8) >> 8;
}

static inline uint32_t get_sample_bytes(enum sof_ipc_frame fmt)
{
	return fmt == SOF_IPC_FRAME_S16_LE ? 2 : 4;
}

static inline uint32_t get_frame_bytes(enum sof_ipc_frame fmt,
				       uint32_t channels)
{
	return get_sample_bytes(fmt) * channels;
}

#endif /* __SOF_AUDIO_FORMAT_H__ */
