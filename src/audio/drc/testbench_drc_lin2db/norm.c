// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
#include "typdef.h"

/* Count the left shift amount to normalize a 32 bit signed integer value
 * without causing overflow. Input value 0 will result to 31.
 */
int norm_int32(int32_t val)
{
	int s;
	int32_t n;

	if (!val)
		return 31;

	n = val << 1;
	s = 0;
	if (val > 0) {
		while (n > 0) {
			n = n << 1;
			s++;
		}
	} 	else { /* we don't goto else section */
		while (n < 0) {
			n = n << 1;
			s++;
		}
	}
	return s;
}
