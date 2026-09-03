// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020-2026 Intel Corporation.
//
// Author: Amery Song <chao.song@intel.com>
//	   Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/common.h>
#include <stdint.h>

#ifndef __SOF_ICOMPLEX16_H__
#define __SOF_ICOMPLEX16_H__

/* Note: the add of packed attribute to icmplex16 would significantly increase
 * processing time of fft_execute_16() so it is not done. The optimized versions of
 * FFT for HiFi will need a different packed data structure vs. generic C.
 *
 * TODO: Use with care with other than 16-bit FFT internals. Access with intrinsics
 * will requires packed and aligned data. Currently there is no such usage in SOF.
 */

/**
 * struct icomplex16 - Storage for a normal complex number.
 * @param real The real part in Q1.15 fractional format.
 * @param imag The imaginary part in Q1.15 fractional format.
 */
struct icomplex16 {
	int16_t real;
	int16_t imag;
};

/*
 * Helpers for 16 bit FFT calculation
 */
static inline void icomplex16_add(const struct icomplex16 *in1, const struct icomplex16 *in2,
				  struct icomplex16 *out)
{
	out->real = in1->real + in2->real;
	out->imag = in1->imag + in2->imag;
}

static inline void icomplex16_sub(const struct icomplex16 *in1, const struct icomplex16 *in2,
				  struct icomplex16 *out)
{
	out->real = in1->real - in2->real;
	out->imag = in1->imag - in2->imag;
}

static inline void icomplex16_mul(const struct icomplex16 *in1, const struct icomplex16 *in2,
				  struct icomplex16 *out)
{
	int32_t real = (int32_t)in1->real * in2->real - (int32_t)in1->imag * in2->imag;
	int32_t imag = (int32_t)in1->real * in2->imag + (int32_t)in1->imag * in2->real;

	out->real = Q_SHIFT_RND(real, 30, 15);
	out->imag = Q_SHIFT_RND(imag, 30, 15);
}

/* complex conjugate */
static inline void icomplex16_conj(struct icomplex16 *comp)
{
	comp->imag = sat_int16(-((int32_t)comp->imag));
}

/* shift a complex n bits, n > 0: left shift, n < 0: right shift */
static inline void icomplex16_shift(const struct icomplex16 *input, int16_t n,
				    struct icomplex16 *output)
{
	int n1, n2;

	if (n >= 0) {
		/* need saturation handling */
		output->real = sat_int16((int32_t)input->real << n);
		output->imag = sat_int16((int32_t)input->imag << n);
	} else {
		n1 = -n;
		n2 = 1 << (n1 - 1);
		output->real = sat_int16(((int32_t)input->real + n2) >> n1);
		output->imag = sat_int16(((int32_t)input->imag + n2) >> n1);
	}
}

#endif /* __SOF_ICOMPLEX16_H__ */
