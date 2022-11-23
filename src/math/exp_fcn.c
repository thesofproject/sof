// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
//
//

/* Include Files */
#include "exp_fcn.h"
#include <stdint.h>
#include <stdbool.h>


/* Function Declarations */
int64_t lomul_s64_SR_sat_near(int64_t a, int64_t b);

void wide_mul_s64(int64_t in_0, int64_t in_1, uint64_t *BitsHiptrOut, uint64_t *BitsLoptrOut);
/*
 * Arguments	: int64_t a
 *		  int64_t b
 * Return Type	: int64_t
 */
int64_t lomul_s64_SR_sat_near(int64_t a, int64_t b)
{
	int64_t result;
	uint64_t u64_rhi;
	uint64_t u64_rlo;
	bool roundup;
	wide_mul_s64(a, b, &u64_rhi, &u64_rlo);
	roundup = ((u64_rlo & BIT_MASK_LOW_Q27P5) != 0ULL);
	u64_rlo =
	    (u64_rhi << 36 | u64_rlo >> 28) + (roundup ? (uint64_t)1 : (uint64_t)0);
	return (int64_t)u64_rlo;
}

/*
 * Arguments	: int64_t in_0
 *		  int64_t in_1
 *		  uint64_t *BitsHiptrOut
 *		  uint64_t *BitsLoptrOut
 * Return Type	: void
 */
void wide_mul_s64(int64_t in_0, int64_t in_1, uint64_t *BitsHiptrOut, uint64_t *BitsLoptrOut)
{
	uint64_t absIn0;
	uint64_t absIn1;
	uint64_t in0Hi;
	uint64_t in0Lo;
	uint64_t in1Hi;
	uint64_t prodHiLo;
	uint64_t prodLoHi;
	absIn0 = (in_0 < 0LL) ? ~(uint64_t)in_0 + 1ULL : (uint64_t)in_0;
	absIn1 = (in_1 < 0LL) ? ~(uint64_t)in_1 + 1ULL : (uint64_t)in_1;
	in0Hi = absIn0 >> 32ULL;
	in0Lo = absIn0 & UINT32_MAX;
	in1Hi = absIn1 >> 32ULL;
	absIn0 = absIn1 & UINT32_MAX;
	prodHiLo = in0Hi * absIn0;
	prodLoHi = in0Lo * in1Hi;
	absIn0 *= in0Lo;
	absIn1 = 0ULL;
	in0Lo = absIn0 + (prodLoHi << 32ULL);
	if (in0Lo < absIn0) {
		absIn1 = 1ULL;
	}
	absIn0 = in0Lo;
	in0Lo += prodHiLo << 32ULL;
	if (in0Lo < absIn0) {
		absIn1++;
	}
	absIn0 = ((absIn1 + in0Hi * in1Hi) + (prodLoHi >> 32ULL)) + (prodHiLo >> 32ULL);
	if ((in_0 != 0LL) && ((in_1 != 0LL) && ((in_0 > 0LL) != (in_1 > 0LL)))) {
		absIn0 = ~absIn0;
		in0Lo = ~in0Lo;
		in0Lo++;
		if (in0Lo == 0ULL) {
			absIn0++;
		}
	}
	*BitsHiptrOut = absIn0;
	*BitsLoptrOut = in0Lo;
}

/* f(x) = a^x, x is variable and a is base
 *
 * Arguments	: int32_t x(Q4.28)
 * input range -5 to 5
 *
 * Return Type	: int32_t ts(Q9.23)
 * output range 0.0067465305 to 148.4131488800
 *+------------------+-----------------+--------+--------+
 *| x		     | ts (returntype) |   x    |  ts    |
 *+----+-----+-------+----+----+-------+--------+--------+
 *|WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 *+----+-----+-------+----+----+-------+--------+--------+
 *| 32 | 28  |  1    | 32 | 23 |   0   | 4.28   | 9.23   |
 *+------------------+-----------------+--------+--------+
 */
int32_t exp_int32(int32_t x)
{
	/* LUT = ceil(1/factorial(n) * 2 ^ 63) */
	static const int64_t iv[19] = {
			4611686018427387904LL,
			1537228672809129301LL,
			384307168202282325LL,
			76861433640456465LL,
			12810238940076077LL,
			1830034134296582LL,
			228754266787072LL,
			25417140754119LL,
			2541714075411LL,
			231064915946LL,
			19255409662LL,
			1481185358LL,
			105798954LL,
			7053264LL,
			440829LL,
			25931LL,
			1441LL,
			76LL,
			4LL
	};

	int64_t dividend;
	int32_t ts;
	int32_t n;
	int64_t qt;
	uint64_t ou0Hi;
	uint64_t ou0Lo;
	dividend = NUM_Q14P18;

	ts = 0;//Q23.9
	n = 0; //Q5.26
	// pre-computation of 1st & 2nd terms
	ts =  TERMS_Q23P9; //Q23.9

	dividend = (x + (1 << 13)) >> 14; //x in Q50.14

	wide_mul_s64(dividend, BIT_MASK_Q63P1, &ou0Hi, &ou0Lo);
	qt = (ou0Hi << 46ULL) | (ou0Lo >> 18ULL);//Q6.26
	ts += (int32_t)((qt >> 35ULL) +
			 (int64_t)((qt & QUOTIENT_SCALE) != 0LL ? (int32_t)1 : (int32_t)0));
	dividend = lomul_s64_SR_sat_near(dividend, (int64_t)x);
	do {
		int64_t sign;
		if (divisor == 0LL)
		{
			qt = dividend < 0LL ? INT64_MIN : INT64_MAX;
		} else {
			wide_mul_s64(dividend, iv[n], &ou0Hi, &ou0Lo);
			qt = (int64_t)(ou0Hi << 45ULL) | (ou0Lo >> 19ULL);
		}

		// sum of the remaining terms
		ts += (int32_t)((qt >> 35ULL) +
				 (int64_t)((qt & QUOTIENT_SCALE) != 0LL ? (int32_t)1 : (int32_t)0));
		(n)++;
		dividend = lomul_s64_SR_sat_near(dividend, (int64_t)x);

		sign = (qt <= INT64_MIN) ? INT64_MAX : -qt;
		if (qt < 0LL) {
			qt = sign;
		}
	} while (!(qt <= CONVERG_ERROR));
	return ts;
}
