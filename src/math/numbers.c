/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *	 Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	 Keyon Jie <yang.jie@linux.intel.com>
 */

/* Euclidean algorithm for greatest common denominator from
 * pseudocode in
 * https://en.wikipedia.org/wiki/Euclidean_algorithm#Implementations
 */

#include <sof/math/numbers.h>
#include <sof/audio/format.h>

int gcd(int a, int b)
{
	int t;
	while (b != 0) {
		t = b;
		b = a % b;
		a = t;
	}
	return a;
}

/* This function searches from vec[] (of length vec_length) integer values
 * of n. The indexes to equal values is returned in idx[]. The function
 * returns the number of found matches. The max_results should be set to
 * 0 (or negative) or vec_length get all the matches. The max_result can be set
 * to 1 to receive only the first match in ascending order. It avoids need
 * for an array for idx.
 */
int find_equal_int16(int16_t idx[], int16_t vec[], int n, int vec_length,
	int max_results)
{
	int nresults = 0;
	int i;

	for (i = 0; i < vec_length; i++) {
		if (vec[i] == n) {
			idx[nresults++] = i;
			if (nresults == max_results)
				break;
		}
	}

	return nresults;
}

/* Return the smallest value found in the vector */
int16_t find_min_int16(int16_t vec[], int vec_length)
{
	int i;
	int min = vec[0];

	for (i = 1; i < vec_length; i++)
		min = (vec[i] < min) ? vec[i] : min;

	return min;
}

/* Return the largest absolute value found in the vector. Note that
 * smallest negative value need to be saturated to preset as int32_t.
 */
int32_t find_max_abs_int32(int32_t vec[], int vec_length)
{
	int i;
	int64_t amax = (vec[0] > 0) ? vec[0] : -vec[0];

	for (i = 1; i < vec_length; i++) {
		amax = (vec[i] > amax) ? vec[i] : amax;
		amax = (-vec[i] > amax) ? -vec[i] : amax;
	}

	return SATP_INT32(amax); /* Amax is always a positive value */
}

/* Count the left shift amount to normalize a 32 bit signed integer value
 * without causing overflow. Input value 0 will result to 31.
 */
int norm_int32(int32_t val)
{
	int s;
	int32_t n;

	if (!val)
		return 31;

	if (val > 0) {
		n = val << 1;
		s = 0;
		while (n > 0) {
			n = n << 1;
			s++;
		}
	} else {
		n = val << 1;
		s = 0;
		while (n < 0) {
			n = n << 1;
			s++;
		}
	}
	return s;
}

/**
 * Basic CRC-32 implementation, based on pseudo-code from
 * https://en.wikipedia.org/wiki/Cyclic_redundancy_check#CRC-32_algorithm
 * 0xEDB88320 is the reversed polynomial representation
 */
uint32_t crc32(const void *data, uint32_t bytes)
{
	uint32_t crc = 0xFFFFFFFF;
	uint32_t cur;
	int i;
	int j;

	for (i = 0; i < bytes; ++i) {
		cur = (crc ^ ((const uint8_t *)data)[i]) & 0xFF;

		for (j = 0; j < 8; ++j)
			cur = cur & 1 ? (cur >> 1) ^ 0xEDB88320 : cur >> 1;

		crc = cur ^ (crc >> 8);
	}

	return ~crc;
}
