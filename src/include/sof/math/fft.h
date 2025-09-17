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

#define FFT_GENERIC
#if defined(__XCC__)

#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
#undef FFT_GENERIC
#define FFT_HIFI3
#endif

#endif

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

/* interfaces of the library */
struct fft_plan *fft_plan_new(void *inb, void *outb, uint32_t size, int bits);
struct processing_module;
struct fft_plan *mod_fft_plan_new(struct processing_module *mod, void *inb,
				  void *outb, uint32_t size, int bits);
void fft_execute_16(struct fft_plan *plan, bool ifft);
void fft_execute_32(struct fft_plan *plan, bool ifft);
void fft_plan_free(struct fft_plan *plan16);
void mod_fft_plan_free(struct processing_module *mod, struct fft_plan *plan16);

#endif /* __SOF_FFT_H__ */
