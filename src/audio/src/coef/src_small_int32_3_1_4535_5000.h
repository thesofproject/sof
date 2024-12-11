/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 */

#include <stdint.h>

__cold_rodata static const int32_t src_int32_3_1_4535_5000_fir[276] = {
	-92545,
	140559,
	-191923,
	241092,
	-280468,
	300406,
	-289386,
	234365,
	-121331,
	-63945,
	334966,
	-703402,
	1177853,
	-1762541,
	2455986,
	-3249742,
	4127275,
	-5063054,
	6021946,
	-6958968,
	7819459,
	-8539714,
	9048086,
	-9266554,
	9112707,
	-8502087,
	7350785,
	-5578150,
	3109467,
	121606,
	-4171063,
	9083324,
	-14891080,
	21616687,
	-29275749,
	37883927,
	-47468754,
	58089770,
	-69873625,
	83078597,
	-98222819,
	116367438,
	-139836570,
	174452167,
	-239067307,
	443704770,
	1745131644,
	-175012245,
	51975026,
	-6063930,
	-17509776,
	31135622,
	-39202118,
	43694778,
	-45679141,
	45810673,
	-44541243,
	42212222,
	-39099402,
	35435453,
	-31421182,
	27231019,
	-23015520,
	18902408,
	-14997036,
	11382768,
	-8121575,
	5254995,
	-2805512,
	778345,
	836405,
	-2061324,
	2929094,
	-3479822,
	3758420,
	-3812130,
	3688274,
	-3432310,
	3086244,
	-2687418,
	2267686,
	-1852975,
	1463188,
	-1112411,
	809377,
	-558113,
	358723,
	-208233,
	101443,
	-31750,
	-8119,
	25474,
	-80455,
	141487,
	-224401,
	330182,
	-457891,
	604136,
	-762591,
	923632,
	-1074098,
	1197260,
	-1273011,
	1278324,
	-1187981,
	975605,
	-614950,
	81459,
	645988,
	-1583179,
	2738604,
	-4111517,
	5690103,
	-7449859,
	9352271,
	-11343859,
	13355664,
	-15303188,
	17086793,
	-18592506,
	19693117,
	-20249392,
	20111130,
	-19117642,
	17097059,
	-13863515,
	9210750,
	-2899622,
	-5364913,
	15975785,
	-29502077,
	46841927,
	-69544115,
	100559342,
	-146278769,
	223461540,
	-393862475,
	1214351617,
	1214351617,
	-393862475,
	223461540,
	-146278769,
	100559342,
	-69544115,
	46841927,
	-29502077,
	15975785,
	-5364913,
	-2899622,
	9210750,
	-13863515,
	17097059,
	-19117642,
	20111130,
	-20249392,
	19693117,
	-18592506,
	17086793,
	-15303188,
	13355664,
	-11343859,
	9352271,
	-7449859,
	5690103,
	-4111517,
	2738604,
	-1583179,
	645988,
	81459,
	-614950,
	975605,
	-1187981,
	1278324,
	-1273011,
	1197260,
	-1074098,
	923632,
	-762591,
	604136,
	-457891,
	330182,
	-224401,
	141487,
	-80455,
	25474,
	-8119,
	-31750,
	101443,
	-208233,
	358723,
	-558113,
	809377,
	-1112411,
	1463188,
	-1852975,
	2267686,
	-2687418,
	3086244,
	-3432310,
	3688274,
	-3812130,
	3758420,
	-3479822,
	2929094,
	-2061324,
	836405,
	778345,
	-2805512,
	5254995,
	-8121575,
	11382768,
	-14997036,
	18902408,
	-23015520,
	27231019,
	-31421182,
	35435453,
	-39099402,
	42212222,
	-44541243,
	45810673,
	-45679141,
	43694778,
	-39202118,
	31135622,
	-17509776,
	-6063930,
	51975026,
	-175012245,
	1745131644,
	443704770,
	-239067307,
	174452167,
	-139836570,
	116367438,
	-98222819,
	83078597,
	-69873625,
	58089770,
	-47468754,
	37883927,
	-29275749,
	21616687,
	-14891080,
	9083324,
	-4171063,
	121606,
	3109467,
	-5578150,
	7350785,
	-8502087,
	9112707,
	-9266554,
	9048086,
	-8539714,
	7819459,
	-6958968,
	6021946,
	-5063054,
	4127275,
	-3249742,
	2455986,
	-1762541,
	1177853,
	-703402,
	334966,
	-63945,
	-121331,
	234365,
	-289386,
	300406,
	-280468,
	241092,
	-191923,
	140559,
	-92545

};

static const struct src_stage src_int32_3_1_4535_5000 = {
	0, 1, 3, 92, 276, 1, 3, 0, 0,
	src_int32_3_1_4535_5000_fir};
