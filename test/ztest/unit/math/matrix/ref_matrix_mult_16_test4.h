/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/* Created 31-Aug-2022 13:19:22 with script ref_matrix.m v1.9-rc1-1687-g6be4f7333-dirty */

#define MATRIX_MULT_16_TEST4_ELEMENTWISE  0
#define MATRIX_MULT_16_TEST4_A_ROWS  6
#define MATRIX_MULT_16_TEST4_A_COLUMNS  1
#define MATRIX_MULT_16_TEST4_B_ROWS  1
#define MATRIX_MULT_16_TEST4_B_COLUMNS  8
#define MATRIX_MULT_16_TEST4_C_ROWS  6
#define MATRIX_MULT_16_TEST4_C_COLUMNS  8
#define MATRIX_MULT_16_TEST4_A_QXY_Y  15
#define MATRIX_MULT_16_TEST4_B_QXY_Y  15
#define MATRIX_MULT_16_TEST4_C_QXY_Y  15

static const int16_t matrix_mult_16_test4_a[6] = {
	  1885,
	 -7669,
	 20092,
	 28297,
	  5628,
	-30238,
};

static const int16_t matrix_mult_16_test4_b[8] = {
	  1932, -16113,  10393,  11618,   -354,  -1301,   6828,  16450,
};

static const int16_t matrix_mult_16_test4_c[48] = {
	   111,   -927,    598,    668,    -20,    -75,    393,    946,
	  -452,   3771,  -2432,  -2719,     83,    304,  -1598,  -3850,
	  1185,  -9880,   6373,   7124,   -217,   -798,   4187,  10086,
	  1668, -13914,   8975,  10033,   -306,  -1123,   5896,  14205,
	   332,  -2767,   1785,   1995,    -61,   -223,   1173,   2825,
	 -1783,  14869,  -9591, -10721,    327,   1201,  -6301, -15180,
};
