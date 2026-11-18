/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/* Created 31-Aug-2022 13:19:22 with script ref_matrix.m v1.9-rc1-1687-g6be4f7333-dirty */

#define MATRIX_MULT_16_TEST1_ELEMENTWISE  0
#define MATRIX_MULT_16_TEST1_A_ROWS  3
#define MATRIX_MULT_16_TEST1_A_COLUMNS  4
#define MATRIX_MULT_16_TEST1_B_ROWS  4
#define MATRIX_MULT_16_TEST1_B_COLUMNS  5
#define MATRIX_MULT_16_TEST1_C_ROWS  3
#define MATRIX_MULT_16_TEST1_C_COLUMNS  5
#define MATRIX_MULT_16_TEST1_A_QXY_Y  15
#define MATRIX_MULT_16_TEST1_B_QXY_Y  15
#define MATRIX_MULT_16_TEST1_C_QXY_Y  14

static const int16_t matrix_mult_16_test1_a[12] = {
	 19277,  -6161, -12335, -26818,
	 10310,  27140,  19845, -10544,
	-23131,   -404,  21417, -17457,
};

static const int16_t matrix_mult_16_test1_b[20] = {
	 -9743, -26470, -14323,  15093,  28656,
	-23012,  -7262,  20498,  -8560,   9035,
	-32365,   4482,   5131,  30363,    417,
	 -3825,   9178,  13964,   9555, -30031,
};

static const int16_t matrix_mult_16_test1_c[15] = {
	  6954, -11703, -12820,  -4381,  19790,
	-20248,  -7291,   5543,   6486,  13208,
	 -5977,   8407,   2886,   2103,  -2034,
};
