// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

#include <sof/math/log.h>
#include <sof/audio/format.h>
/**
 * Base-e logarithm  loge(x)
 *
 * loge = (u) computes the base-e logarithm of
 * u using lookup table.
 * input u must be scalar/real number and positive
 *
 * +------------------+-----------------+--------+--------+
 * | inpfxp	      |loge (returntype)| inpfxp |  loge  |
 * +----+-----+-------+----+----+-------+--------+--------+
 * |WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 * +----+-----+-------+----+----+-------+--------+--------+
 * | 32 | 0   |	 0    | 32 | 27 |   0	| 32.0	 | 5.27	  |
 * +------------------+-----------------+--------+--------+
 *
 * Arguments	: uint32_t numerator [1 to 4294967295- Q32.0]
 * Return Type	: uint32_t UQ5.27 [ 0 to 22.1808076352]
 */
uint32_t ln_int32(uint32_t numerator)
{
	return((uint32_t)Q_SHIFT_RND((int64_t)base2_logarithm(numerator) *
				     ONE_OVER_LOG2_E, 64, 32));
}
