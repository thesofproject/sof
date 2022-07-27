/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* Window functions related functions */

#ifndef __SOF_MATH_WINDOW_H__
#define __SOF_MATH_WINDOW_H__

#include <sof/audio/format.h>
#include <stdint.h>

#define WIN_BLACKMAN_A0 Q_CONVERT_FLOAT(7938.0 / 18608.0, 15) /* For "exact" blackman */

/**
 * \brief Return rectangular window, simply values of one
 * \param[in,out]  win  Output vector with coefficients
 * \param[in]  length  Length of coefficients vector
 */
void win_rectangular_16b(int16_t win[], int length);

/**
 * \brief Calculate Blackman window function, reference
 * https://en.wikipedia.org/wiki/Window_function#Blackman_window
 *
 * \param[in,out]  win     Output vector with coefficients
 * \param[in]      length  Length of coefficients vector
 * \param[in]      a0      Parameter for window shape, use e.g. 0.42 as Q1.15
 */
void win_blackman_16b(int16_t win[], int length, int16_t a0);

/**
 * \brief Calculate Hamming window function, reference
 * https://en.wikipedia.org/wiki/Window_function#Hann_and_Hamming_windows
 *
 * \param[in,out]  win     Output vector with coefficients
 * \param[in]      length  Length of coefficients vector
 */
void win_hamming_16b(int16_t win[], int length);

/**
 * \brief Calculate Povey window function. It's a window function
 * from Pytorch.
 *
 * \param[in,out]  win     Output vector with coefficients
 * \param[in]      length  Length of coefficients vector
 */
void win_povey_16b(int16_t win[], int length);

#endif /* __SOF_MATH_WINDOW_H__ */
