/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Amery Song <chao.song@intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_FFT_H__
#define __SOF_FFT_H__

#include <sof/common.h>

#define FFT_SIZE_MAX	1024

struct icomplex32 {
	int32_t real;
	int32_t imag;
};

struct fft_plan {
	uint32_t size;	/* fft size */
	uint32_t len;	/* fft length in exponent of 2 */
	uint32_t *bit_reverse_idx;	/* pointer to bit reverse index array */
	struct icomplex32 *inb;	/* pointer to input integer complex buffer */
	struct icomplex32 *outb;/* pointer to output integer complex buffer */
};

/*
 * These helpers are optimized for FFT calculation only.
 * e.g. _add/sub() assume the output won't be saturate so no check needed,
 * and _mul() assumes Q1.31 * Q1.31 so the output will be shifted to be Q1.31.
 */

static inline void icomplex32_add(struct icomplex32 *in1, struct icomplex32 *in2,
				  struct icomplex32 *out)
{
	out->real = in1->real + in2->real;
	out->imag = in1->imag + in2->imag;
}

static inline void icomplex32_sub(struct icomplex32 *in1, struct icomplex32 *in2,
				  struct icomplex32 *out)
{
	out->real = in1->real - in2->real;
	out->imag = in1->imag - in2->imag;
}

static inline void icomplex32_mul(struct icomplex32 *in1, struct icomplex32 *in2,
				  struct icomplex32 *out)
{
	out->real = ((int64_t)in1->real * in2->real - (int64_t)in1->imag * in2->imag) >> 31;
	out->imag = ((int64_t)in1->real * in2->imag + (int64_t)in1->imag * in2->real) >> 31;
}

/* complex conjugate */
static inline void icomplex_conj(struct icomplex32 *comp)
{
	comp->imag = SATP_INT32((int64_t)-1 * comp->imag);
}

/* shift a complex n bits, n > 0: left shift, n < 0: right shift */
static inline void icomplex_shift(struct icomplex32 *input, int32_t n, struct icomplex32 *output)
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

/* interfaces of the library */
struct fft_plan *fft_plan_new(struct icomplex32 *inb, struct icomplex32 *outb, uint32_t size);
void fft_plan_free(struct fft_plan *plan);
void fft_execute(struct fft_plan *plan, bool ifft);

#endif /* __SOF_FFT_H__ */
