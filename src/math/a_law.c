// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/a_law.h>
#include <stdint.h>

#define SOFM_ALAW_SIGN_BIT		0x80
#define SOFM_ALAW_MAX			4095
#define SOFM_ALAW_TOGGLE_EVEN_BITS	0x55
#define SOFM_ALAW_MANTISSA_MASK		0x0f
#define SOFM_ALAW_MANTISSA_BITS		4
#define SOFM_ALAW_SHIFT_MASK		0x07
#define SOFM_ALAW_DEC_ONES_MASK		0x21  /* 0b100001 for "1abcd1", see below */

/*
 * A-law encode table (sign bit is b12)
 *
 * Input values 11:0		Output values 6:0
 *
 * 0 0 0 0 0 0 0 a b c d x      0 0 0 a b c d
 * 0 0 0 0 0 0 1 a b c d x      0 0 1 a b c d
 * 0 0 0 0 0 1 a b c d x x      0 1 0 a b c d
 * 0 0 0 0 1 a b c d x x x      0 1 1 a b c d
 * 0 0 0 1 a b c d x x x x      1 0 0 a b c d
 * 0 0 1 a b c d x x x x x      1 0 1 a b c d
 * 0 1 a b c d x x x x x x      1 1 0 a b c d
 * 1 a b c d x x x x x x x      1 1 1 a b c d
 *
 *
 * A-law decode table (sign bit is b7)
 *
 * Input values 6:0	Output values 11:0
 *
 * 0 0 0 a b c d	0 0 0 0 0 0 0 a b c d 1
 * 0 0 1 a b c d	0 0 0 0 0 0 1 a b c d 1
 * 0 1 0 a b c d	0 0 0 0 0 1 a b c d 1 0
 * 0 1 1 a b c d	0 0 0 0 1 a b c d 1 0 0
 * 1 0 0 a b c d	0 0 0 1 a b c d 1 0 0 0
 * 1 0 1 a b c d	0 0 1 a b c d 1 0 0 0 0
 * 1 1 0 a b c d	0 1 a b c d 1 0 0 0 0 0
 * 1 1 1 a b c d	1 a b c d 1 0 0 0 0 0 0
 *
 */

/* Shift values lookup table for above table for 7
 * highest sample value bits.
 */
static uint8_t alaw_encode_shifts[128] = {
	1, 1, 2, 2, 3, 3, 3, 3,
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

uint8_t sofm_a_law_encode(int16_t sample)
{
	int sign = SOFM_ALAW_SIGN_BIT;
	int shift = 0;
	int low_bits;
	uint8_t byte;

	/* Convert to 13 bits with shift */
	sample >>= 3;

	/* Negative samples are 1's complement with zero sign bit */
	if (sample < 0) {
		sign = 0;
		sample = -sample - 1;
	}

	if (sample > SOFM_ALAW_MAX)
		sample = SOFM_ALAW_MAX;

	if (sample > 31) {
		shift = alaw_encode_shifts[sample >> 5];
		low_bits = (sample >> shift) & SOFM_ALAW_MANTISSA_MASK;
	} else {
		low_bits = (sample >> 1) & SOFM_ALAW_MANTISSA_MASK;
	}

	byte = (shift << SOFM_ALAW_MANTISSA_BITS) | low_bits;
	byte = (byte | sign) ^ SOFM_ALAW_TOGGLE_EVEN_BITS;
	return byte;
}

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
int16_t sofm_a_law_decode(int8_t byte)
{
	int low_bits;
	int shift;
	int sign;
	int16_t value;

	byte ^= SOFM_ALAW_TOGGLE_EVEN_BITS;
	low_bits = byte & SOFM_ALAW_MANTISSA_MASK;
	shift = (byte >> SOFM_ALAW_MANTISSA_BITS) & SOFM_ALAW_SHIFT_MASK;
	sign = byte & SOFM_ALAW_SIGN_BIT;

	if (shift > 0)
		value = (low_bits << shift) | (SOFM_ALAW_DEC_ONES_MASK << (shift - 1));
	else
		value = (low_bits << 1) | 1;

	if (!sign)
		value = -value;

	/* Shift 13 bit Q1.12 to 16 bit Q1.15 */
	return value << 3;
}
