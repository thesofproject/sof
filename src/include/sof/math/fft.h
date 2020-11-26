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

struct fft_plan *fft_plan_new(struct icomplex32 *inb, struct icomplex32 *outb, uint32_t size);
void fft_plan_free(struct fft_plan *plan);
void fft_execute(struct fft_plan *plan, bool ifft);

#endif /* __SOF_FFT_H__ */
