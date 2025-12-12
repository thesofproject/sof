/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2025 Intel Corporation.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* Window functions related functions */

#ifndef __SOF_MATH_WINDOW_H__
#define __SOF_MATH_WINDOW_H__

#include <sof/audio/format.h>
#include <stdint.h>

#define WIN_BLACKMAN_A0_Q15 Q_CONVERT_FLOAT(7938.0 / 18608.0, 15) /* For "exact" blackman 16bit */
#define WIN_BLACKMAN_A0_Q31 Q_CONVERT_FLOAT(7938.0 / 18608.0, 31) /* For "exact" blackman 32bit */

/**
 * Returns a rectangular window with 16 bits, simply a values of ones vector
 * @param win		Pointer to output vector with Q1.15 coefficients
 * @param length	Length of coefficients vector
 */
void win_rectangular_16b(int16_t win[], int length);

/**
 * Returns a rectangular window with 32 bits, simply a values of ones vector
 * @param win		Pointer to output vector with Q1.31 coefficients
 * @param length	Length of coefficients vector
 */
void win_rectangular_32b(int32_t win[], int length);

/**
 * Calculates a Blackman window function with 16 bits, reference
 * https://en.wikipedia.org/wiki/Window_function#Blackman_window
 *
 * @param win		Pointer to output vector with Q1.15 coefficients
 * @param length	Length of coefficients vector
 * @param a0		Parameter for window shape, use e.g. 0.42 as Q1.15
 */
void win_blackman_16b(int16_t win[], int length, int16_t a0);

/**
 * Calculates a Blackman window function with 32 bits, reference
 * https://en.wikipedia.org/wiki/Window_function#Blackman_window
 *
 * @param win		Pointer to output vector with Q1.31 coefficients
 * @param length	Length of coefficients vector
 * @param a0		Parameter for window shape, use e.g. 0.42 as Q1.31
 */
void win_blackman_32b(int32_t win[], int length, int32_t a0);

/**
 * Calculates a Hann window function with 16 bits, reference
 * https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows
 *
 * @param win		Pointer to output vector with Q1.15 coefficients
 * @param length	Length of coefficients vector
 */
void win_hann_16b(int16_t win[], int length);

/**
 * Calculates a Hann window function with 32 bits, reference
 * https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows
 *
 * @param win		Pointer to output vector with Q1.31 coefficients
 * @param length	Length of coefficients vector
 */
void win_hann_32b(int32_t win[], int length);

/**
 * Calculates a Hamming window function with 16 bits, reference
 * https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows
 *
 * @param win		Pointer to output vector with Q1.15 coefficients
 * @param length	Length of coefficients vector
 */
void win_hamming_16b(int16_t win[], int length);

/**
 * Calculates a Hamming window function with 32 bits, reference
 * https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows
 *
 * @param win		Pointer to output vector with Q1.31 coefficients
 * @param length	Length of coefficients vector
 */
void win_hamming_32b(int32_t win[], int length);

/**
 * Calculates a Povey window function with 16 bits. It's a window function
 * used in Pytorch and Kaldi.
 *
 * @param win		Output vector with coefficients
 * @param length	Length of coefficients vector
 */
void win_povey_16b(int16_t win[], int length);

#endif /* __SOF_MATH_WINDOW_H__ */
