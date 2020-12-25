// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>

//	drc_lin2dB_fixed.c :
//	warning disabled to support file system
//
#pragma warning (disable : 4996)
#pragma warning (disable : 4013)
#include <stdio.h>
#include "stdint.h"
#include "math.h"
#include "typdef.h"
#include "norm.h"
#include "testvector.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>




/*
 * function[y]= input_data_fixpt(x)
 * Arguments    : const int32_t x[TEST_VECTOR]
 *                int32_t y[TEST_VECTOR]
 * Return Type  : void
 */
void input_data_fixpt(const int32_t x[TEST_VECTOR], int32_t y[TEST_VECTOR])
{
    memcpy(&y[0], &x[0], 639U * (sizeof(int32_t)));
}

/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 30.1030); regulated to Q11.21: (-1024.0, 1024.0)
 */

inline int32_t drc_lin2db_fixed(int32_t linear, uint16_t idx, FILE *fd)		/* Data logging purpose*/
{
	int32_t log10_linear;
	/* For negative or zero, just return a very small dB value. */
	if (linear <= 0) {
		fprintf(fd, " %13.8f \n", -0.00047684);
		return Q_CONVERT_FLOAT(-1000.0f, 21);
	}	else {
		log10_linear = log10_fixed(linear); /* Q6.26 */
		log10_linear_log[idx] = q_mult(20, log10_linear, 0, 26, 21);		/* Dbg buffer*/
		fprintf(fd, " %13li\n", q_mult(20, log10_linear, 0, 26, 21));
		return q_mult(20, log10_linear, 0, 26, 21);	/*Q11.21*/ /*-67108866/2^21 = 32.0dB*/
	}
}
/*
 * Input depends on precision_x
 * Output range [0.5, 1); regulated to Q2.30
 */
static inline int32_t rexp_fixed(int32_t x, int precision_x, int *e)
{
	int bit = 31 - norm_int32(x);

	*e = bit - precision_x;

	if (bit > 30)
		return Q_SHIFT_RND(x, bit, 30);
	if (bit < 30)
		return Q_SHIFT_LEFT(x, bit, 30);
	return x;
}
/*
 * Input is Q6.26: max 32.0
 * Output range ~ (-inf, 1.505); regulated to Q6.26: (-32.0, 32.0)
 */
static inline int32_t log10_fixed(int32_t x)
{
#define QC 26		/* Coefficients obtained from:
					 * fpminimax(log10(x), 5, [|SG...|], [1/2;sqrt(2)/2], absolute);
					 * max err ~= 6.088e-8
					 */
	const int32_t ONE_OVER_SQRT2 = Q_CONVERT_FLOAT(0.70710678118654752f, 30); /* 1/sqrt(2) */
	const int32_t A5 = Q_CONVERT_FLOAT(1.131880283355712890625f, QC);
	const int32_t A4 = Q_CONVERT_FLOAT(-4.258677959442138671875f, QC);
	const int32_t A3 = Q_CONVERT_FLOAT(6.81631565093994140625f, QC);
	const int32_t A2 = Q_CONVERT_FLOAT(-6.1185703277587890625f, QC);
	const int32_t A1 = Q_CONVERT_FLOAT(3.6505267620086669921875f, QC);
	const int32_t A0 = Q_CONVERT_FLOAT(-1.217894077301025390625f, QC);
	const int32_t LOG10_2 = Q_CONVERT_FLOAT(0.301029995663981195214f, QC);
	int32_t e;
	int32_t exp; /* Q31.1 */
	int32_t x2, x4; /* Q2.30 */
	int32_t A5Xx, A3Xx;

	x = rexp_fixed(x, 26, &e); /* Q2.30 */
	exp = (int32_t)e << 1; /* Q_CONVERT_FLOAT(e, 1) */

	if (x > ONE_OVER_SQRT2) {
		x = q_mult(x, ONE_OVER_SQRT2, 30, 30, 30);
		exp += 1; /* Q_CONVERT_FLOAT(0.5, 1) */
	}

	x2 = q_mult(x, x, 30, 30, 30);
	x4 = q_mult(x2, x2, 30, 30, 30);
	A5Xx = q_mult(A5, x, QC, 30, QC);
	A3Xx = q_mult(A3, x, QC, 30, QC);
	return q_mult((A5Xx + A4), x4, QC, 30, QC) + q_mult((A3Xx + A2), x2, QC, 30, QC)
		+ q_mult(A1, x, QC, 30, QC) + A0 + q_mult(exp, LOG10_2, 1, QC, QC);
#undef QC
}


int main(void)
{
	int32_t l_testvector, x[TEST_VECTOR], y[TEST_VECTOR]; 	/*Increase/decrease length as per need*/
	uint16_t idx = 0;
	int i;
	/*int mkdir(const char* dirname);*/
	mkdir("Results", 0777);
	FILE *fd = fopen("Results/mag2dB.txt", "w");
	fprintf(fd, " %10s  %10s %10s \n", "idx", "testvector", "Fixlog10linear");
	data_initialization_fixpt(x);
	input_data_fixpt(x, y);
	for (i = 0; i < TEST_VECTOR; i++)	{
		l_testvector = y[i];
		/* Data logging purpose_start*/
		idx++;
		fprintf(fd, " %10d %11li ", idx, l_testvector);
		drc_lin2db_fixed(l_testvector, idx, fd);
		/* Data logging purpose*/
	}
	fclose(fd);
}
