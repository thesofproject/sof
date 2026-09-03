/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/* Created 31-Aug-2022 13:19:22 with script ref_matrix.m v1.9-rc1-1687-g6be4f7333-dirty */

#define MATRIX_MULT_16_TEST2_ELEMENTWISE  0
#define MATRIX_MULT_16_TEST2_A_ROWS  6
#define MATRIX_MULT_16_TEST2_A_COLUMNS  7
#define MATRIX_MULT_16_TEST2_B_ROWS  7
#define MATRIX_MULT_16_TEST2_B_COLUMNS  8
#define MATRIX_MULT_16_TEST2_C_ROWS  6
#define MATRIX_MULT_16_TEST2_C_COLUMNS  8
#define MATRIX_MULT_16_TEST2_A_QXY_Y  0
#define MATRIX_MULT_16_TEST2_B_QXY_Y  0
#define MATRIX_MULT_16_TEST2_C_QXY_Y  0

static const int16_t matrix_mult_16_test2_a[42] = {
	    46,     -7,    -40,     15,    -11,    -26,     36,
	   -30,     16,     27,     49,     41,      1,     20,
	   -62,    -34,      1,     17,     31,     56,    -37,
	    61,    -57,     57,    -36,    -60,    -32,    -20,
	    -1,     12,    -54,     44,    -29,     57,     37,
	    -1,     -4,     41,     17,    -61,    -53,    -28,
};

static const int16_t matrix_mult_16_test2_b[56] = {
	   -49,     12,     19,     38,    -43,     58,    -46,    -34,
	    15,     38,    -60,    -15,     10,    -30,     49,    -46,
	   -54,    -29,     34,     14,     57,     16,     23,     23,
	    61,     24,     33,     56,     30,     19,    -39,    -45,
	   -53,     15,     49,     24,     56,     -9,    -40,     30,
	    61,    -19,     51,    -62,     10,     24,      4,    -19,
	   -31,    -63,     41,    -41,     40,     38,    -13,     27,
};

static const int16_t matrix_mult_16_test2_c[48] = {
	 -1403,   -133,     40,   2005,  -3314,   3366,  -4096,  -1701,
	   509,    -23,   3885,   1844,   7565,   -442,  -1022,    451,
	  6431,     75,   4315,  -2091,   3709,  -2578,     11,   1797,
	 -7270,  -2983,    -63,   3319,  -5504,   4488,   -352,   1747,
	  9696,   -783,   1880,  -4257,  -1169,   2589,  -1417,  -4694,
	  -320,    911,  -4664,   4518,  -2216,   -746,   2722,  -1183,
};
