/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Amery Song <chao.song@intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_FFT_H__
#define __SOF_FFT_H__

#include <sof/audio/format.h>
#include <sof/common.h>
#include <stdbool.h>
#include <stdint.h>

#define FFT_SIZE_MAX	1024

struct icomplex32 {
	int32_t real;
	int32_t imag;
};

/* Note: the add of packed attribute to icmplex16 would significantly increase
 * processing time of fft_execute_16() so it is not done. The optimized versions of
 * FFT for HiFi will need a different packed data structure vs. generic C.
 */
struct icomplex16 {
	int16_t real;
	int16_t imag;
};

struct fft_plan {
	uint32_t size;	/* fft size */
	uint32_t len;	/* fft length in exponent of 2 */
	uint16_t *bit_reverse_idx;	/* pointer to bit reverse index array */
	struct icomplex32 *inb32;	/* pointer to input integer complex buffer */
	struct icomplex32 *outb32;	/* pointer to output integer complex buffer */
	struct icomplex16 *inb16;	/* pointer to input integer complex buffer */
	struct icomplex16 *outb16;	/* pointer to output integer complex buffer */
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
static inline void icomplex32_conj(struct icomplex32 *comp)
{
	comp->imag = SATP_INT32((int64_t)-1 * comp->imag);
}

/* shift a complex n bits, n > 0: left shift, n < 0: right shift */
static inline void icomplex32_shift(struct icomplex32 *input, int32_t n, struct icomplex32 *output)
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

/*
 * Helpers for 16 bit FFT calculation
 */

static inline void icomplex16_add(struct icomplex16 *in1, struct icomplex16 *in2,
				  struct icomplex16 *out)
{
	out->real = in1->real + in2->real;
	out->imag = in1->imag + in2->imag;
}

static inline void icomplex16_sub(struct icomplex16 *in1, struct icomplex16 *in2,
				  struct icomplex16 *out)
{
	out->real = in1->real - in2->real;
	out->imag = in1->imag - in2->imag;
}

static inline void icomplex16_mul(struct icomplex16 *in1, struct icomplex16 *in2,
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
static inline void icomplex16_shift(struct icomplex16 *input, int16_t n, struct icomplex16 *output)
{
	int n_rnd = -n - 1;

	if (n >= 0) {
		/* need saturation handling */
		output->real = sat_int16((int32_t)input->real << n);
		output->imag = sat_int16((int32_t)input->imag << n);
	} else {
		output->real = sat_int16((((int32_t)input->real >> n_rnd) + 1) >> 1);
		output->imag = sat_int16((((int32_t)input->imag >> n_rnd) + 1) >> 1);
	}
}

/* interfaces of the library */
struct fft_plan *fft_plan_new(void *inb, void *outb, uint32_t size, int bits);
void fft_execute_16(struct fft_plan *plan, bool ifft);
void fft_execute_32(struct fft_plan *plan, bool ifft);
void fft_plan_free(struct fft_plan *plan16);

#endif /* __SOF_FFT_H__ */
