// SPDX-License-Identifier: BSD-3-Clause
/*
 *Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 *
 */

#include <sof/math/exp_fcn.h>
#include <sof/common.h>
#include <rtos/symbol.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#if defined(SOFM_EXPONENTIAL_HIFI3) || defined(SOFM_EXPONENTIAL_HIFI4) || \
	    defined(SOFM_EXPONENTIAL_HIFI5)

#if XCHAL_HAVE_HIFI5
#include <xtensa/tie/xt_hifi5.h>
#elif XCHAL_HAVE_HIFI4
#include <xtensa/tie/xt_hifi4.h>
#else
#include <xtensa/tie/xt_hifi3.h>
#endif

#include <xtensa/tie/xt_hifi2.h>
#include <xtensa/tie/xt_FP.h>

#define SOFM_CONVERG_ERROR 28823037624320LL /* error smaller than 1e-4,1/2 ^ -44.7122876209085 */

/*
 * Arguments	: int64_t in_0
 *		  int64_t in_1
 *		  uint64_t *ptroutbitshi
 *		  uint64_t *ptroutbitslo
 * Return Type	: void
 * Description:Perform element-wise multiplication on in_0 and in_1
 * while keeping the required product word length and fractional
 * length in mind. mul_s64 function divide the 64-bit quantities
 * into two 32-bit words,multiply the low words to produce the
 * lowest and second-lowest words in the result, then both pairs
 * of low and high words from different numbers to produce the
 * second and third lowest words in the result, and finally both
 * high words to produce the two highest words in the outcome.
 * Add them all up, taking carry into consideration.
 *
 * The 64 x 64 bit multiplication of operands in_0 and in_1 is
 * shown in the image below. The 64-bit operand in_0,in_1 is
 * represented by the notation in0_H, in1_H for the top 32 bits
 * and in0_L, in1_L for the bottom 32 bits.
 *
 *				in0_H : in0_L
 *			x	in1_H : in1_L
 *			---------------------
 *	P0			in0_L x in1_L
 *	P1		in0_H x in1_L		64 bit inner multiplication
 *	P2		in0_L x in1_H		64 bit inner multiplication
 *	P3	in0_H x in1_H
 *			--------------------
 *			[64 x 64 bit multiplication] sum of inner products
 * All combinations are multiplied by one another and then added.
 * Each inner product is moved into its proper power location.given the names
 * of the inner products, redoing the addition where 000 represents 32 zero
 * bits.The inner products can be added together in 64 bit addition.The sum
 * of two 64-bit numbers yields a 65-bit output.
 *		   (P0H:P0L)
 *		P1H(P1L:000)
 *		P2H(P2L:000)
 *	P3H:P3L(000:000)
 *	.......(aaa:P0L)
 * By combining P0H:P0L and P1L:000. This can lead to a carry, denote as CRY0.
 * The partial result is then multiplied by P2L:000.
 * We call it CRY1 because it has the potential to carry again.
 *	(CRY0 + CRY1)P0H:P0L
 *	(	 P1H)P1L:000
 *	(	 P2H)P2L:000
 *	(P3H:	 P3L)000:000
 *	--------------------
 *	(ccc:bbb)aaa:P0L
 * P1H, P2H, and P3H:P3L are added to the carry CRY0 + CRY1.This increase will
 * not result in an overflow.
 *
 */
static void mul_s64(ae_int64 in_0, ae_int64 in_1, ae_int64 *__restrict__ ptroutbitshi,
		    ae_int64 *__restrict__ ptroutbitslo)
{
	ae_int64 producthihi, producthilo, productlolo;
	ae_int64 producthi, product_hl_lh_h, product_hl_lh_l, carry;

#if (SOFM_EXPONENTIAL_HIFI4 == 1 || SOFM_EXPONENTIAL_HIFI5 == 1)

	ae_int32x2 in0_32 = AE_MOVINT32X2_FROMINT64(in_0);
	ae_int32x2 in1_32 = AE_MOVINT32X2_FROMINT64(in_1);

	ae_ep ep_lolo = AE_MOVEA(0);
	ae_ep ep_hilo = AE_MOVEA(0);
	ae_ep ep_HL_LH = AE_MOVEA(0);

	producthihi = AE_MUL32_HH(in0_32, in1_32);

	/* AE_MULZAAD32USEP.HL.LH - Unsigned lower parts and signed higher 32-bit parts dual */
	/* multiply and accumulate operation on two 32x2 operands with 72-bit output */
	/* Input-32x32-bit(in1_32xin0_32)into 72-bit multiplication operations */
	/* Output-lower 64 bits of the result are stored in producthilo */
	/* Output-upper eight bits are stored in ep_hilo */
	AE_MULZAAD32USEP_HL_LH(ep_hilo, producthilo, in1_32, in0_32);
	productlolo = AE_MUL32U_LL(in0_32, in1_32);

	product_hl_lh_h = AE_SRAI72(ep_hilo, producthilo, 32);
	product_hl_lh_l = AE_SLAI64(producthilo, 32);

	/* The AE_ADD72 procedure adds two 72-bit elements. The first 72-bit value is created */
	/* by concatenating the MSBs and LSBs of operands ep[7:0] and d[63:0]. Similarly, the */
	/* second value is created by concatenating bits from operands ep1[7:0] and d1[63:0]. */
	AE_ADD72(ep_lolo, productlolo, ep_HL_LH, product_hl_lh_l);

	carry = AE_SRAI72(ep_lolo, productlolo, 32);

	carry = AE_SRLI64(carry, 32);
	producthi = AE_ADD64(producthihi, carry);

	producthi = AE_ADD64(producthi, product_hl_lh_h);
	*ptroutbitslo = productlolo;
#elif SOFM_EXPONENTIAL_HIFI3 == 1

	ae_int64 producthi_1c;
	ae_int64 producthi_2c;
	ae_int64 productlo_2c;
	ae_int64 productlo;

	ae_int64 s0 = AE_SRLI64(in_0, 63);
	ae_int64 s1 = AE_SRLI64(in_1, 63);
	bool x_or = (bool)((int)(int64_t)s0 ^ (int)(int64_t)s1);

	ae_int32x2 in0_32 = AE_MOVINT32X2_FROMINT64(AE_ABS64(in_0));
	ae_int32x2 in1_32 = AE_MOVINT32X2_FROMINT64(AE_ABS64(in_1));

	producthihi = AE_MUL32_HH(in0_32, in1_32);
	producthilo = AE_MUL32U_LL(in1_32, AE_MOVINT32X2_FROMINT64
				  ((AE_MOVINT64_FROMINT32X2(in0_32) >> 32)));
	producthilo += AE_MUL32U_LL(AE_MOVINT32X2_FROMINT64
				  ((AE_MOVINT64_FROMINT32X2(in1_32) >> 32)), in0_32);
	productlolo = AE_MUL32U_LL(in0_32, in1_32);

	product_hl_lh_h = AE_SRAI64(producthilo, 32);
	product_hl_lh_l = AE_SLAI64(producthilo, 32);

	productlo = AE_ADD64(productlolo, product_hl_lh_l);
	producthi = AE_ADD64(producthihi, product_hl_lh_h);

	carry = AE_ADD64(AE_SRLI64(productlolo, 1), AE_SRLI64(product_hl_lh_l, 1));
	carry = AE_SRLI64(carry, 63);
	producthi = AE_ADD64(producthi, carry);

	producthi_1c = AE_NOT64(producthi);
	producthi_2c = AE_NEG64(producthi);
	productlo_2c = AE_NEG64(productlo);

	if (x_or) {
		if (productlo == (ae_int64)0) {
			producthi = producthi_2c;
		} else {
			producthi = producthi_1c;
			productlo = productlo_2c;
		}
	}
	*ptroutbitslo = productlo;
#endif //(XCHAL_HAVE_HIFI4 || XCHAL_HAVE_HIFI5)

	*ptroutbitshi = producthi;
}

/*
 * Arguments	: int64_t a
 *		  int64_t b
 * Return Type	: int64_t
 */
static int64_t lomul_s64_sr_sat_near(int64_t a, int64_t b)
{
	ae_int64 u64_chi;
	ae_int64 u64_clo;
	ae_int64 temp;

	mul_s64(a, b, &u64_chi, &u64_clo);

	ae_int64 roundup = AE_AND64(u64_clo, SOFM_EXP_BIT_MASK_LOW_Q27P5);

	roundup = AE_SRLI64(roundup, 27);
	temp = AE_OR64(AE_SLAI64(u64_chi, 36), AE_SRLI64(u64_clo, 28));

	return AE_ADD64(temp, roundup);
}

static const int64_t onebyfact_Q63[19] = {
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

/* f(x) = a^x, x is variable and a is base
 *
 * Arguments	: int32_t x(Q4.28)
 * input range -5 to 5
 *
 * Return Type	: int32_t ts(Q9.23)
 * output range 0.0067465305 to 148.4131488800
 *+------------------+-----------------+--------+--------+
 *| x		     | ts (returntype) |   x	|  ts	 |
 *+----+-----+-------+----+----+-------+--------+--------+
 *|WLen| FLen|Signbit|WLen|FLen|Signbit| Qformat| Qformat|
 *+----+-----+-------+----+----+-------+--------+--------+
 *| 32 | 28  |	1    | 32 | 23 |   0   | 4.28	| 9.23	 |
 *+------------------+-----------------+--------+--------+
 */
int32_t sofm_exp_int32(int32_t x)
{
	ae_int64 outhi;
	ae_int64 outlo;
	ae_int64 qt;
	ae_int64 onebyfact;
	ae_int64 temp;

	ae_int64 *ponebyfact_Q63 = (ae_int64 *)onebyfact_Q63;
	ae_int64 ts = SOFM_EXP_TERMS_Q23P9;
	ae_int64 mp = (x + SOFM_EXP_LSHIFT_BITS) >> 14; /* x in Q50.14 */;
	xtbool flag;
	int64_t b_n;

	mul_s64(mp, SOFM_EXP_BIT_MASK_Q62P2, &outhi, &outlo);
	qt = AE_OR64(AE_SLAI64(outhi, 46), AE_SRLI64(outlo, 18));

	temp = AE_SRAI64(AE_ADD64(qt, SOFM_EXP_QUOTIENT_SCALE), 35);

	ts = AE_ADD64(ts, temp);

	mp = lomul_s64_sr_sat_near(mp, (int64_t)x);

	for (b_n = 0; b_n < 64;) {
		AE_L64_IP(onebyfact, ponebyfact_Q63, 8);

		mul_s64(mp, onebyfact, &outhi, &outlo);
		qt = AE_OR64(AE_SLAI64(outhi, 45), AE_SRLI64(outlo, 19));

		temp = AE_SRAI64(AE_ADD64(qt, SOFM_EXP_QUOTIENT_SCALE), 35);
		ts = AE_ADD64(ts, temp);

		mp = lomul_s64_sr_sat_near(mp, (int64_t)x);

		const ae_int64 sign = AE_NEG64(qt);

		flag = AE_LT64(qt, 0);
		AE_MOVT64(qt, sign, flag);

		if (!(qt < (ae_int64)SOFM_CONVERG_ERROR))
			b_n++;
		else
			b_n = 64;
	}

	return AE_MOVAD32_L(AE_MOVINT32X2_FROMINT64(ts));
}

/* Fractional multiplication with shift and round
 * Note that the parameters px and py must be cast to (int64_t) if other type.
 */
static inline int exp_hifi_q_multsr_32x32(int a, int b, int c, int d, int e)
{
	ae_int64 res;
	int xt_o;
	int shift;

	res = AE_MUL32_LL(a, b);
	shift = XT_SUB(XT_ADD(c, d), XT_ADD(e, 1));
	res = AE_SRAA64(res, shift);
	res = AE_ADD64(res, 1);
	res = AE_SRAI64(res, 1);
	xt_o = AE_MOVINT32_FROMINT64(res);

	return xt_o;
}

/* A macro for Q-shifts */
static inline int exp_hifi_q_shift_rnd(int a, int b, int c)
{
	ae_int32 res;
	int shift;

	shift = XT_SUB(b, XT_ADD(c, 1));
	res = AE_SRAA32(a, shift);
	res = AE_ADD32(res, 1);
	res = AE_SRAI32(res, 1);

	return res;
}

/* Alternative version since compiler does not allow (x >> -1) */
static inline int exp_hifi_q_shift_left(int a, int b, int c)
{
	ae_int32 xt_o;
	int shift;

	shift = XT_SUB(c, b);
	xt_o = AE_SLAA32(a, shift);

	return xt_o;
}

#define q_mult(a, b, qa, qb, qy) ((int32_t)exp_hifi_q_multsr_32x32((int64_t)(a), b, qa, qb, qy))
/* Fixed point exponent function for approximate range -11.5 .. 7.6
 * that corresponds to decibels range -100 .. +66 dB.
 *
 * The functions uses rule exp(x) = exp(x/2) * exp(x/2) to reduce
 * the input argument for private small value exp() function that is
 * accurate with input range -2.0 .. +2.0. The number of possible
 * divisions by 2 is computed into variable n. The returned value is
 * exp()^(2^n).
 *
 * Input  is Q5.27, -16.0 .. +16.0, but note the input range limitation
 * Output is Q12.20, 0.0 .. +2048.0
 */

int32_t sofm_exp_fixed(int32_t x)
{
	int32_t xs;
	int32_t y;
	int32_t y0;
	int i;
	int n = 0;

	if (x < SOFM_EXP_FIXED_INPUT_MIN)
		return 0;

	if (x > SOFM_EXP_FIXED_INPUT_MAX)
		return INT32_MAX;

	/* x is Q5.27 */
	xs = x;
	while (xs >= SOFM_EXP_TWO_Q27 || xs <= SOFM_EXP_MINUS_TWO_Q27) {
		xs >>= 1;
		n++;
	}

	/* sofm_exp_int32() input is Q4.28, while x1 is Q5.27
	 * sofm_exp_int32() output is Q9.23, while y0 is Q12.20
	 */
	y0 = exp_hifi_q_shift_rnd(sofm_exp_int32(exp_hifi_q_shift_left(xs, 27, 28)),
				  23, 20);
	y = SOFM_EXP_ONE_Q20;
	for (i = 0; i < (1 << n); i++)
		y = (int32_t)exp_hifi_q_multsr_32x32((int64_t)y, y0, 20, 20, 20);

	return y;
}
EXPORT_SYMBOL(sofm_exp_fixed);

#endif
