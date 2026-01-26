// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020-2026 Intel Corporation.
//
// Author: Amery Song <chao.song@intel.com>
//	   Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/exp_fcn.h>
#include <sof/math/log.h>
#include <sof/math/trig.h>
#include <sof/common.h>
#include <stdint.h>

#ifndef __SOF_ICOMPLEX32_H__
#define __SOF_ICOMPLEX32_H__

/**
 * struct icomplex32 - Storage for a normal complex number.
 * @param real The real part in Q1.31 fractional format.
 * @param imag The imaginary part in Q1.31 fractional format.
 */
struct icomplex32 {
	int32_t real;
	int32_t imag;
};

/*
 * These helpers are optimized for FFT calculation only.
 * e.g. _add/sub() assume the output won't be saturate so no check needed,
 * and _mul() assumes Q1.31 * Q1.31 so the output will be shifted to be Q1.31.
 */

static inline void icomplex32_add(const struct icomplex32 *in1, const struct icomplex32 *in2,
				  struct icomplex32 *out)
{
	out->real = in1->real + in2->real;
	out->imag = in1->imag + in2->imag;
}

static inline void icomplex32_adds(const struct icomplex32 *in1, const struct icomplex32 *in2,
				   struct icomplex32 *out)
{
	out->real = sat_int32((int64_t)in1->real + in2->real);
	out->imag = sat_int32((int64_t)in1->imag + in2->imag);
}

static inline void icomplex32_sub(const struct icomplex32 *in1, const struct icomplex32 *in2,
				  struct icomplex32 *out)
{
	out->real = in1->real - in2->real;
	out->imag = in1->imag - in2->imag;
}

static inline void icomplex32_mul(const struct icomplex32 *in1, const struct icomplex32 *in2,
				  struct icomplex32 *out)
{
	out->real = ((int64_t)in1->real * in2->real - (int64_t)in1->imag * in2->imag) >> 31;
	out->imag = ((int64_t)in1->real * in2->imag + (int64_t)in1->imag * in2->real) >> 31;
}

/* complex conjugate */
static inline void icomplex32_conj(struct icomplex32 *comp)
{
	comp->imag = SATP_INT32((int64_t)-1 * comp->imag);
}

/* shift a complex n bits, n > 0: left shift, n < 0: right shift */
static inline void icomplex32_shift(const struct icomplex32 *input, int32_t n,
				    struct icomplex32 *output)
{
	if (n > 0) {
		/* need saturation handling */
		output->real = SATP_INT32(SATM_INT32((int64_t)input->real << n));
		output->imag = SATP_INT32(SATM_INT32((int64_t)input->imag << n));
	} else {
		output->real = input->real >> -n;
		output->imag = input->imag >> -n;
	}
}

#endif /* __SOF_ICOMPLEX32_H__ */
