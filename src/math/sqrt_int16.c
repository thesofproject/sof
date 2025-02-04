// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

#include <rtos/symbol.h>
#include <sof/math/sqrt.h>

#define SQRT_WRAP_SCHAR_BITS 0xFF

/*
 *  Square root
 *
 *  Y = SQRTLOOKUP_INT16(U) computes the square root of
 *  U using lookup tables.
 *  Range of u is [0 to 65535]
 *  Range of y is [0 to 4]
 * +------------------+-----------------+--------+--------+
 * | u		      | y (returntype)	|   u	 |  y	  |
 * +----+-----+-------+----+----+-------+--------+--------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 * +----+-----+-------+----+----+-------+--------+--------+
 * | 16 | 12  |	 0    | 16 | 12 |   1	| 4.12	 | 4.12	  |
 * +------------------+-----------------+--------+--------+

 * Arguments	: uint16_t u
 * Return Type	: int32_t
 */
uint16_t sqrt_int16(uint16_t u)
{
	static const int32_t iv1[193] = {
	    46341, 46702, 47059, 47415, 47767, 48117, 48465, 48809, 49152, 49492, 49830, 50166,
	    50499, 50830, 51159, 51486, 51811, 52134, 52454, 52773, 53090, 53405, 53719, 54030,
	    54340, 54647, 54954, 55258, 55561, 55862, 56162, 56459, 56756, 57051, 57344, 57636,
	    57926, 58215, 58503, 58789, 59073, 59357, 59639, 59919, 60199, 60477, 60753, 61029,
	    61303, 61576, 61848, 62119, 62388, 62657, 62924, 63190, 63455, 63719, 63982, 64243,
	    64504, 64763, 65022, 65279, 65536, 65792, 66046, 66300, 66552, 66804, 67054, 67304,
	    67553, 67801, 68048, 68294, 68539, 68784, 69027, 69270, 69511, 69752, 69992, 70232,
	    70470, 70708, 70945, 71181, 71416, 71651, 71885, 72118, 72350, 72581, 72812, 73042,
	    73271, 73500, 73728, 73955, 74182, 74408, 74633, 74857, 75081, 75304, 75527, 75748,
	    75969, 76190, 76410, 76629, 76848, 77066, 77283, 77500, 77716, 77932, 78147, 78361,
	    78575, 78788, 79001, 79213, 79424, 79635, 79846, 80056, 80265, 80474, 80682, 80890,
	    81097, 81303, 81509, 81715, 81920, 82125, 82329, 82532, 82735, 82938, 83140, 83341,
	    83542, 83743, 83943, 84143, 84342, 84540, 84739, 84936, 85134, 85331, 85527, 85723,
	    85918, 86113, 86308, 86502, 86696, 86889, 87082, 87275, 87467, 87658, 87849, 88040,
	    88231, 88420, 88610, 88799, 88988, 89176, 89364, 89552, 89739, 89926, 90112, 90298,
	    90484, 90669, 90854, 91038, 91222, 91406, 91589, 91772, 91955, 92137, 92319, 92501,
	    92682};
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
	int32_t xfi_tmp;
	int32_t y;
	uint16_t v;
	int32_t a_i;
	int32_t l1_i;
	int32_t l2_i;
	int num_left_shifts;
	int shift_factor;
	int xfi;
	unsigned int slice_temp;
	int sign = 1;

	if (!u)
		return 0;
	/* Normalize the input such that u = x * 2^n and 0.5 <= x < 2
	 * Normalize to the range [1, 2)
	 * normalizes the input U
	 * such that the output X is
	 * U = X*2^N
	 * 1 <= X < 2
	 * The output X is unsigned with one integer bit.
	 * The input U must be scalar and positive.
	 * The number of bits in a byte is assumed to be B=8.
	 * Reinterpret the input as an unsigned integer.
	 * Unroll the loop in generated code so there will be no branching.
	 * For each iteration, see how many leading zeros are in the high
	 * byte of V, and shift them out to the left. Continue with the
	 * shifted V for as many bytes as it has.
	 * The index is the high byte of the input plus 1 to make it a
	 * one-based index.
	 * Index into the number-of-leading-zeros lookup table.  This lookup
	 * table takes in a byte and returns the number of leading zeros in the
	 * binary representation.
	 */

	shift_factor = iv[u >> 8];
	/*  Left-shift out all the leading zeros in the high byte. */
	v = u << shift_factor;
	/*  Update the total number of left-shifts */
	num_left_shifts = shift_factor;
	/* For each iteration, see how many leading zeros are in the high
	 * byte of V, and shift them out to the left. Continue with the
	 * shifted V for as many bytes as it has.
	 * The index is the high byte of the input plus 1 to make it a
	 * one-based index.
	 * Index into the number-of-leading-zeros lookup table.  This lookup
	 * table takes in a byte and returns the number of leading zeros in the
	 * binary representation.
	 */
	shift_factor = iv[v >> 8];
	/*  Left-shift out all the leading zeros in the high byte.
	 *  Update the total number of left-shifts
	 */
	num_left_shifts += shift_factor;
	/*  The input has been left-shifted so the most-significant-bit is a 1.
	 *  Reinterpret the output as unsigned with one integer bit, so
	 *  that 1 <= x < 2.
	 *  Let Q = int(u).  Then u = Q*2^(-u_fraction_length),
	 *  and x = Q*2^num_left_shifts * 2^(1-word_length).  Therefore,
	 *  u = x*2^n, where n is defined as:
	 */
	xfi_tmp = (3 - num_left_shifts) & 1;
	v = (v << shift_factor) >> xfi_tmp;
	/*  Extract the high byte of x */
	/*  Convert the high byte into an index for SQRTLUT */
	slice_temp = SQRT_WRAP_SCHAR_BITS & (v >> 8);
	/*  The upper byte was used for the index into SQRTLUT.
	 *  The remainder, r, interpreted as a fraction, is used to
	 *  linearly interpolate between points.
	 */
	a_i = iv1[((v >> 8) - 63) - 1];
	a_i <<= 8;
	l1_i = iv1[SQRT_WRAP_SCHAR_BITS & ((slice_temp + 194) - 1)];
	l2_i = iv1[(slice_temp - 63) - 1];
	y = a_i + (v & SQRT_WRAP_SCHAR_BITS) * (l1_i - l2_i);
	xfi = (((xfi_tmp - num_left_shifts) + 3) >> 1);
	shift_factor = (((xfi_tmp - num_left_shifts) + 3) >> 1);
	if (xfi != 0) {
		if (xfi > 0)
			y <<= (shift_factor >= 32) ? 0 : shift_factor;
		else
			y >>= -sign * xfi;
	}
	y = ((y >> 11) + 1) >> 1;

	return y;
}
EXPORT_SYMBOL(sqrt_int16);
