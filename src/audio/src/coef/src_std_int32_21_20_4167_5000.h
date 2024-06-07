/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

static const int32_t src_int32_21_20_4167_5000_fir[1092] = {
	-148365,
	253251,
	-300044,
	198018,
	165016,
	-904116,
	2109032,
	-3809131,
	5936407,
	-8293365,
	10533360,
	-12160474,
	12553979,
	-11019062,
	6861167,
	523301,
	-11551306,
	26366797,
	-44779009,
	66240300,
	-89877091,
	114589459,
	-139253201,
	163152569,
	-187327445,
	223249753,
	1846610288,
	130583077,
	-146550977,
	141645404,
	-128384601,
	110446135,
	-90107433,
	69197295,
	-49230987,
	31387478,
	-16470657,
	4896061,
	3285785,
	-8331721,
	10723485,
	-11073331,
	10031194,
	-8205040,
	6102582,
	-4098230,
	2424839,
	-1186255,
	384239,
	47534,
	-209322,
	206397,
	-166569,
	297924,
	-389487,
	351244,
	-65886,
	-594767,
	1742814,
	-3438876,
	5652714,
	-8226411,
	10847843,
	-13042081,
	14186734,
	-13554119,
	10378908,
	-3945006,
	-6319272,
	20752631,
	-39396566,
	61975727,
	-87948323,
	116670859,
	-147794258,
	182325160,
	-226613958,
	321135533,
	1835142568,
	43907150,
	-105054593,
	118266980,
	-115448664,
	104364730,
	-88671030,
	70818254,
	-52688233,
	35733161,
	-20992285,
	9094186,
	-279892,
	-5544994,
	8733380,
	-9805380,
	9355049,
	-7966914,
	6151241,
	-4303075,
	2685872,
	-1436800,
	588081,
	-97459,
	-119135,
	158426,
	-182704,
	339334,
	-475770,
	504292,
	-304392,
	-263274,
	1331662,
	-2992218,
	5253760,
	-8001533,
	10964194,
	-13696782,
	15586558,
	-15885789,
	13771400,
	-8426423,
	-867126,
	14639231,
	-33166657,
	56457111,
	-84320484,
	116601851,
	-153783204,
	198725893,
	-263628339,
	423384751,
	1812335900,
	-36098867,
	-63585259,
	93497987,
	-100735707,
	96501411,
	-85629375,
	71099825,
	-55106036,
	39336334,
	-25040115,
	13043342,
	-3770016,
	-2712550,
	6623954,
	-8384165,
	8521553,
	-7587128,
	6084582,
	-4422234,
	2888933,
	-1652111,
	773268,
	-234431,
	-31202,
	110367,
	-196221,
	376406,
	-556988,
	654120,
	-546125,
	84690,
	882096,
	-2475521,
	4744073,
	-7619042,
	10875497,
	-14107040,
	16721924,
	-17965764,
	16972057,
	-12836658,
	4706629,
	8132057,
	-26189820,
	49762567,
	-79024638,
	114331768,
	-157036133,
	211951991,
	-297587677,
	529067481,
	1778446098,
	-108855370,
	-22856546,
	67827790,
	-84560253,
	87041119,
	-81072038,
	70063328,
	-56459696,
	42144471,
	-28548276,
	16675229,
	-7121438,
	112182,
	4436862,
	-6839472,
	7549877,
	-7076264,
	5906903,
	-4455925,
	3032010,
	-1829335,
	937004,
	-361114,
	52885,
	63190,
	-206594,
	408096,
	-631244,
	797620,
	-786478,
	442941,
	401584,
	-1896682,
	4130433,
	-7082270,
	10578583,
	-14259577,
	17565651,
	-19749515,
	19916591,
	-17091120,
	10299303,
	1346008,
	-18582587,
	41993823,
	-72123658,
	109849580,
	-157414230,
	221643306,
	-327720020,
	637190454,
	1733852586,
	-173887893,
	16460928,
	41745825,
	-67254839,
	76193594,
	-75114450,
	67753885,
	-56744596,
	44120724,
	-31462127,
	19928738,
	-10274824,
	2877007,
	2214320,
	-5202728,
	6461321,
	-6447091,
	5624466,
	-4405963,
	3114286,
	-1966436,
	1077005,
	-475532,
	131688,
	17791,
	-213333,
	433415,
	-696695,
	931684,
	-1020701,
	804881,
	-101577,
	-1265023,
	3421790,
	-6397585,
	10074178,
	-14145721,
	18095511,
	-21197204,
	22544278,
	-21106513,
	15805872,
	-5596560,
	-10475609,
	33274801,
	-63711743,
	103185037,
	-154827509,
	227489980,
	-353276190,
	746709000,
	1679053311,
	-230831156,
	53748160,
	15733235,
	-49163659,
	64189024,
	-67895272,
	64239127,
	-55975907,
	45244301,
	-33739058,
	22750948,
	-13175685,
	5531904,
	-1690,
	-3506371,
	5278863,
	-5714267,
	5245328,
	-4275678,
	3136112,
	-2062208,
	1191526,
	-576030,
	203942,
	-25024,
	-215999,
	451454,
	-751588,
	1053259,
	-1243986,
	1163621,
	-618392,
	-591147,
	2629159,
	-5574342,
	9366983,
	-13761673,
	18294762,
	-22274546,
	24799176,
	-24802430,
	21120916,
	-12567987,
	-2011483,
	23749746,
	-53913345,
	94409091,
	-149237778,
	229239476,
	-373541538,
	856539835,
	1614658080,
	-279431514,
	88445423,
	-9745121,
	-30636165,
	51273443,
	-59573385,
	59607514,
	-54187936,
	45510558,
	-35349048,
	25097943,
	-15775292,
	8029917,
	-2170207,
	-1783226,
	4026693,
	-4894023,
	4779139,
	-4069814,
	3098967,
	-2116265,
	1279372,
	-661292,
	268575,
	-64550,
	-214216,
	461406,
	-794303,
	1159418,
	-1451564,
	1512104,
	-1139329,
	113226,
	1765456,
	-4624776,
	8465674,
	-13108687,
	18152577,
	-22953565,
	26631281,
	-28102936,
	26140600,
	-19437611,
	6657687,
	13580950,
	-42881510,
	83633578,
	-140660768,
	226702870,
	-387847468,
	965574536,
	1541380411,
	-319547822,
	120058811,
	-34248170,
	-12020719,
	37703923,
	-50324581,
	53966307,
	-51433125,
	44930776,
	-36274990,
	26935463,
	-18031467,
	10327964,
	-4252020,
	-65882,
	2729728,
	-4003815,
	4236922,
	-3794399,
	3005395,
	-2129028,
	1339905,
	-730354,
	324719,
	-100192,
	-207687,
	462594,
	-823393,
	1247420,
	-1638800,
	1843234,
	-1654484,
	835459,
	845301,
	-3563842,
	7382834,
	-12193161,
	17664378,
	-23213259,
	27997590,
	-30938080,
	30764655,
	-26074168,
	15372957,
	2946086,
	-30795665,
	71010149,
	-129167353,
	219760264,
	-395582527,
	1072693503,
	1460028025,
	-351150697,
	148165618,
	-57365516,
	6341573,
	23743694,
	-40338013,
	47439236,
	-47780750,
	43531654,
	-36512758,
	28239346,
	-19909241,
	12387561,
	-6210372,
	1613909,
	1413132,
	-3061969,
	3630827,
	-3456591,
	2858925,
	-2101682,
	1373031,
	-782605,
	371717,
	-131473,
	-196202,
	454488,
	-837618,
	1314775,
	-1801289,
	2150023,
	-2153762,
	1562212,
	-115216,
	-2408993,
	6134795,
	-11026626,
	16832058,
	-23040137,
	28863054,
	-33245339,
	34898342,
	-32348243,
	23971387,
	-7964791,
	-17858858,
	56728449,
	-114883829,
	208365225,
	-396202914,
	1176780210,
	1371492118,
	-374320235,
	172418462,
	-78723625,
	24122758,
	9657239,
	-29812468,
	40163908,
	-43315322,
	41354530,
	-36071041,
	28995789,
	-21381358,
	14175443,
	-8011590,
	3225807,
	101840,
	-2087313,
	2973875,
	-3064512,
	2663971,
	-2036136,
	1379185,
	-817788,
	409128,
	-158038,
	-179653,
	436725,
	-835989,
	1359301,
	-1934954,
	2425723,
	-2627070,
	2279687,
	-1098814,
	-1179906,
	4741422,
	-9625642,
	15664078,
	-22428631,
	29201397,
	-34970939,
	38454339,
	-38134741,
	32289015,
	-18952542,
	-4294518,
	41013562,
	-97991225,
	192548148,
	-389242219,
	1276735561,
	1276735561,
	-389242219,
	192548148,
	-97991225,
	41013562,
	-4294518,
	-18952542,
	32289015,
	-38134741,
	38454339,
	-34970939,
	29201397,
	-22428631,
	15664078,
	-9625642,
	4741422,
	-1179906,
	-1098814,
	2279687,
	-2627070,
	2425723,
	-1934954,
	1359301,
	-835989,
	436725,
	-179653,
	-158038,
	409128,
	-817788,
	1379185,
	-2036136,
	2663971,
	-3064512,
	2973875,
	-2087313,
	101840,
	3225807,
	-8011590,
	14175443,
	-21381358,
	28995789,
	-36071041,
	41354530,
	-43315322,
	40163908,
	-29812468,
	9657239,
	24122758,
	-78723625,
	172418462,
	-374320235,
	1371492118,
	1176780210,
	-396202914,
	208365225,
	-114883829,
	56728449,
	-17858858,
	-7964791,
	23971387,
	-32348243,
	34898342,
	-33245339,
	28863054,
	-23040137,
	16832058,
	-11026626,
	6134795,
	-2408993,
	-115216,
	1562212,
	-2153762,
	2150023,
	-1801289,
	1314775,
	-837618,
	454488,
	-196202,
	-131473,
	371717,
	-782605,
	1373031,
	-2101682,
	2858925,
	-3456591,
	3630827,
	-3061969,
	1413132,
	1613909,
	-6210372,
	12387561,
	-19909241,
	28239346,
	-36512758,
	43531654,
	-47780750,
	47439236,
	-40338013,
	23743694,
	6341573,
	-57365516,
	148165618,
	-351150697,
	1460028025,
	1072693503,
	-395582527,
	219760264,
	-129167353,
	71010149,
	-30795665,
	2946086,
	15372957,
	-26074168,
	30764655,
	-30938080,
	27997590,
	-23213259,
	17664378,
	-12193161,
	7382834,
	-3563842,
	845301,
	835459,
	-1654484,
	1843234,
	-1638800,
	1247420,
	-823393,
	462594,
	-207687,
	-100192,
	324719,
	-730354,
	1339905,
	-2129028,
	3005395,
	-3794399,
	4236922,
	-4003815,
	2729728,
	-65882,
	-4252020,
	10327964,
	-18031467,
	26935463,
	-36274990,
	44930776,
	-51433125,
	53966307,
	-50324581,
	37703923,
	-12020719,
	-34248170,
	120058811,
	-319547822,
	1541380411,
	965574536,
	-387847468,
	226702870,
	-140660768,
	83633578,
	-42881510,
	13580950,
	6657687,
	-19437611,
	26140600,
	-28102936,
	26631281,
	-22953565,
	18152577,
	-13108687,
	8465674,
	-4624776,
	1765456,
	113226,
	-1139329,
	1512104,
	-1451564,
	1159418,
	-794303,
	461406,
	-214216,
	-64550,
	268575,
	-661292,
	1279372,
	-2116265,
	3098967,
	-4069814,
	4779139,
	-4894023,
	4026693,
	-1783226,
	-2170207,
	8029917,
	-15775292,
	25097943,
	-35349048,
	45510558,
	-54187936,
	59607514,
	-59573385,
	51273443,
	-30636165,
	-9745121,
	88445423,
	-279431514,
	1614658080,
	856539835,
	-373541538,
	229239476,
	-149237778,
	94409091,
	-53913345,
	23749746,
	-2011483,
	-12567987,
	21120916,
	-24802430,
	24799176,
	-22274546,
	18294762,
	-13761673,
	9366983,
	-5574342,
	2629159,
	-591147,
	-618392,
	1163621,
	-1243986,
	1053259,
	-751588,
	451454,
	-215999,
	-25024,
	203942,
	-576030,
	1191526,
	-2062208,
	3136112,
	-4275678,
	5245328,
	-5714267,
	5278863,
	-3506371,
	-1690,
	5531904,
	-13175685,
	22750948,
	-33739058,
	45244301,
	-55975907,
	64239127,
	-67895272,
	64189024,
	-49163659,
	15733235,
	53748160,
	-230831156,
	1679053311,
	746709000,
	-353276190,
	227489980,
	-154827509,
	103185037,
	-63711743,
	33274801,
	-10475609,
	-5596560,
	15805872,
	-21106513,
	22544278,
	-21197204,
	18095511,
	-14145721,
	10074178,
	-6397585,
	3421790,
	-1265023,
	-101577,
	804881,
	-1020701,
	931684,
	-696695,
	433415,
	-213333,
	17791,
	131688,
	-475532,
	1077005,
	-1966436,
	3114286,
	-4405963,
	5624466,
	-6447091,
	6461321,
	-5202728,
	2214320,
	2877007,
	-10274824,
	19928738,
	-31462127,
	44120724,
	-56744596,
	67753885,
	-75114450,
	76193594,
	-67254839,
	41745825,
	16460928,
	-173887893,
	1733852586,
	637190454,
	-327720020,
	221643306,
	-157414230,
	109849580,
	-72123658,
	41993823,
	-18582587,
	1346008,
	10299303,
	-17091120,
	19916591,
	-19749515,
	17565651,
	-14259577,
	10578583,
	-7082270,
	4130433,
	-1896682,
	401584,
	442941,
	-786478,
	797620,
	-631244,
	408096,
	-206594,
	63190,
	52885,
	-361114,
	937004,
	-1829335,
	3032010,
	-4455925,
	5906903,
	-7076264,
	7549877,
	-6839472,
	4436862,
	112182,
	-7121438,
	16675229,
	-28548276,
	42144471,
	-56459696,
	70063328,
	-81072038,
	87041119,
	-84560253,
	67827790,
	-22856546,
	-108855370,
	1778446098,
	529067481,
	-297587677,
	211951991,
	-157036133,
	114331768,
	-79024638,
	49762567,
	-26189820,
	8132057,
	4706629,
	-12836658,
	16972057,
	-17965764,
	16721924,
	-14107040,
	10875497,
	-7619042,
	4744073,
	-2475521,
	882096,
	84690,
	-546125,
	654120,
	-556988,
	376406,
	-196221,
	110367,
	-31202,
	-234431,
	773268,
	-1652111,
	2888933,
	-4422234,
	6084582,
	-7587128,
	8521553,
	-8384165,
	6623954,
	-2712550,
	-3770016,
	13043342,
	-25040115,
	39336334,
	-55106036,
	71099825,
	-85629375,
	96501411,
	-100735707,
	93497987,
	-63585259,
	-36098867,
	1812335900,
	423384751,
	-263628339,
	198725893,
	-153783204,
	116601851,
	-84320484,
	56457111,
	-33166657,
	14639231,
	-867126,
	-8426423,
	13771400,
	-15885789,
	15586558,
	-13696782,
	10964194,
	-8001533,
	5253760,
	-2992218,
	1331662,
	-263274,
	-304392,
	504292,
	-475770,
	339334,
	-182704,
	158426,
	-119135,
	-97459,
	588081,
	-1436800,
	2685872,
	-4303075,
	6151241,
	-7966914,
	9355049,
	-9805380,
	8733380,
	-5544994,
	-279892,
	9094186,
	-20992285,
	35733161,
	-52688233,
	70818254,
	-88671030,
	104364730,
	-115448664,
	118266980,
	-105054593,
	43907150,
	1835142568,
	321135533,
	-226613958,
	182325160,
	-147794258,
	116670859,
	-87948323,
	61975727,
	-39396566,
	20752631,
	-6319272,
	-3945006,
	10378908,
	-13554119,
	14186734,
	-13042081,
	10847843,
	-8226411,
	5652714,
	-3438876,
	1742814,
	-594767,
	-65886,
	351244,
	-389487,
	297924,
	-166569,
	206397,
	-209322,
	47534,
	384239,
	-1186255,
	2424839,
	-4098230,
	6102582,
	-8205040,
	10031194,
	-11073331,
	10723485,
	-8331721,
	3285785,
	4896061,
	-16470657,
	31387478,
	-49230987,
	69197295,
	-90107433,
	110446135,
	-128384601,
	141645404,
	-146550977,
	130583077,
	1846610288,
	223249753,
	-187327445,
	163152569,
	-139253201,
	114589459,
	-89877091,
	66240300,
	-44779009,
	26366797,
	-11551306,
	523301,
	6861167,
	-11019062,
	12553979,
	-12160474,
	10533360,
	-8293365,
	5936407,
	-3809131,
	2109032,
	-904116,
	165016,
	198018,
	-300044,
	253251,
	-148365

};

static const struct src_stage src_int32_21_20_4167_5000 = {
	19, 20, 21, 52, 1092, 20, 21, 0, 0,
	src_int32_21_20_4167_5000_fir};
