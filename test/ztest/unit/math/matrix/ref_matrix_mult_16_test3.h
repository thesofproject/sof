/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/* Created 31-Aug-2022 13:19:22 with script ref_matrix.m v1.9-rc1-1687-g6be4f7333-dirty */

#define MATRIX_MULT_16_TEST3_ELEMENTWISE  1
#define MATRIX_MULT_16_TEST3_A_ROWS  5
#define MATRIX_MULT_16_TEST3_A_COLUMNS  6
#define MATRIX_MULT_16_TEST3_B_ROWS  5
#define MATRIX_MULT_16_TEST3_B_COLUMNS  6
#define MATRIX_MULT_16_TEST3_C_ROWS  5
#define MATRIX_MULT_16_TEST3_C_COLUMNS  6
#define MATRIX_MULT_16_TEST3_A_QXY_Y  15
#define MATRIX_MULT_16_TEST3_B_QXY_Y  15
#define MATRIX_MULT_16_TEST3_C_QXY_Y  15

static const int16_t matrix_mult_16_test3_a[30] = {
	 23663, -18301,   5145,  24885,  28067,   7557,
	  5068, -23957,  -4802,  18506, -17209,  22372,
	-23746, -15777,  -3153,   6782,   3516, -18088,
	 -3783,  -9536, -22760,  21341,  -5638,   2116,
	 30943, -30206, -29372,  25730,  25178, -29591,
};

static const int16_t matrix_mult_16_test3_b[30] = {
	 23845,  24708, -29283,  24416,  27463,  18332,
	 -4428, -16617, -12608,  21807, -30988,  -1056,
	  9420,  13385, -15170, -21833,  21322, -28217,
	-14987,  15556,   7216, -15191,   4158,  11965,
	-23855, -18550, -10367,   1097,  23384,  -8219,
};

static const int16_t matrix_mult_16_test3_c[30] = {
	 17219, -13799,  -4598,  18542,  23523,   4228,
	  -685,  12149,   1848,  12316,  16274,   -721,
	 -6826,  -6445,   1460,  -4519,   2288,  15576,
	  1730,  -4527,  -5012,  -9894,   -715,    773,
	-22526,  17100,   9293,    861,  17968,   7422,
};
