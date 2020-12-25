// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
/* Include Files */
#pragma warning (disable : 4996)
#pragma warning (disable : 4013)
#include <stdio.h>
#include <stdlib.h>
#include "typedef.h"
void init_struc_fixpt(int32_t x[21]);

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: [-1.0, 1.0]; regulated to Q2.30: (-2.0, 2.0)
 */
static inline int32_t drc_asin_fixed(int32_t x, int8_t i, FILE *fd)
{
#define qcl 30
#define qch 26
	/* Coefficients obtained from:
	 * If x <= 1/sqrt(2), then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [-1e-30;1/sqrt(2)], absolute)
	 *   max err ~= 1.89936e-5
	 * Else then
	 *   fpminimax(asin(x), [|1,3,5,7|], [|SG...|], [1/sqrt(2);1], absolute)
	 *   max err ~= 3.085226e-2
	 */
	const int32_t TWO_OVER_PI = Q_CONVERT_FLOAT(0.63661977236758134f, qcl); /* 2/pi */
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(0.70710678118654752f, qcl); /* 1/sqrt(2) */
	const int32_t A7L = Q_CONVERT_FLOAT(0.1181826665997505187988281f, qcl);
	const int32_t A5L = Q_CONVERT_FLOAT(4.0224377065896987915039062e-2f, qcl);
	const int32_t A3L = Q_CONVERT_FLOAT(0.1721895635128021240234375f, qcl);
	const int32_t A1L = Q_CONVERT_FLOAT(0.99977016448974609375f, qcl);

	const int32_t A7H = Q_CONVERT_FLOAT(14.12774658203125f, qch);
	const int32_t A5H = Q_CONVERT_FLOAT(-30.1692714691162109375f, qch);
	const int32_t A3H = Q_CONVERT_FLOAT(21.4760608673095703125f, qch);
	const int32_t A1H = Q_CONVERT_FLOAT(-3.894591808319091796875f, qch);

	int32_t A7, A5, A3, A1, qc;
	int32_t x2, x4;
	int32_t A3Xx2, A7Xx2, asinx;

	/*if (ABS(x) <= ONE_OVER_SQRT2) {*/
	if (abs(x) <= ONE_OVER_SQRT2) {				/*ATTENTION - replace ABS with abs-Need to check*/
		A7 = A7L;
		A5 = A5L;
		A3 = A3L;
		A1 = A1L;
		qc = qcl;
	}	else {
		A7 = A7H;
		A5 = A5H;
		A3 = A3H;
		A1 = A1H;
		qc = qch;
		x = Q_SHIFT_RND(x, qcl, qch); /* Q6.26 */
	}

	x2 = q_multq(x, x, qc);
	x4 = q_multq(x2, x2, qc);

	A3Xx2 = q_multq(A3, x2, qc);
	A7Xx2 = q_multq(A7, x2, qc);

	asinx = q_multq(x, (q_multq(x4, (A7Xx2 + A5), qc) + A3Xx2 + A1), qc);	/*Warning - Arithmatic overflow,+ on 4byte value and casting the result to a 8 byte value*/
	fprintf(fd, " %13li\n", q_mult(asinx, TWO_OVER_PI, qc, qcl, 30));
	return q_mult(asinx, TWO_OVER_PI, qc, qcl, 30);
#undef qch
#undef qcl
}


/*
 * function x = init_struc_fixpt
 * Q2.30 %asin: [-1.0, 1.0]; y = drc_asin_fixed(x) = 2/pi * asin(x)
 * Arguments    : int32_t x[21] Q2.30
 * Return Type  : void
 */
void init_struc_fixpt(int32_t x[21])
{
	static const int32_t iv[21] = { -1073741824, -966367642, -858993459,
	  -751619277, -644245094, -536870912, -429496730, -322122547, -214748365,
	  -107374182, 0, 107374182, 214748365, 322122547, 429496730, 536870912,
	  644245094, 751619277, 858993459, 966367642, 1073741824 };

	memcpy(&x[0], &iv[0], 21U * (sizeof(int32_t)));

	/* x = fi([-1:0.1:1],1,32,30); */
}

/*
 * Input is Q2.30: (-2.0, 2.0)
 * Output range: (-1.0, 1.0); regulated to Q1.31: (-1.0, 1.0)
 */
int main(void)
{
	int32_t x[TEST_VECTOR];
	int8_t i;
	mkdir("Results", 0777);
	FILE *fd = fopen("Results/drc_asin_fixed.txt", "w");
	fprintf(fd, " %10s  %10s %13s \n", "idx", "in-asine", "out-asine");
	init_struc_fixpt(x);
	for (i = 0; i < TEST_VECTOR; i++) {
		fprintf(fd, " %10d %11li ", i, x[i]);
		drc_asin_fixed(x[i], i, fd);
	}
	fclose(fd);
}