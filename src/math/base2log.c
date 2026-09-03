// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

#include <sof/math/log.h>

/* Defines Constant*/
#define BASE2LOG_WRAP_SCHAR_BITS 0xFF
#define BASE2LOG_UPPERBYTES 0xFFFFFF
#define BASE2LOG_WORDLENGTH 0x1F
/**
 *  Base-2 logarithm log2(n)
 *
 * Y = (u) computes the base-2 logarithm of
 * u using lookup table.
 * input u must be scalar/real number and positive
 *
 * +------------------+-----------------+--------+--------+
 * | u		      | y (returntype)	|   u	 |  y	  |
 * +----+-----+-------+----+----+-------+--------+--------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 * +----+-----+-------+----+----+-------+--------+--------+
 * | 32 | 0   |	 0    | 32 | 16 |   0	| 32.0	 | 16.16  |
 * +------------------+-----------------+--------+--------+
 *
 * Arguments	: uint32_t u
 * u input range [1 to 4294967295]
 *
 * Return Type	: int32_t  y
 * y output range [0 to 32]
 *
 */
int32_t base2_logarithm(uint32_t u)
{
	static const int32_t iv1[129] = {
	    0,	   736,	  1466,	 2190,	2909,  3623,  4331,  5034,  5732,  6425,  7112,	 7795,
	    8473,  9146,  9814,	 10477, 11136, 11791, 12440, 13086, 13727, 14363, 14996, 15624,
	    16248, 16868, 17484, 18096, 18704, 19308, 19909, 20505, 21098, 21687, 22272, 22854,
	    23433, 24007, 24579, 25146, 25711, 26272, 26830, 27384, 27936, 28484, 29029, 29571,
	    30109, 30645, 31178, 31707, 32234, 32758, 33279, 33797, 34312, 34825, 35334, 35841,
	    36346, 36847, 37346, 37842, 38336, 38827, 39316, 39802, 40286, 40767, 41246, 41722,
	    42196, 42667, 43137, 43603, 44068, 44530, 44990, 45448, 45904, 46357, 46809, 47258,
	    47705, 48150, 48593, 49034, 49472, 49909, 50344, 50776, 51207, 51636, 52063, 52488,
	    52911, 53332, 53751, 54169, 54584, 54998, 55410, 55820, 56229, 56635, 57040, 57443,
	    57845, 58245, 58643, 59039, 59434, 59827, 60219, 60609, 60997, 61384, 61769, 62152,
	    62534, 62915, 63294, 63671, 64047, 64421, 64794, 65166, 65536};
	static const int8_t iv[256] = {
	    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	    3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	    2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint64_t v;
	uint32_t x;
	int32_t a_i;
	int32_t l1_i;
	int32_t s_v;
	int32_t l2_i;
	int32_t l3_i;
	int num_left_shifts;
	int shift_factor;
	unsigned int slice_temp;
	/*  Skipping for-loop */
	/*  Unroll the loop so there will be no branching. */
	/*  Loop unroll execute 20% faster (CPU cycle count )than for loop */
	/*  Every iteration, look for leading zeros are in the high */
	/*  byte of V, and shift them out to the left. Continue with the */
	/*  shifted V for as many bytes as it has. */
	/*  The index is the high byte of the input plus 1 to make it a */
	/*  one-based index. */
	/*  Index into the number-of-leading-zeros lookup table.  This lookup */
	/*  table takes in a byte and returns the number of leading zeros in the */
	/*  binary representation. */
	shift_factor = iv[u >> 24];
	/*  Left-shift out all the leading zeros in the high byte. */
	v = (uint64_t)u << shift_factor;
	/*  Update the total number of left-shifts */
	num_left_shifts = shift_factor;
	shift_factor = iv[v >> 24];
	/*  Left-shift out all the leading zeros in the high byte. */
	v <<= shift_factor;
	/*  Update the total number of left-shifts */
	num_left_shifts += shift_factor;
	shift_factor = iv[v >> 24];
	/*  Left-shift out all the leading zeros in the high byte. */
	v <<= shift_factor;
	/*  Update the total number of left-shifts */
	num_left_shifts += shift_factor;
	shift_factor = iv[v >> 24];
	/*  Left-shift out all the leading zeros in the high byte. */
	/*  Update the total number of left-shifts */
	num_left_shifts += shift_factor;
	/*  The input has been left-shifted so the most-significant-bit is a 1. */
	/*  Reinterpret the output as unsigned with one integer bit, so */
	/*  that 1 <= x < 2. */
	x = (uint32_t)v << shift_factor;
	/*  Let Q = int(u).  Then u = Q*2^(-u_fraction_length), */
	/*  and x = Q*2^num_left_shifts * 2^(1-word_length).  Therefore, */
	/*  u = x*2^n, where n is defined as: */
	/*  Extract the high byte of x */
	/*  Convert the high byte into an index for lookup table */
	slice_temp = (v << shift_factor) >> 24;
	/*  Interpolate between points. */
	/*  The upper byte was used for the index into lookup table */
	/*  The remaining bits make up the fraction between points. */
	a_i = BASE2LOG_WORDLENGTH - num_left_shifts;
	a_i <<= 16;
	l1_i = iv1[(BASE2LOG_WRAP_SCHAR_BITS & ((x >> 24) + 129)) - 1];
	s_v = a_i + l1_i;
	l2_i = iv1[(BASE2LOG_WRAP_SCHAR_BITS & (slice_temp + 130)) - 1];
	l3_i = iv1[(slice_temp - 127) - 1];
	return s_v + (((x & BASE2LOG_UPPERBYTES) * (int64_t)(l2_i - l3_i)) >> 24);
}

