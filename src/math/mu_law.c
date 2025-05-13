// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/mu_law.h>
#include <stdint.h>

#define SOFM_MULAW_BIAS			33
#define SOFM_MULAW_MAX			8191
#define SOFM_MULAW_TOGGLE_BITS		0x7f
#define SOFM_MULAW_SIGN_BIT		0x80
#define SOFM_MULAW_MANTISSA_MASK	0x0f
#define SOFM_MULAW_MANTISSA_BITS	4
#define SOFM_MULAW_SHIFT_MASK		0x07
#define SOFM_MULAW_DEC_ONES_MASK	0x21  /* 0b100001 for "1abcd1", see below */

/*
 * mu-law encode table (sign bit is b12)
 *
 * Input values 12:0		Output values 6:0
 *
 * 0 0 0 0 0 0 0 1 a b c d x      0 0 0 a b c d
 * 0 0 0 0 0 0 1 a b c d x x      0 0 1 a b c d
 * 0 0 0 0 0 1 a b c d x x x      0 1 0 a b c d
 * 0 0 0 0 1 a b c d x x x x      0 1 1 a b c d
 * 0 0 0 1 a b c d x x x x x      1 0 0 a b c d
 * 0 0 1 a b c d x x x x x x      1 0 1 a b c d
 * 0 1 a b c d x x x x x x x      1 1 0 a b c d
 * 1 a b c d x x x x x x x x      1 1 1 a b c d
 *
 * mu-law decode table (sign bit is b7)
 *
 * Input values 6:0	Output values 12:0
 *
 * 0 0 0 a b c d	0 0 0 0 0 0 0 1 a b c d 1
 * 0 0 1 a b c d	0 0 0 0 0 0 1 a b c d 1 0
 * 0 1 0 a b c d	0 0 0 0 0 1 a b c d 1 0 0
 * 0 1 1 a b c d	0 0 0 0 1 a b c d 1 0 0 0
 * 1 0 0 a b c d	0 0 0 1 a b c d 1 0 0 0 0
 * 1 0 1 a b c d	0 0 1 a b c d 1 0 0 0 0 0
 * 1 1 0 a b c d	0 1 a b c d 1 0 0 0 0 0 0
 * 1 1 1 a b c d        1 a b c d 1 0 0 0 0 0 0 0
 */

/* Shift values lookup table for above table for 7
 * highest sample value bits.
 */
static uint8_t mulaw_encode_shifts[128] = {
	0, 1, 2, 2, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7,
};

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

uint8_t sofm_mu_law_encode(int16_t sample)
{
	int sign = SOFM_MULAW_SIGN_BIT;
	int shift = 0;
	int low_bits;
	uint8_t byte;

	/* Convert to 14 bits with shift */
	sample >>= 2;

	/* Negative samples are 1's complement with zero sign bit */
	if (sample < 0) {
		sign = 0;
		sample = -sample - 1;
	}

	sample += SOFM_MULAW_BIAS;
	if (sample > SOFM_MULAW_MAX)
		sample = SOFM_MULAW_MAX;

	shift = mulaw_encode_shifts[sample >> 6];
	low_bits = (sample >> (shift + 1)) & SOFM_MULAW_MANTISSA_MASK;

	byte = (shift << SOFM_MULAW_MANTISSA_BITS) | low_bits | sign;
	byte ^= SOFM_MULAW_TOGGLE_BITS;
	return byte;
}

/**
 * sofm_mu_law_decode() - Decode mu-law encoded code word
 * @param byte: Encoded code word
 *
 * The mu-law decoder expands a 8 bit code word into a 14 bit sample value.
 * In the SOF the high 14 bits are aligned to the most significant bits
 * to be compatible with normal s16 Q1.15 samples.
 *
 * @return: Sampple value in s16 format
 */
int16_t sofm_mu_law_decode(int8_t byte)
{
	int low_bits;
	int shift;
	int sign;
	int16_t value;

	sign = byte & SOFM_MULAW_SIGN_BIT;
	byte ^= SOFM_MULAW_TOGGLE_BITS;
	low_bits = byte & SOFM_MULAW_MANTISSA_MASK;
	shift = (byte >> SOFM_MULAW_MANTISSA_BITS) & SOFM_MULAW_SHIFT_MASK;
	value = (low_bits << (shift + 1)) | (SOFM_MULAW_DEC_ONES_MASK << shift);
	value -= SOFM_MULAW_BIAS;
	if (!sign)
		value = -value;

	/* Shift 14 bit Q1.13 to 16 bit Q1.15 */
	return value << 2;
}
