/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef AUDIO_FORMAT_H
#define AUDIO_FORMAT_H

/* Maximum and minimum values for integer types */
#define INT16_MAXVALUE  32767
#define INT16_MINVALUE -32768
#define INT24_MAXVALUE  8388607
#define INT24_MINVALUE -8388608
#define INT32_MAXVALUE  2147483647
#define INT32_MINVALUE -2147483648

/* Collection of common fractional numbers */
#define ONE_Q2_30 1073741824 /* Q2.30 1.0 */
#define ONE_Q1_31 2147483647 /* Q1.31 ~1.0 */


/* A more clever macro for Q-shifts */
#define Q_SHIFT(x, src_q, dst_q) ((x)>>((src_q)-(dst_q)))
#define Q_SHIFT_RND(x, src_q, dst_q) ((((x) >> ((src_q)-(dst_q) -1)) +1) >> 1)

/* Alternative version since compiler does not allow (x >> -1) */
#define Q_SHIFT_LEFT(x, src_q, dst_q) ((x)<<((dst_q)-(src_q)))

/* Fractional multiplication with shift
 * Note that the parameters px and py must be cast to (int64_t) if other type.
 */
#define Q_MULTS_32X32(px, py, qx, qy, qp) ((px) * (py) >> (((qx)+(qy)-(qp))))

/* Fractional multiplication with shift and round
 * Note that the parameters px and py must be cast to (int64_t) if other type.
 */
#define Q_MULTSR_32X32(px, py, qx, qy, qp) (((px) * (py) >> (((qx)+(qy)-(qp)-1) +1) >> 1))

/* Saturation */
#define SATP_INT32(x) (((x) > INT32_MAXVALUE) ? INT32_MAXVALUE : (x))
#define SATM_INT32(x) (((x) < INT32_MINVALUE) ? INT32_MINVALUE : (x))

static inline int64_t q_mults_32x32(int32_t x, int32_t y,
	const int qx, const int qy, const int qp)
{
	return ((int64_t)x * y) >> (qx+qy-qp);
}

static inline int64_t q_multsr_32x32(int32_t x, int32_t y,
	const int qx, const int qy, const int qp)
{
	return ((((int64_t)x * y) >> (qx+qy-qp-1)) + 1) >> 1;
}

static inline int32_t q_mults_16x16(int16_t x, int16_t y,
	const int qx, const int qy, const int qp)
{
	return ((int32_t)x * y) >> (qx+qy-qp);
}

static inline int16_t q_multsr_16x16(int16_t x, int16_t y,
	const int qx, const int qy, const int qp)
{
	return ((((int32_t)x * y) >> (qx+qy-qp-1)) + 1) >> 1;
}

/* Saturation inline functions */

static inline int32_t sat_int32(int64_t x)
{
#if 1
	/* TODO: Is this faster */
	if (x > INT32_MAXVALUE)
		return INT32_MAXVALUE;
	else if (x < INT32_MINVALUE)
		return INT32_MINVALUE;
	else
		return (int32_t)x;
#else
	/* Or this */
	int64_t y;
	y = SATP_INT32(x);
	return (int32_t)SATM_INT32(y);

#endif
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
	if (x > INT16_MAXVALUE)
		return INT16_MAXVALUE;
	else if (x < INT16_MINVALUE)
		return INT16_MINVALUE;
	else
		return (int16_t)x;
}

#endif
