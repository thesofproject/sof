// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/**
 * fft_plan_common_new() - Common FFT prepare function
 * @param mod: Pointer to module
 * @param inb: Buffer to use for complex input data
 * @param outb: Buffer to use for complex output data
 * @param size: Size of FFT as number of bins
 * @param bits: World length of FFT. Currently only 32 is supported.
 * @return Pointer to FFT plan
 */
struct fft_plan *fft_plan_common_new(struct processing_module *mod, void *inb,
				     void *outb, uint32_t size, int bits);

/**
 * fft_plan_init_bit_reverse - Configures a bit reversal lookup vector
 * @param bit_reverse_idx: Pointer to array to store bit reverse lookup
 * @param size: Size of FFT
 * @param len: Power of two value equals FFT size
 */
void fft_plan_init_bit_reverse(uint16_t *bit_reverse_idx, int size, int len);
