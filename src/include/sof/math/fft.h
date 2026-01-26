/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Amery Song <chao.song@intel.com>
 *	   Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_FFT_H__
#define __SOF_FFT_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/format.h>
#include <sof/math/icomplex32.h>
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

#define FFT_SIZE_MIN		1
#define FFT_SIZE_MAX		1024
#define FFT_MULTI_COUNT_MAX	3

struct fft_plan {
	uint32_t size;	/* fft size */
	uint32_t len;	/* fft length in exponent of 2 */
	uint16_t *bit_reverse_idx;	/* pointer to bit reverse index array */
	struct icomplex32 *inb32;	/* pointer to input integer complex buffer */
	struct icomplex32 *outb32;	/* pointer to output integer complex buffer */
	struct icomplex16 *inb16;	/* pointer to input integer complex buffer */
	struct icomplex16 *outb16;	/* pointer to output integer complex buffer */
};

struct fft_multi_plan {
	struct fft_plan *fft_plan[FFT_MULTI_COUNT_MAX];
	struct icomplex32 *tmp_i32[FFT_MULTI_COUNT_MAX]; /* pointer to input buffer */
	struct icomplex32 *tmp_o32[FFT_MULTI_COUNT_MAX]; /* pointer to output buffer */
	struct icomplex32 *inb32;	/* pointer to input integer complex buffer */
	struct icomplex32 *outb32;	/* pointer to output integer complex buffer */
	uint16_t *bit_reverse_idx;
	uint32_t total_size;
	uint32_t fft_size;
	int num_ffts;
};

/* interfaces of the library */
struct fft_plan *mod_fft_plan_new(struct processing_module *mod, void *inb,
				  void *outb, uint32_t size, int bits);
void fft_execute_16(struct fft_plan *plan, bool ifft);
void fft_execute_32(struct fft_plan *plan, bool ifft);
void mod_fft_plan_free(struct processing_module *mod, struct fft_plan *plan);

/**
 * mod_fft_multi_plan_new() - Prepare FFT for 2^N size and some other FFT sizes
 * @param mod: Pointer to module
 * @param inb: Buffer to use for complex input data
 * @param outb: Buffer to use for complex output data
 * @param size: Size of FFT as number of bins
 * @param bits: World length of FFT. Currently only 32 is supported.
 * @return Pointer to allocated FFT plan
 *
 * This function does the preparations to calculate FFT. If the size is power of two
 * the operation is similar to mod_fft_plan_new(). Some other FFT sizes like 1536 is
 * supported by allocated multiple FFT plans and by wrapping all needed for similar
 * usage as power of two size FFT.
 */

struct fft_multi_plan *mod_fft_multi_plan_new(struct processing_module *mod, void *inb,
					      void *outb, uint32_t size, int bits);
/**
 * fft_multi_execute_32() - Calculate Fast Fourier Transform (FFT) for 2^size and other
 * @param plan: Pointer to FFT plan created with mod_fft_multi_plan_new()
 * @param ifft: Value 0 calculates FFT, value 1 calculates IFFT
 *
 * This function calculates the FFT with the buffers defined with mod_fft_multi_plan_new().
 */
void fft_multi_execute_32(struct fft_multi_plan *plan, bool ifft);

/**
 * mod_fft_multi_plan_free() - Free the FFT plan
 * @param mod: Pointer to module
 * @param plan: Pointe to FFT plan
 *
 * This function frees the allocations done internally by the function mod_fft_multi_plan_new().
 * The input and output buffers need to be freed separately.
 */
void mod_fft_multi_plan_free(struct processing_module *mod, struct fft_multi_plan *plan);

/**
 * dft3_32() - Discrete Fourier Transform (DFT) for size 3.
 * @param input: Pointer to complex values input array, Q1.31
 * @param output: Pointer to complex values output array, Q3.29
 *
 * This function is useful for calculating some non power of two FFTs. E.g. the
 * FFT for size 1536 is done with three 512 size FFTs and one 3 size DFT.
 */
void dft3_32(struct icomplex32 *input, struct icomplex32 *output);

#endif /* __SOF_FFT_H__ */
