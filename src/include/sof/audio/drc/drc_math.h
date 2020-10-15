/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_DRC_DRC_MATH_H__
#define __SOF_AUDIO_DRC_DRC_MATH_H__

#include <stddef.h>
#include <stdint.h>

#define DRC_NEG_TWO_DB 0.7943282347242815f /* -2dB = 10^(-2/20) */

float decibels_to_linear(float decibels);
float linear_to_decibels(float linear);
float warp_logf(float x);
float warp_sinf(float x);
float warp_asinf(float x);
float warp_powf(float x, float y);
float knee_expf(float input);
int isbadf(float x);

#endif //  __SOF_AUDIO_DRC_DRC_MATH_H__
