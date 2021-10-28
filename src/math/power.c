// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

 /* Include Files */
#include <sof/math/power.h>

/* Function Declarations */
static int32_t div_s32s64(int64_t denominator);

/* Function Definitions */
/**
 * Arguments	: int64_t denominator
 *
 * Return Type	: int32_t
 */
static inline int32_t div_s32s64(int64_t denominator)
{
	uint64_t b_denominator;
	int32_t quotient;

	if (denominator == 0) {
		quotient = INT32_MAX;
	} else {
		if (denominator < 0)
			b_denominator = ~(uint64_t)denominator + 1;
		else
			b_denominator = (uint64_t)denominator;

		b_denominator = 1125899906842624ULL / b_denominator;
		if (denominator < 0L)
			quotient = (int32_t)-(int64_t)b_denominator;
		else
			quotient = (int32_t)b_denominator;
	}
	return quotient;
}

/**
 * Arguments	: int32_t b
 * base = [-1 to 2^5];Q7.25
 *
 *		: int32_t e
 * exponent = [-1 to 3];Q3.29
 *
 * Return Type	: int32_t
 * power = [-32768 to +32768];Q17.15
 */
int32_t power_scalar_function(int32_t b, int32_t e)
{
	int32_t i;
	int32_t k;
	int32_t multiplier;
	int32_t p;

	p = 32768;

	if (e < 0) {
		if (e <= INT32_MIN)
			e = INT32_MAX;
		else
			e = -e;

		if (b == 0)
			multiplier = INT32_MAX;
		else
			multiplier = div_s32s64((int64_t)b);

	} else {
		multiplier = b;
	}
	i = e >> 29;
	for (k = 0; k < i; k++)
		p = (int32_t)(((int64_t)p * (int64_t)multiplier) >> 25);

	return p;
}
