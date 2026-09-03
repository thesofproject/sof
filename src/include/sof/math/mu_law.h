/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 */

#ifndef __SOF_MATH_MU_LAW_H__
#define __SOF_MATH_MU_LAW_H__

#include <stdint.h>

/**
 * sofm_mu_law_encode() - Encode sample with mu-law coding
 * @param sample: A s16 sample value
 *
 * The mu-law codec is defined in ITU-T G.711 standard and has been used
 * in telecommunications in USA and Japan. The mu-law coding compresses
 * 14 bit samples to 8 bit data stream. In SOF the high 14 bits of s16
 * format are used for compatibility with normal audios.
 *
 * @return: Compressed 8 bit code value
 */
uint8_t sofm_mu_law_encode(int16_t sample);

/**
 * sofm_mu_law_decode() - Decode mu-law encoded code word
 * @param byte: Encoded code word
 *
 * The mu-law decoder expands a 8 bit code word into a 14 bit sample value.
 * In the SOF the high 14 bits are aligned to the most significant bits
 * to be compatible with normal s16 Q1.15 samples.
 *
 * @return: Sample value in s16 format
 */
int16_t sofm_mu_law_decode(int8_t byte);

#endif /* __SOF_MATH_MU_LAW_H__ */
