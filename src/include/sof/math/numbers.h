/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_NUMBERS_H__
#define __SOF_MATH_NUMBERS_H__

#include <stdint.h>

#ifndef __ZEPHYR__
/* Unsafe and portable macros for consistency with Zephyr.
 * See SEI CERT-C PRE31-C
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#endif /* ! __ZEPHYR__ */

#define ABS(a) ({		\
	typeof(a) __a = (a);	\
	__a < 0 ? -__a : __a;	\
})
#define SGN(a) ({		\
	typeof(a) __a = (a);	\
	__a < 0 ? -1 :		\
	__a > 0 ? 1 : 0;	\
})

int gcd(int a, int b); /* Calculate greatest common divisor for a and b */

/* This is a divide function that returns ceil of the quotient.
 * E.g. ceil_divide(9, 3) returns 3, ceil_divide(10, 3) returns 4.
 */
static inline int ceil_divide(int a, int b)
{
	int c;

	c = a / b;

	/* First, we check whether the signs of the params are different.
	 * If they are, we already know the result is going to be negative and
	 * therefore, is going to be already rounded up (truncated).
	 *
	 * If the signs are the same, we check if there was any remainder in
	 * the division by multiplying the number back.
	 */
	if (!((a ^ b) & (1 << ((sizeof(int) * 8) - 1))) && c * b != a)
		c++;

	return c;
}

/* Find indices of equal values in a vector of integer values */
int find_equal_int16(int16_t idx[], int16_t vec[], int n, int vec_length,
	int max_results);

/* Return the smallest value found in a vector */
int16_t find_min_int16(int16_t vec[], int vec_length);

/* Return the largest absolute value found in a vector */
int32_t find_max_abs_int32(int32_t vec[], int vec_length);

/* Count the left shift amount to normalize a 32 bit signed integer value
 * without causing overflow. Input value 0 will result to 31.
 */
int norm_int32(int32_t val);

uint32_t crc32(uint32_t base, const void *data, uint32_t bytes);

/* merges two 16-bit values into a single 32-bit value */
#define merge_16b16b(high, low) (((uint32_t)(high) << 16) | \
				 ((low) & 0xFFFF))

/* merges two 4-bit values into a single 8-bit value */
#define merge_4b4b(high, low) (((uint8_t)(high) << 4) | \
			       ((low) & 0xF))

/* Zephyr defines this - remove local copy once Zephyr integration complete */
#ifdef INT_MIN
#undef INT_MIN
#endif

/* Zephyr defines this - remove local copy once Zephyr integration complete */
#ifdef INT_MAX
#undef INT_MAX
#endif

/* Get max and min signed integer values for N bits word length */
#define INT_MAX(N)	((int64_t)((1ULL << ((N) - 1)) - 1))
#define INT_MIN(N)	((int64_t)(-((1ULL << ((N) - 1)) - 1) - 1))

#endif /* __SOF_MATH_NUMBERS_H__ */
