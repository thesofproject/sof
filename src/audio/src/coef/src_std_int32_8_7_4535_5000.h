/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

__cold_rodata static const int32_t src_int32_8_7_4535_5000_fir[640] = {
	-93442,
	125750,
	-138530,
	116746,
	-43044,
	-101212,
	334492,
	-673307,
	1130107,
	-1711012,
	2413497,
	-3224193,
	4117001,
	-5051674,
	5973063,
	-6811159,
	7482045,
	-7889824,
	7929516,
	-7490867,
	6462930,
	-4739228,
	2223229,
	1166168,
	-5489507,
	10782375,
	-17052137,
	24276232,
	-32402484,
	41351981,
	-51025278,
	61313219,
	-72114819,
	83367552,
	-95102678,
	107559185,
	-121458671,
	138823375,
	-166299384,
	239693620,
	1906322077,
	-9155468,
	-48066112,
	66316974,
	-73372729,
	75158695,
	-73800022,
	70331439,
	-65384437,
	59413677,
	-52783069,
	45800568,
	-38731928,
	31805359,
	-25212372,
	19107391,
	-13607426,
	8792492,
	-4707074,
	1362701,
	1258444,
	-3199055,
	4521635,
	-5303103,
	5629354,
	-5590070,
	5273997,
	-4764909,
	4138376,
	-3459432,
	2781152,
	-2144104,
	1576595,
	-1095574,
	708054,
	-412876,
	202662,
	-65790,
	-11742,
	44663,
	-132653,
	197980,
	-257939,
	298508,
	-301891,
	247105,
	-111018,
	-130148,
	498877,
	-1014055,
	1688395,
	-2525745,
	3518433,
	-4644847,
	5867453,
	-7131421,
	8364048,
	-9475101,
	10358167,
	-10893025,
	10948993,
	-10389113,
	9074969,
	-6871821,
	3653688,
	692113,
	-6261556,
	13132355,
	-21365269,
	31009674,
	-42115701,
	54757023,
	-69072344,
	85342903,
	-104147133,
	126702130,
	-155732115,
	198172545,
	-276796278,
	527626477,
	1819577413,
	-204301038,
	62632189,
	-8995514,
	-18631993,
	34478025,
	-43632343,
	48430179,
	-50153312,
	49615358,
	-47397405,
	43952580,
	-39654929,
	34822532,
	-29728081,
	24603352,
	-19640909,
	14994852,
	-10781648,
	7081550,
	-3940876,
	1375167,
	626850,
	-2098677,
	3091078,
	-3666967,
	3896424,
	-3852056,
	3604922,
	-3221139,
	2759303,
	-2268728,
	1788516,
	-1347378,
	964118,
	-648631,
	403293,
	-224566,
	104689,
	-33319,
	-157401,
	249355,
	-351579,
	452951,
	-537563,
	584769,
	-569671,
	464121,
	-238246,
	-137518,
	689930,
	-1440088,
	2400356,
	-3571235,
	4938388,
	-6470013,
	8114782,
	-9800532,
	11433856,
	-12900711,
	14068057,
	-14786499,
	14893780,
	-14218878,
	12586331,
	-9820284,
	5747523,
	-198546,
	-6994767,
	16007069,
	-27036704,
	40338832,
	-56287799,
	75497698,
	-99070470,
	129159125,
	-170440817,
	234865238,
	-363259237,
	835404953,
	1653204873,
	-336879526,
	153079914,
	-77234854,
	35196411,
	-8682787,
	-9007956,
	20945903,
	-28778067,
	33510715,
	-35832580,
	36262449,
	-35220704,
	33064806,
	-30106580,
	26620047,
	-22844382,
	18984585,
	-15211356,
	11661068,
	-8436324,
	5607361,
	-3214353,
	1270562,
	233819,
	-1327322,
	2052619,
	-2461913,
	2612553,
	-2563062,
	2369743,
	-2083970,
	1750225,
	-1404888,
	1075730,
	-782049,
	535321,
	-340250,
	196063,
	-97941,
	-161676,
	269707,
	-403118,
	555269,
	-714319,
	862702,
	-976996,
	1028269,
	-982981,
	804478,
	-455075,
	-101301,
	896012,
	-1952277,
	3281574,
	-4880094,
	6725437,
	-8773807,
	10957871,
	-13185496,
	15339450,
	-17278127,
	18837243,
	-19832342,
	20061790,
	-19309771,
	17348537,
	-13938827,
	8826736,
	-1734368,
	-7660419,
	19763765,
	-35142385,
	54674937,
	-79870317,
	113606237,
	-162110074,
	241570301,
	-410115501,
	1140827794,
	1420741782,
	-404301345,
	214269566,
	-130169088,
	81125199,
	-48496846,
	25274569,
	-8231184,
	-4345798,
	13478677,
	-19847633,
	23953028,
	-26195886,
	26918835,
	-26426583,
	24995354,
	-22876290,
	20295681,
	-17453751,
	14523076,
	-11647299,
	8940551,
	-6487778,
	4346040,
	-2546721,
	1098526,
	8926,
	-801135,
	1314612,
	-1592849,
	1682583,
	-1630502,
	1480527,
	-1271747,
	1037023,
	-802247,
	586201,
	-400924,
	252466,
	-141924,
	-141924,
	252466,
	-400924,
	586201,
	-802247,
	1037023,
	-1271747,
	1480527,
	-1630502,
	1682583,
	-1592849,
	1314612,
	-801135,
	8926,
	1098526,
	-2546721,
	4346040,
	-6487778,
	8940551,
	-11647299,
	14523076,
	-17453751,
	20295681,
	-22876290,
	24995354,
	-26426583,
	26918835,
	-26195886,
	23953028,
	-19847633,
	13478677,
	-4345798,
	-8231184,
	25274569,
	-48496846,
	81125199,
	-130169088,
	214269566,
	-404301345,
	1420741782,
	1140827794,
	-410115501,
	241570301,
	-162110074,
	113606237,
	-79870317,
	54674937,
	-35142385,
	19763765,
	-7660419,
	-1734368,
	8826736,
	-13938827,
	17348537,
	-19309771,
	20061790,
	-19832342,
	18837243,
	-17278127,
	15339450,
	-13185496,
	10957871,
	-8773807,
	6725437,
	-4880094,
	3281574,
	-1952277,
	896012,
	-101301,
	-455075,
	804478,
	-982981,
	1028269,
	-976996,
	862702,
	-714319,
	555269,
	-403118,
	269707,
	-161676,
	-97941,
	196063,
	-340250,
	535321,
	-782049,
	1075730,
	-1404888,
	1750225,
	-2083970,
	2369743,
	-2563062,
	2612553,
	-2461913,
	2052619,
	-1327322,
	233819,
	1270562,
	-3214353,
	5607361,
	-8436324,
	11661068,
	-15211356,
	18984585,
	-22844382,
	26620047,
	-30106580,
	33064806,
	-35220704,
	36262449,
	-35832580,
	33510715,
	-28778067,
	20945903,
	-9007956,
	-8682787,
	35196411,
	-77234854,
	153079914,
	-336879526,
	1653204873,
	835404953,
	-363259237,
	234865238,
	-170440817,
	129159125,
	-99070470,
	75497698,
	-56287799,
	40338832,
	-27036704,
	16007069,
	-6994767,
	-198546,
	5747523,
	-9820284,
	12586331,
	-14218878,
	14893780,
	-14786499,
	14068057,
	-12900711,
	11433856,
	-9800532,
	8114782,
	-6470013,
	4938388,
	-3571235,
	2400356,
	-1440088,
	689930,
	-137518,
	-238246,
	464121,
	-569671,
	584769,
	-537563,
	452951,
	-351579,
	249355,
	-157401,
	-33319,
	104689,
	-224566,
	403293,
	-648631,
	964118,
	-1347378,
	1788516,
	-2268728,
	2759303,
	-3221139,
	3604922,
	-3852056,
	3896424,
	-3666967,
	3091078,
	-2098677,
	626850,
	1375167,
	-3940876,
	7081550,
	-10781648,
	14994852,
	-19640909,
	24603352,
	-29728081,
	34822532,
	-39654929,
	43952580,
	-47397405,
	49615358,
	-50153312,
	48430179,
	-43632343,
	34478025,
	-18631993,
	-8995514,
	62632189,
	-204301038,
	1819577413,
	527626477,
	-276796278,
	198172545,
	-155732115,
	126702130,
	-104147133,
	85342903,
	-69072344,
	54757023,
	-42115701,
	31009674,
	-21365269,
	13132355,
	-6261556,
	692113,
	3653688,
	-6871821,
	9074969,
	-10389113,
	10948993,
	-10893025,
	10358167,
	-9475101,
	8364048,
	-7131421,
	5867453,
	-4644847,
	3518433,
	-2525745,
	1688395,
	-1014055,
	498877,
	-130148,
	-111018,
	247105,
	-301891,
	298508,
	-257939,
	197980,
	-132653,
	44663,
	-11742,
	-65790,
	202662,
	-412876,
	708054,
	-1095574,
	1576595,
	-2144104,
	2781152,
	-3459432,
	4138376,
	-4764909,
	5273997,
	-5590070,
	5629354,
	-5303103,
	4521635,
	-3199055,
	1258444,
	1362701,
	-4707074,
	8792492,
	-13607426,
	19107391,
	-25212372,
	31805359,
	-38731928,
	45800568,
	-52783069,
	59413677,
	-65384437,
	70331439,
	-73800022,
	75158695,
	-73372729,
	66316974,
	-48066112,
	-9155468,
	1906322077,
	239693620,
	-166299384,
	138823375,
	-121458671,
	107559185,
	-95102678,
	83367552,
	-72114819,
	61313219,
	-51025278,
	41351981,
	-32402484,
	24276232,
	-17052137,
	10782375,
	-5489507,
	1166168,
	2223229,
	-4739228,
	6462930,
	-7490867,
	7929516,
	-7889824,
	7482045,
	-6811159,
	5973063,
	-5051674,
	4117001,
	-3224193,
	2413497,
	-1711012,
	1130107,
	-673307,
	334492,
	-101212,
	-43044,
	116746,
	-138530,
	125750,
	-93442

};

static const struct src_stage src_int32_8_7_4535_5000 = {
	6, 7, 8, 80, 640, 7, 8, 0, 0,
	src_int32_8_7_4535_5000_fir};
