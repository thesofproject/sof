// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

/* Include Files */
#include <sof/math/log.h>

/**
 *  Base-2 logarithm
 *
 *  Y = (U) computes the base-2 logarithm of
 *  U using lookup table.
 *
 * Arguments	: uint32_t u[Q31.1]
 * Return Type	: int32_t  y[Q16.16]
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
	uint32_t v;
	uint32_t x;
	int8_t Num_left_shifts;
	int8_t shift_factor;
	uint8_t slice_temp;

	shift_factor = iv[(u >> 24)];
	v = u << (uint64_t)shift_factor;
	Num_left_shifts = shift_factor;
	shift_factor = iv[(v >> 24)];
	v <<= (uint64_t)shift_factor;
	Num_left_shifts += shift_factor;
	shift_factor = iv[(v >> 24)];
	v <<= (uint64_t)shift_factor;
	Num_left_shifts += shift_factor;
	shift_factor = iv[(v >> 24)];
	Num_left_shifts += shift_factor;
	x = v << (uint64_t)shift_factor;
	slice_temp = (uint8_t)((v << (uint64_t)shift_factor) >> 24);
	/*  Interpolate between points. */
	/*  The upper byte was used for the index into lookup table */
	/*  The remaining bits make up the fraction between points. */
	return (((31 - Num_left_shifts) << 16) +
		iv1[(uint8_t)((x >> 24) + 129) - 1]) +
	       ((((x & 16777215) *
			   (int64_t)(int32_t)
			   (iv1[(uint8_t)(slice_temp + 130) - 1] -
			    iv1[(uint8_t)(slice_temp - 127) - 1])) >>
			  8) >> 16);
}

