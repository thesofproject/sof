/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 */

#ifndef __SOF_MATH_A_LAW_H__
#define __SOF_MATH_A_LAW_H__

#include <stdint.h>

/**
 * sofm_a_law_encode() - Encode sample with A-law coding
 * @param sample: A s16 sample value
 *
 * The A-law codec is defined in ITU-T G.711 standard and has been used
 * in telecommunications in e.g. Europe. The A-law coding compresses 13 bit
 * samples to 8 bit data stream. In SOF the high 13 bits of s16 format are
 * used for compatibility with normal audios.
 *
 * @return: Compressed 8 bit code value
 */
uint8_t sofm_a_law_encode(int16_t sample);

/**
 * sofm_a_law_decode() - Decode A-law encoded code word
 * @param byte: Encoded code word
 *
 * The A-law decoder expands a 8 bit code word into a 13 bit sample value.
 * In the SOF the high 13 bits are aligned to the most significant bits
 * to be compatible with normal s16 Q1.15 samples.
 *
 * @return: Sample value in s16 format
 */
int16_t sofm_a_law_decode(int8_t byte);

#endif /* __SOF_MATH_A_LAW_H__ */
