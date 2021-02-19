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

int32_t drc_lin2db_fixed(int32_t linear); /* Input:Q6.26 Output:Q11.21 */
int32_t drc_log_fixed(int32_t x); /* Input:Q6.26 Output:Q6.26 */
int32_t drc_sin_fixed(int32_t x); /* Input:Q2.30 Output:Q1.31 */
int32_t drc_asin_fixed(int32_t x); /* Input:Q2.30 Output:Q2.30 */
int32_t drc_pow_fixed(int32_t x, int32_t y); /* Input:Q6.26, Q2.30 Output:Q12.20 */
int32_t drc_inv_fixed(int32_t x, int32_t precision_x, int32_t precision_y);

#endif //  __SOF_AUDIO_DRC_DRC_MATH_H__
