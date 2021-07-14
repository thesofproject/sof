/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */

#ifndef __SOF_MATH_CORDIC_H__
#define __SOF_MATH_CORDIC_H__

#include <stdint.h>

#define CORDIC_31B_TABLE_SIZE		31
#define CORDIC_15B_TABLE_SIZE		15
#define CORDIC_30B_ITABLE_SIZE		30
#define CORDIC_16B_ITABLE_SIZE		16

/**
 * cordic_atan2_lookup_table = atan(2.^-(0:N-1)) N = 31/16
 * CORDIC Gain is cordic_gain = prod(sqrt(1 + 2.^(-2*(0:31/16-1))))
 * Inverse CORDIC Gain,inverse_cordic_gain = 1 / cordic_gain
 */
static const int32_t cordic_lookup[CORDIC_31B_TABLE_SIZE] = { 843314857, 497837829,
	263043837, 133525159, 67021687, 33543516, 16775851, 8388437, 4194283, 2097149,
	1048576, 524288, 262144, 131072, 65536, 32768, 16384, 8192, 4096, 2048, 1024,
	512, 256, 128, 64, 32, 16, 8, 4, 2, 1 };
 /* calculate LUT = 2*atan(2.^(-1:-1:(nIters)+1)) */
static const int32_t cordic_ilookup[CORDIC_30B_ITABLE_SIZE] = {
	    497837829, 263043836, 133525158, 67021686, 33543515, 16775850,
	    8388437,   4194282,	  2097149,   1048575,  524287,	 262143,
	    131071,    65535,	  32767,     16383,    8191,	 4095,
	    2047,      1023,	  511,	     255,      127,	 63,
	    31,	       15,	  8,	     4,	       2,	 1};

#endif /* __SOF_MATH_CORDIC_H__ */
