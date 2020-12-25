// SPDX - License - Identifier: BSD - 3 - Clause
//
//Copyright(c) 2020 Intel Corporation.All rights reserved.
//
//Author : Shriram Shastry <malladi.sastry@linux.intel.com>
#pragma warning (disable : 4996)
#pragma warning (disable : 4013)
#include <stdio.h>
#include <stdlib.h>
#include "typdef.h"
#include "init_sinedata_fixpt.h"
#include <stdint.h>

const int32_t sine_table[SINE_TABLE_SIZE] = {
	0,
	6588387,
	13176712,
	19764913,
	26352928,
	32940695,
	39528151,
	46115236,
	52701887,
	59288042,
	65873638,
	72458615,
	79042909,
	85626460,
	92209205,
	98791081,
	105372028,
	111951983,
	118530885,
	125108670,
	131685278,
	138260647,
	144834714,
	151407418,
	157978697,
	164548489,
	171116732,
	177683365,
	184248325,
	190811551,
	197372981,
	203932553,
	210490206,
	217045877,
	223599506,
	230151030,
	236700388,
	243247517,
	249792358,
	256334847,
	262874923,
	269412525,
	275947592,
	282480061,
	289009871,
	295536961,
	302061269,
	308582734,
	315101294,
	321616889,
	328129457,
	334638936,
	341145265,
	347648383,
	354148229,
	360644742,
	367137860,
	373627523,
	380113669,
	386596237,
	393075166,
	399550396,
	406021864,
	412489512,
	418953276,
	425413098,
	431868915,
	438320667,
	444768293,
	451211734,
	457650927,
	464085813,
	470516330,
	476942419,
	483364019,
	489781069,
	496193509,
	502601279,
	509004318,
	515402566,
	521795963,
	528184448,
	534567963,
	540946445,
	547319836,
	553688076,
	560051103,
	566408860,
	572761285,
	579108319,
	585449903,
	591785976,
	598116478,
	604441351,
	610760535,
	617073970,
	623381597,
	629683357,
	635979190,
	642269036,
	648552837,
	654830534,
	661102068,
	667367379,
	673626408,
	679879097,
	686125386,
	692365218,
	698598533,
	704825272,
	711045377,
	717258790,
	723465451,
	729665303,
	735858287,
	742044345,
	748223418,
	754395449,
	760560379,
	766718151,
	772868706,
	779011986,
	785147934,
	791276492,
	797397602,
	803511207,
	809617248,
	815715670,
	821806413,
	827889421,
	833964637,
	840032003,
	846091463,
	852142959,
	858186434,
	864221832,
	870249095,
	876268167,
	882278991,
	888281511,
	894275670,
	900261412,
	906238681,
	912207419,
	918167571,
	924119082,
	930061894,
	935995952,
	941921200,
	947837582,
	953745043,
	959643527,
	965532978,
	971413341,
	977284561,
	983146583,
	988999351,
	994842809,
	1000676905,
	1006501581,
	1012316784,
	1018122458,
	1023918549,
	1029705003,
	1035481765,
	1041248781,
	1047005996,
	1052753356,
	1058490807,
	1064218296,
	1069935767,
	1075643168,
	1081340445,
	1087027543,
	1092704410,
	1098370992,
	1104027236,
	1109673088,
	1115308496,
	1120933406,
	1126547765,
	1132151521,
	1137744620,
	1143327011,
	1148898640,
	1154459455,
	1160009404,
	1165548435,
	1171076495,
	1176593532,
	1182099495,
	1187594332,
	1193077990,
	1198550419,
	1204011566,
	1209461381,
	1214899812,
	1220326808,
	1225742318,
	1231146290,
	1236538675,
	1241919421,
	1247288477,
	1252645793,
	1257991319,
	1263325005,
	1268646799,
	1273956652,
	1279254515,
	1284540337,
	1289814068,
	1295075658,
	1300325059,
	1305562221,
	1310787095,
	1315999631,
	1321199780,
	1326387493,
	1331562722,
	1336725418,
	1341875532,
	1347013016,
	1352137822,
	1357249900,
	1362349204,
	1367435684,
	1372509294,
	1377569985,
	1382617710,
	1387652421,
	1392674071,
	1397682613,
	1402677999,
	1407660183,
	1412629117,
	1417584755,
	1422527050,
	1427455956,
	1432371426,
	1437273414,
	1442161874,
	1447036759,
	1451898025,
	1456745625,
	1461579513,
	1466399644,
	1471205973,
	1475998455,
	1480777044,
	1485541695,
	1490292364,
	1495029005,
	1499751575,
	1504460029,
	1509154322,
	1513834410,
	1518500249,
	1523151796,
	1527789006,
	1532411836,
	1537020243,
	1541614182,
	1546193612,
	1550758488,
	1555308767,
	1559844407,
	1564365366,
	1568871600,
	1573363067,
	1577839726,
	1582301533,
	1586748446,
	1591180425,
	1595597427,
	1599999410,
	1604386334,
	1608758157,
	1613114837,
	1617456334,
	1621782607,
	1626093615,
	1630389318,
	1634669675,
	1638934646,
	1643184190,
	1647418268,
	1651636840,
	1655839867,
	1660027308,
	1664199124,
	1668355276,
	1672495724,
	1676620431,
	1680729357,
	1684822463,
	1688899710,
	1692961061,
	1697006478,
	1701035921,
	1705049354,
	1709046738,
	1713028036,
	1716993211,
	1720942224,
	1724875039,
	1728791619,
	1732691927,
	1736575926,
	1740443580,
	1744294852,
	1748129706,
	1751948106,
	1755750016,
	1759535401,
	1763304223,
	1767056449,
	1770792043,
	1774510970,
	1778213194,
	1781898680,
	1785567395,
	1789219304,
	1792854372,
	1796472564,
	1800073848,
	1803658188,
	1807225552,
	1810775906,
	1814309215,
	1817825448,
	1821324571,
	1824806551,
	1828271355,
	1831718951,
	1835149305,
	1838562387,
	1841958164,
	1845336603,
	1848697673,
	1852041343,
	1855367580,
	1858676354,
	1861967633,
	1865241387,
	1868497585,
	1871736195,
	1874957188,
	1878160534,
	1881346201,
	1884514160,
	1887664382,
	1890796836,
	1893911493,
	1897008324,
	1900087300,
	1903148391,
	1906191569,
	1909216806,
	1912224072,
	1915213339,
	1918184580,
	1921137766,
	1924072870,
	1926989863,
	1929888719,
	1932769410,
	1935631909,
	1938476189,
	1941302224,
	1944109986,
	1946899450,
	1949670588,
	1952423376,
	1955157787,
	1957873795,
	1960571374,
	1963250500,
	1965911147,
	1968553291,
	1971176905,
	1973781966,
	1976368449,
	1978936330,
	1981485584,
	1984016188,
	1986528117,
	1989021349,
	1991495859,
	1993951624,
	1996388621,
	1998806828,
	2001206221,
	2003586778,
	2005948477,
	2008291295,
	2010615209,
	2012920200,
	2015206244,
	2017473320,
	2019721407,
	2021950483,
	2024160528,
	2026351521,
	2028523441,
	2030676268,
	2032809981,
	2034924561,
	2037019987,
	2039096240,
	2041153301,
	2043191149,
	2045209766,
	2047209132,
	2049189230,
	2051150040,
	2053091543,
	2055013722,
	2056916559,
	2058800035,
	2060664132,
	2062508835,
	2064334123,
	2066139982,
	2067926393,
	2069693341,
	2071440807,
	2073168776,
	2074877232,
	2076566159,
	2078235539,
	2079885359,
	2081515602,
	2083126253,
	2084717297,
	2086288719,
	2087840504,
	2089372637,
	2090885104,
	2092377891,
	2093850984,
	2095304369,
	2096738031,
	2098151959,
	2099546138,
	2100920555,
	2102275198,
	2103610053,
	2104925108,
	2106220351,
	2107495769,
	2108751351,
	2109987084,
	2111202958,
	2112398959,
	2113575079,
	2114731304,
	2115867625,
	2116984030,
	2118080510,
	2119157053,
	2120213650,
	2121250291,
	2122266966,
	2123263665,
	2124240379,
	2125197099,
	2126133816,
	2127050521,
	2127947205,
	2128823861,
	2129680479,
	2130517051,
	2131333571,
	2132130029,
	2132906419,
	2133662733,
	2134398965,
	2135115106,
	2135811152,
	2136487094,
	2137142926,
	2137778643,
	2138394239,
	2138989707,
	2139565042,
	2140120239,
	2140655292,
	2141170196,
	2141664947,
	2142139540,
	2142593970,
	2143028233,
	2143442325,
	2143836243,
	2144209981,
	2144563538,
	2144896909,
	2145210091,
	2145503082,
	2145775879,
	2146028479,
	2146260880,
	2146473079,
	2146665075,
	2146836865,
	2146988449,
	2147119824,
	2147230990,
	2147321945,
	2147392689,
	2147443221,
	2147473541,
	2147483647
};

/*degree is input in Q2.30 */
/*radian is outputin Q1.31 */
inline int32_t drc_sin_fixed(int32_t x, int8_t i, FILE *fd)
{
	const int32_t PI_OVER_TWO = Q_CONVERT_FLOAT(1.57079632679489661923f, 30);
	/* input range of sin_fixed() is non-negative */
	int32_t abs_sin_val = sin_fixed(q_mult(ABS(x), PI_OVER_TWO, 30, 30, 28));
   fprintf(fd, " %13li\n", SGN(x) < 0 ? -abs_sin_val : abs_sin_val);
	return SGN(x) < 0 ? -abs_sin_val : abs_sin_val;
}

/* Compute fixed point sine with table lookup and interpolation */
int32_t sin_fixed(int32_t w)
{
	int idx;
	int32_t frac;
	int32_t s0;
	int32_t s1;
	int32_t delta;
	int64_t sine;
	int64_t idx_tmp;

	/* Q4.28 x Q12.20 -> Q16.48 */
	idx_tmp = (int64_t)w * SINE_C_Q20;
	idx = (int)(idx_tmp >> 48); /* Shift to Q0 */
	idx_tmp = idx_tmp >> 17; /* Shift to Q16.31 */
	idx_tmp = idx_tmp - (idx << 31); /* Get fraction */
	frac = (int32_t)idx_tmp; /* Q1.31 */
	s0 = sine_lookup(idx); /* Q1.31 */
	s1 = sine_lookup(idx + 1); /* Q1.31 */
	delta = s1 - s0; /* Q1.31 */
	/* All Q1.31 */
	sine = s0 + q_mults_32x32(frac, delta, Q_SHIFT_BITS_64(31, 31, 31));

	return (int32_t)sine;
}


/* Sine lookup table read */
static inline int32_t sine_lookup(int idx)
{
	int32_t s;
	int i1;

	i1 = idx & (2 * SINE_NQUART - 1);
	if (i1 > SINE_NQUART)
		i1 = 2 * SINE_NQUART - i1;

	if (idx > 2 * SINE_NQUART)
		s = -sine_table[i1];
	else
		s = sine_table[i1];

	return s;
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
	FILE *fd = fopen("Results/drc_sin_fixed.txt", "w");
	fprintf(fd, " %10s  %10s %10s \n", "idx", "in-sine", "out-sine");
	init_sinedata_fixpt(x);
	for (i = 0; i < TEST_VECTOR; i++) {
		fprintf(fd, " %10d %11li ", i, x[i]);
		drc_sin_fixed(x[i], i, fd);
	}
	fclose(fd);
}

/*
 * File trailer for drc_sin_fixed.c
 *
 * [EOF]
 */
