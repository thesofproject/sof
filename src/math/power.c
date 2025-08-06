// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//	   Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//

 /* Include Files */
#include <sof/audio/format.h>
#include <sof/trace/trace.h>
#include <sof/math/power.h>
#include <sof/lib/uuid.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <stdint.h>

SOF_DEFINE_REG_UUID(math_power);

DECLARE_TR_CTX(math_power_tr, SOF_UUID(math_power_uuid), LOG_LEVEL_INFO);
/* define constant */
#define POWER_MAX_LIMIT 0x8000

/* Function Definitions */

/**
 * Arguments	: int32_t b
 * base input range [-32 to +32]
 *
 *		: int32_t e
 * exponent input range [ -3 to +3 ]
 *
 * Return Type	: int32_t
 * power output range [-32768 to + 32768]
 * ATTENTION - FRACTIONAL EXPONENTIAL IS NOT SUPPORTED
 * +------------------+------------------+------------------+--------+--------+--------+
 * | base	      | exponent	 | power(returntype)|	b    |	e     |	 p     |
 * +----+-----+-------+----+-----+-------+----+----+--------+--------+--------+--------+
 * |WLen| FLen|Signbit|WLen| FLen|Signbit|WLen|FLen|Signbit | Qformat| Qformat| Qformat|
 * +----+-----+-------+----+-----+-------+----+----+--------+--------+--------+--------+
 * | 32 | 25  |	 1    | 32 | 29	 |  1	 | 32 | 15 |   1    | 6.26   | 2.30   | 16.16  |
 * +------------------+------------------+------------------+--------+--------+--------+
 */
int32_t power_int32(int32_t b, int32_t e)
{
	int32_t i;
	int32_t k;
	int32_t multiplier;
	int32_t p;

	p = POWER_MAX_LIMIT;

	if (e < 0) {
		e = -e;
		if (b) {
			multiplier = (int32_t)((1LL << 50) / (int64_t)b);
		} else {
			multiplier = INT32_MAX;
			tr_err(&math_power_tr, "Divide by zero error.");
		}
	} else {
		multiplier = b;
	}
	i = e >> 29;
	for (k = 0; k < i; k++)
		p = sat_int32((((p * (int64_t)multiplier) >> 24) + 1) >> 1);

	return p;
}
