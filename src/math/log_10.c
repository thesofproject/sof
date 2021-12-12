// SPDX-License-Identifier: BSD-3-Clause/
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

#include <sof/math/log.h>
#include <sof/audio/format.h>
/**
 * Base-10 logarithm  log10(x)
 *
 * loge = (u) computes the base-10 logarithm of
 * u using lookup table.
 * input u must be scalar/real number and positive
 *
 * +------------------+-----------------+--------+--------+
 * | inpfxp	      |log10(returntype)| inpfxp |  loge  |
 * +----+-----+-------+----+----+-------+--------+--------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 * +----+-----+-------+----+----+-------+--------+--------+
 * | 32 | 0   |	 0    | 32 | 28 | 0     | 32.0	 | 4.28   |
 * +------------------+-----------------+--------+--------+
 * Arguments	: uint32_t numerator [1 to 4294967295, Q32.0]
 * Return Type	: uint32_t UQ4.28 [0 to 9.6329499409]
 */
uint32_t log10_int32(uint32_t numerator)
{
	return((uint32_t)Q_SHIFT_RND((int64_t)base2_logarithm(numerator) *
				     ONE_OVER_LOG2_10, 63, 32));
}
