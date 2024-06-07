/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

static const int32_t src_int32_5_6_4354_5000_fir[380] = {
	-110765,
	202615,
	-181213,
	-60411,
	501917,
	-921634,
	953432,
	-299021,
	-994344,
	2375699,
	-2930341,
	1853065,
	938266,
	-4421028,
	6675774,
	-5774708,
	1018501,
	6157586,
	-12304414,
	13377047,
	-6992399,
	-5591173,
	19053336,
	-25865264,
	19988179,
	-798577,
	-24932443,
	44636431,
	-45182001,
	19866113,
	26046873,
	-74343877,
	98819194,
	-74821263,
	-10514225,
	152667682,
	-335622766,
	582806597,
	1554862275,
	262253212,
	-280549671,
	194075077,
	-79589975,
	-18519170,
	74070145,
	-81274868,
	52259402,
	-9027332,
	-26768671,
	42009053,
	-35327860,
	14935758,
	7047191,
	-20678805,
	22006533,
	-13416074,
	1099419,
	8600165,
	-12066006,
	9371949,
	-3330588,
	-2481991,
	5557309,
	-5318073,
	2851946,
	63634,
	-1989169,
	2393574,
	-1619886,
	439526,
	460670,
	-781233,
	616459,
	-258714,
	-28531,
	136998,
	-84272,
	227519,
	-313644,
	183996,
	244422,
	-848023,
	1264573,
	-1041170,
	-62356,
	1773109,
	-3236736,
	3338270,
	-1353763,
	-2384260,
	6258494,
	-7899350,
	5411268,
	1253179,
	-9597597,
	15229439,
	-13885351,
	4029762,
	11363736,
	-25148377,
	28871533,
	-17231281,
	-7971064,
	36720596,
	-53867037,
	45807278,
	-8187315,
	-48973765,
	101648548,
	-118211915,
	69147397,
	66618723,
	-316432297,
	912796765,
	1434247379,
	-10835542,
	-175714416,
	187947221,
	-124187428,
	37194100,
	34538116,
	-69457064,
	65060050,
	-34187001,
	-3335742,
	30011856,
	-37157358,
	26443067,
	-6800413,
	-11082012,
	19809927,
	-17768431,
	8487264,
	2149878,
	-9109682,
	10269995,
	-6653311,
	1157507,
	3259408,
	-4952125,
	3982226,
	-1602490,
	-676653,
	1872174,
	-1825398,
	1008386,
	-84144,
	-482090,
	579489,
	-372305,
	106682,
	52972,
	-26669,
	196411,
	-387115,
	417986,
	-109422,
	-552896,
	1295790,
	-1602646,
	975263,
	680802,
	-2778428,
	4131268,
	-3494980,
	381983,
	4297017,
	-8261457,
	8789295,
	-4266521,
	-4414489,
	13534842,
	-17852419,
	13241572,
	511288,
	-18365939,
	31340812,
	-30467503,
	11871794,
	19540469,
	-50157374,
	62395995,
	-42778263,
	-9896067,
	80067707,
	-136995981,
	141425750,
	-50460371,
	-208459562,
	1209907046,
	1209907046,
	-208459562,
	-50460371,
	141425750,
	-136995981,
	80067707,
	-9896067,
	-42778263,
	62395995,
	-50157374,
	19540469,
	11871794,
	-30467503,
	31340812,
	-18365939,
	511288,
	13241572,
	-17852419,
	13534842,
	-4414489,
	-4266521,
	8789295,
	-8261457,
	4297017,
	381983,
	-3494980,
	4131268,
	-2778428,
	680802,
	975263,
	-1602646,
	1295790,
	-552896,
	-109422,
	417986,
	-387115,
	196411,
	-26669,
	52972,
	106682,
	-372305,
	579489,
	-482090,
	-84144,
	1008386,
	-1825398,
	1872174,
	-676653,
	-1602490,
	3982226,
	-4952125,
	3259408,
	1157507,
	-6653311,
	10269995,
	-9109682,
	2149878,
	8487264,
	-17768431,
	19809927,
	-11082012,
	-6800413,
	26443067,
	-37157358,
	30011856,
	-3335742,
	-34187001,
	65060050,
	-69457064,
	34538116,
	37194100,
	-124187428,
	187947221,
	-175714416,
	-10835542,
	1434247379,
	912796765,
	-316432297,
	66618723,
	69147397,
	-118211915,
	101648548,
	-48973765,
	-8187315,
	45807278,
	-53867037,
	36720596,
	-7971064,
	-17231281,
	28871533,
	-25148377,
	11363736,
	4029762,
	-13885351,
	15229439,
	-9597597,
	1253179,
	5411268,
	-7899350,
	6258494,
	-2384260,
	-1353763,
	3338270,
	-3236736,
	1773109,
	-62356,
	-1041170,
	1264573,
	-848023,
	244422,
	183996,
	-313644,
	227519,
	-84272,
	136998,
	-28531,
	-258714,
	616459,
	-781233,
	460670,
	439526,
	-1619886,
	2393574,
	-1989169,
	63634,
	2851946,
	-5318073,
	5557309,
	-2481991,
	-3330588,
	9371949,
	-12066006,
	8600165,
	1099419,
	-13416074,
	22006533,
	-20678805,
	7047191,
	14935758,
	-35327860,
	42009053,
	-26768671,
	-9027332,
	52259402,
	-81274868,
	74070145,
	-18519170,
	-79589975,
	194075077,
	-280549671,
	262253212,
	1554862275,
	582806597,
	-335622766,
	152667682,
	-10514225,
	-74821263,
	98819194,
	-74343877,
	26046873,
	19866113,
	-45182001,
	44636431,
	-24932443,
	-798577,
	19988179,
	-25865264,
	19053336,
	-5591173,
	-6992399,
	13377047,
	-12304414,
	6157586,
	1018501,
	-5774708,
	6675774,
	-4421028,
	938266,
	1853065,
	-2930341,
	2375699,
	-994344,
	-299021,
	953432,
	-921634,
	501917,
	-60411,
	-181213,
	202615,
	-110765

};

static const struct src_stage src_int32_5_6_4354_5000 = {
	1, 1, 5, 76, 380, 6, 5, 0, 0,
	src_int32_5_6_4354_5000_fir};
