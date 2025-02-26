/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2019 Intel Corporation. All rights reserved.
 */

/* Conversion from 48000 Hz to 11025 Hz */
/* NUM_FILTERS=4, FILTER_LENGTH=96, alpha=5.700000, gamma=0.417000 */

__cold_rodata static const int32_t coeff48000to11025[] = {
/* Filter #4, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-5276), /* Filter:4, Coefficient: 1 */
CONVERT_COEFF(-12951), /* Filter:4, Coefficient: 2 */
CONVERT_COEFF(41170), /* Filter:3, Coefficient: 1 */
CONVERT_COEFF(24714), /* Filter:3, Coefficient: 2 */
CONVERT_COEFF(90439), /* Filter:2, Coefficient: 1 */
CONVERT_COEFF(156156), /* Filter:2, Coefficient: 2 */
CONVERT_COEFF(-36529), /* Filter:1, Coefficient: 1 */
CONVERT_COEFF(89808), /* Filter:1, Coefficient: 2 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-16487), /* Filter:4, Coefficient: 3 */
CONVERT_COEFF(-12216), /* Filter:4, Coefficient: 4 */
CONVERT_COEFF(-16130), /* Filter:3, Coefficient: 3 */
CONVERT_COEFF(-68429), /* Filter:3, Coefficient: 4 */
CONVERT_COEFF(166381), /* Filter:2, Coefficient: 3 */
CONVERT_COEFF(85133), /* Filter:2, Coefficient: 4 */
CONVERT_COEFF(257736), /* Filter:1, Coefficient: 3 */
CONVERT_COEFF(391514), /* Filter:1, Coefficient: 4 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(540), /* Filter:4, Coefficient: 5 */
CONVERT_COEFF(18145), /* Filter:4, Coefficient: 6 */
CONVERT_COEFF(-107599), /* Filter:3, Coefficient: 5 */
CONVERT_COEFF(-106756), /* Filter:3, Coefficient: 6 */
CONVERT_COEFF(-87027), /* Filter:2, Coefficient: 5 */
CONVERT_COEFF(-298777), /* Filter:2, Coefficient: 6 */
CONVERT_COEFF(396015), /* Filter:1, Coefficient: 5 */
CONVERT_COEFF(201937), /* Filter:1, Coefficient: 6 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(33188), /* Filter:4, Coefficient: 7 */
CONVERT_COEFF(37139), /* Filter:4, Coefficient: 8 */
CONVERT_COEFF(-50319), /* Filter:3, Coefficient: 7 */
CONVERT_COEFF(54036), /* Filter:3, Coefficient: 8 */
CONVERT_COEFF(-456335), /* Filter:2, Coefficient: 7 */
CONVERT_COEFF(-457073), /* Filter:2, Coefficient: 8 */
CONVERT_COEFF(-185458), /* Filter:1, Coefficient: 7 */
CONVERT_COEFF(-658946), /* Filter:1, Coefficient: 8 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(24307), /* Filter:4, Coefficient: 9 */
CONVERT_COEFF(-4499), /* Filter:4, Coefficient: 10 */
CONVERT_COEFF(171682), /* Filter:3, Coefficient: 9 */
CONVERT_COEFF(249813), /* Filter:3, Coefficient: 10 */
CONVERT_COEFF(-238997), /* Filter:2, Coefficient: 9 */
CONVERT_COEFF(174254), /* Filter:2, Coefficient: 10 */
CONVERT_COEFF(-1024879), /* Filter:1, Coefficient: 9 */
CONVERT_COEFF(-1067924), /* Filter:1, Coefficient: 10 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-40503), /* Filter:4, Coefficient: 11 */
CONVERT_COEFF(-68959), /* Filter:4, Coefficient: 12 */
CONVERT_COEFF(237789), /* Filter:3, Coefficient: 11 */
CONVERT_COEFF(112225), /* Filter:3, Coefficient: 12 */
CONVERT_COEFF(656680), /* Filter:2, Coefficient: 11 */
CONVERT_COEFF(1007920), /* Filter:2, Coefficient: 12 */
CONVERT_COEFF(-648377), /* Filter:1, Coefficient: 11 */
CONVERT_COEFF(205595), /* Filter:1, Coefficient: 12 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-74494), /* Filter:4, Coefficient: 13 */
CONVERT_COEFF(-48117), /* Filter:4, Coefficient: 14 */
CONVERT_COEFF(-104001), /* Filter:3, Coefficient: 13 */
CONVERT_COEFF(-339463), /* Filter:3, Coefficient: 14 */
CONVERT_COEFF(1025112), /* Filter:2, Coefficient: 13 */
CONVERT_COEFF(596557), /* Filter:2, Coefficient: 14 */
CONVERT_COEFF(1256824), /* Filter:1, Coefficient: 13 */
CONVERT_COEFF(2103513), /* Filter:1, Coefficient: 14 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(7013), /* Filter:4, Coefficient: 15 */
CONVERT_COEFF(73976), /* Filter:4, Coefficient: 16 */
CONVERT_COEFF(-493890), /* Filter:3, Coefficient: 15 */
CONVERT_COEFF(-476173), /* Filter:3, Coefficient: 16 */
CONVERT_COEFF(-220901), /* Filter:2, Coefficient: 15 */
CONVERT_COEFF(-1180767), /* Filter:2, Coefficient: 16 */
CONVERT_COEFF(2312568), /* Filter:1, Coefficient: 15 */
CONVERT_COEFF(1604844), /* Filter:1, Coefficient: 16 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(126345), /* Filter:4, Coefficient: 17 */
CONVERT_COEFF(137657), /* Filter:4, Coefficient: 18 */
CONVERT_COEFF(-247687), /* Filter:3, Coefficient: 17 */
CONVERT_COEFF(147413), /* Filter:3, Coefficient: 18 */
CONVERT_COEFF(-1906017), /* Filter:2, Coefficient: 17 */
CONVERT_COEFF(-2021581), /* Filter:2, Coefficient: 18 */
CONVERT_COEFF(21881), /* Filter:1, Coefficient: 17 */
CONVERT_COEFF(-2005545), /* Filter:1, Coefficient: 18 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(93034), /* Filter:4, Coefficient: 19 */
CONVERT_COEFF(-1731), /* Filter:4, Coefficient: 20 */
CONVERT_COEFF(581451), /* Filter:3, Coefficient: 19 */
CONVERT_COEFF(878980), /* Filter:3, Coefficient: 20 */
CONVERT_COEFF(-1318811), /* Filter:2, Coefficient: 19 */
CONVERT_COEFF(113142), /* Filter:2, Coefficient: 20 */
CONVERT_COEFF(-3742184), /* Filter:1, Coefficient: 19 */
CONVERT_COEFF(-4386660), /* Filter:1, Coefficient: 20 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-118109), /* Filter:4, Coefficient: 21 */
CONVERT_COEFF(-212105), /* Filter:4, Coefficient: 22 */
CONVERT_COEFF(881333), /* Filter:3, Coefficient: 21 */
CONVERT_COEFF(518154), /* Filter:3, Coefficient: 22 */
CONVERT_COEFF(1853939), /* Filter:2, Coefficient: 21 */
CONVERT_COEFF(3252995), /* Filter:2, Coefficient: 22 */
CONVERT_COEFF(-3396385), /* Filter:1, Coefficient: 21 */
CONVERT_COEFF(-779248), /* Filter:1, Coefficient: 22 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-239586), /* Filter:4, Coefficient: 23 */
CONVERT_COEFF(-174729), /* Filter:4, Coefficient: 24 */
CONVERT_COEFF(-143352), /* Filter:3, Coefficient: 23 */
CONVERT_COEFF(-896882), /* Filter:3, Coefficient: 24 */
CONVERT_COEFF(3650862), /* Filter:2, Coefficient: 23 */
CONVERT_COEFF(2652871), /* Filter:2, Coefficient: 24 */
CONVERT_COEFF(2779889), /* Filter:1, Coefficient: 23 */
CONVERT_COEFF(6048018), /* Filter:1, Coefficient: 24 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-24432), /* Filter:4, Coefficient: 25 */
CONVERT_COEFF(168052), /* Filter:4, Coefficient: 26 */
CONVERT_COEFF(-1453384), /* Filter:3, Coefficient: 25 */
CONVERT_COEFF(-1543048), /* Filter:3, Coefficient: 26 */
CONVERT_COEFF(350965), /* Filter:2, Coefficient: 25 */
CONVERT_COEFF(-2609224), /* Filter:2, Coefficient: 26 */
CONVERT_COEFF(7629537), /* Filter:1, Coefficient: 25 */
CONVERT_COEFF(6502907), /* Filter:1, Coefficient: 26 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(333676), /* Filter:4, Coefficient: 27 */
CONVERT_COEFF(400546), /* Filter:4, Coefficient: 28 */
CONVERT_COEFF(-1029512), /* Filter:3, Coefficient: 27 */
CONVERT_COEFF(8151), /* Filter:3, Coefficient: 28 */
CONVERT_COEFF(-5174708), /* Filter:2, Coefficient: 27 */
CONVERT_COEFF(-6226971), /* Filter:2, Coefficient: 28 */
CONVERT_COEFF(2518773), /* Filter:1, Coefficient: 27 */
CONVERT_COEFF(-3351884), /* Filter:1, Coefficient: 28 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(322102), /* Filter:4, Coefficient: 29 */
CONVERT_COEFF(100162), /* Filter:4, Coefficient: 30 */
CONVERT_COEFF(1264729), /* Filter:3, Coefficient: 29 */
CONVERT_COEFF(2286428), /* Filter:3, Coefficient: 30 */
CONVERT_COEFF(-5018466), /* Filter:2, Coefficient: 29 */
CONVERT_COEFF(-1546610), /* Filter:2, Coefficient: 30 */
CONVERT_COEFF(-9170470), /* Filter:1, Coefficient: 29 */
CONVERT_COEFF(-12602533), /* Filter:1, Coefficient: 30 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-207102), /* Filter:4, Coefficient: 31 */
CONVERT_COEFF(-497722), /* Filter:4, Coefficient: 32 */
CONVERT_COEFF(2621280), /* Filter:3, Coefficient: 31 */
CONVERT_COEFF(1995783), /* Filter:3, Coefficient: 32 */
CONVERT_COEFF(3294826), /* Filter:2, Coefficient: 31 */
CONVERT_COEFF(7886940), /* Filter:2, Coefficient: 32 */
CONVERT_COEFF(-11762954), /* Filter:1, Coefficient: 31 */
CONVERT_COEFF(-6054156), /* Filter:1, Coefficient: 32 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-657260), /* Filter:4, Coefficient: 33 */
CONVERT_COEFF(-599700), /* Filter:4, Coefficient: 34 */
CONVERT_COEFF(453662), /* Filter:3, Coefficient: 33 */
CONVERT_COEFF(-1602376), /* Filter:3, Coefficient: 34 */
CONVERT_COEFF(10370645), /* Filter:2, Coefficient: 33 */
CONVERT_COEFF(9314284), /* Filter:2, Coefficient: 34 */
CONVERT_COEFF(3330959), /* Filter:1, Coefficient: 33 */
CONVERT_COEFF(13498465), /* Filter:1, Coefficient: 34 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-303789), /* Filter:4, Coefficient: 35 */
CONVERT_COEFF(169268), /* Filter:4, Coefficient: 36 */
CONVERT_COEFF(-3496605), /* Filter:3, Coefficient: 35 */
CONVERT_COEFF(-4480875), /* Filter:3, Coefficient: 36 */
CONVERT_COEFF(4342738), /* Filter:2, Coefficient: 35 */
CONVERT_COEFF(-3512410), /* Filter:2, Coefficient: 36 */
CONVERT_COEFF(20611375), /* Filter:1, Coefficient: 35 */
CONVERT_COEFF(21154438), /* Filter:1, Coefficient: 36 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(685660), /* Filter:4, Coefficient: 37 */
CONVERT_COEFF(1076048), /* Filter:4, Coefficient: 38 */
CONVERT_COEFF(-3992362), /* Filter:3, Coefficient: 37 */
CONVERT_COEFF(-1882814), /* Filter:3, Coefficient: 38 */
CONVERT_COEFF(-11914041), /* Filter:2, Coefficient: 37 */
CONVERT_COEFF(-17803913), /* Filter:2, Coefficient: 38 */
CONVERT_COEFF(13330874), /* Filter:1, Coefficient: 37 */
CONVERT_COEFF(-1889934), /* Filter:1, Coefficient: 38 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(1189612), /* Filter:4, Coefficient: 39 */
CONVERT_COEFF(947693), /* Filter:4, Coefficient: 40 */
CONVERT_COEFF(1466176), /* Filter:3, Coefficient: 39 */
CONVERT_COEFF(5196025), /* Filter:3, Coefficient: 40 */
CONVERT_COEFF(-18332740), /* Filter:2, Coefficient: 39 */
CONVERT_COEFF(-11858951), /* Filter:2, Coefficient: 40 */
CONVERT_COEFF(-20501310), /* Filter:1, Coefficient: 39 */
CONVERT_COEFF(-36179493), /* Filter:1, Coefficient: 40 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(378181), /* Filter:4, Coefficient: 41 */
CONVERT_COEFF(-382925), /* Filter:4, Coefficient: 42 */
CONVERT_COEFF(8192385), /* Filter:3, Coefficient: 41 */
CONVERT_COEFF(9416173), /* Filter:3, Coefficient: 42 */
CONVERT_COEFF(1316974), /* Filter:2, Coefficient: 41 */
CONVERT_COEFF(18760292), /* Filter:2, Coefficient: 42 */
CONVERT_COEFF(-41896152), /* Filter:1, Coefficient: 41 */
CONVERT_COEFF(-32009702), /* Filter:1, Coefficient: 42 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-1125926), /* Filter:4, Coefficient: 43 */
CONVERT_COEFF(-1628684), /* Filter:4, Coefficient: 44 */
CONVERT_COEFF(8242832), /* Filter:3, Coefficient: 43 */
CONVERT_COEFF(4697569), /* Filter:3, Coefficient: 44 */
CONVERT_COEFF(36372844), /* Filter:2, Coefficient: 43 */
CONVERT_COEFF(49436642), /* Filter:2, Coefficient: 44 */
CONVERT_COEFF(-4216305), /* Filter:1, Coefficient: 43 */
CONVERT_COEFF(39274782), /* Filter:1, Coefficient: 44 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-1725846), /* Filter:4, Coefficient: 45 */
CONVERT_COEFF(-1363486), /* Filter:4, Coefficient: 46 */
CONVERT_COEFF(-498294), /* Filter:3, Coefficient: 45 */
CONVERT_COEFF(-6096589), /* Filter:3, Coefficient: 46 */
CONVERT_COEFF(53943647), /* Filter:2, Coefficient: 45 */
CONVERT_COEFF(47812424), /* Filter:2, Coefficient: 46 */
CONVERT_COEFF(91783432), /* Filter:1, Coefficient: 45 */
CONVERT_COEFF(143507821), /* Filter:1, Coefficient: 46 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-620573), /* Filter:4, Coefficient: 47 */
CONVERT_COEFF(310846), /* Filter:4, Coefficient: 48 */
CONVERT_COEFF(-10663877), /* Filter:3, Coefficient: 47 */
CONVERT_COEFF(-12993683), /* Filter:3, Coefficient: 48 */
CONVERT_COEFF(31605996), /* Filter:2, Coefficient: 47 */
CONVERT_COEFF(8506473), /* Filter:2, Coefficient: 48 */
CONVERT_COEFF(183866425), /* Filter:1, Coefficient: 47 */
CONVERT_COEFF(204194917), /* Filter:1, Coefficient: 48 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(1181314), /* Filter:4, Coefficient: 49 */
CONVERT_COEFF(1758112), /* Filter:4, Coefficient: 50 */
CONVERT_COEFF(-12461531), /* Filter:3, Coefficient: 49 */
CONVERT_COEFF(-9210650), /* Filter:3, Coefficient: 50 */
CONVERT_COEFF(-16471793), /* Filter:2, Coefficient: 49 */
CONVERT_COEFF(-37810559), /* Filter:2, Coefficient: 50 */
CONVERT_COEFF(200025358), /* Filter:1, Coefficient: 49 */
CONVERT_COEFF(172279208), /* Filter:1, Coefficient: 50 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(1894276), /* Filter:4, Coefficient: 51 */
CONVERT_COEFF(1570552), /* Filter:4, Coefficient: 52 */
CONVERT_COEFF(-4109186), /* Filter:3, Coefficient: 51 */
CONVERT_COEFF(1507388), /* Filter:3, Coefficient: 52 */
CONVERT_COEFF(-50965956), /* Filter:2, Coefficient: 51 */
CONVERT_COEFF(-53557320), /* Filter:2, Coefficient: 52 */
CONVERT_COEFF(127020431), /* Filter:1, Coefficient: 51 */
CONVERT_COEFF(73842077), /* Filter:1, Coefficient: 52 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(897380), /* Filter:4, Coefficient: 53 */
CONVERT_COEFF(77192), /* Filter:4, Coefficient: 54 */
CONVERT_COEFF(6226751), /* Filter:3, Coefficient: 53 */
CONVERT_COEFF(8960398), /* Filter:3, Coefficient: 54 */
CONVERT_COEFF(-45919329), /* Filter:2, Coefficient: 53 */
CONVERT_COEFF(-30871316), /* Filter:2, Coefficient: 54 */
CONVERT_COEFF(23363491), /* Filter:1, Coefficient: 53 */
CONVERT_COEFF(-15432232), /* Filter:1, Coefficient: 54 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-659808), /* Filter:4, Coefficient: 55 */
CONVERT_COEFF(-1125092), /* Filter:4, Coefficient: 56 */
CONVERT_COEFF(9231586), /* Filter:3, Coefficient: 55 */
CONVERT_COEFF(7267416), /* Filter:3, Coefficient: 56 */
CONVERT_COEFF(-12800875), /* Filter:2, Coefficient: 55 */
CONVERT_COEFF(3635617), /* Filter:2, Coefficient: 56 */
CONVERT_COEFF(-37267226), /* Filter:1, Coefficient: 55 */
CONVERT_COEFF(-41497734), /* Filter:1, Coefficient: 56 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-1224338), /* Filter:4, Coefficient: 57 */
CONVERT_COEFF(-977499), /* Filter:4, Coefficient: 58 */
CONVERT_COEFF(3877187), /* Filter:3, Coefficient: 57 */
CONVERT_COEFF(167487), /* Filter:3, Coefficient: 58 */
CONVERT_COEFF(14790492), /* Filter:2, Coefficient: 57 */
CONVERT_COEFF(18905076), /* Filter:2, Coefficient: 58 */
CONVERT_COEFF(-31720872), /* Filter:1, Coefficient: 57 */
CONVERT_COEFF(-14278017), /* Filter:1, Coefficient: 58 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-502204), /* Filter:4, Coefficient: 59 */
CONVERT_COEFF(31518), /* Filter:4, Coefficient: 60 */
CONVERT_COEFF(-2806990), /* Filter:3, Coefficient: 59 */
CONVERT_COEFF(-4344247), /* Filter:3, Coefficient: 60 */
CONVERT_COEFF(16363909), /* Filter:2, Coefficient: 59 */
CONVERT_COEFF(9303267), /* Filter:2, Coefficient: 60 */
CONVERT_COEFF(3817177), /* Filter:1, Coefficient: 59 */
CONVERT_COEFF(16872467), /* Filter:1, Coefficient: 60 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(458491), /* Filter:4, Coefficient: 61 */
CONVERT_COEFF(668294), /* Filter:4, Coefficient: 62 */
CONVERT_COEFF(-4258557), /* Filter:3, Coefficient: 61 */
CONVERT_COEFF(-2868976), /* Filter:3, Coefficient: 62 */
CONVERT_COEFF(754906), /* Filter:2, Coefficient: 61 */
CONVERT_COEFF(-6366692), /* Filter:2, Coefficient: 62 */
CONVERT_COEFF(21863748), /* Filter:1, Coefficient: 61 */
CONVERT_COEFF(18819227), /* Filter:1, Coefficient: 62 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(632521), /* Filter:4, Coefficient: 63 */
CONVERT_COEFF(402936), /* Filter:4, Coefficient: 64 */
CONVERT_COEFF(-834307), /* Filter:3, Coefficient: 63 */
CONVERT_COEFF(1096696), /* Filter:3, Coefficient: 64 */
CONVERT_COEFF(-10107072), /* Filter:2, Coefficient: 63 */
CONVERT_COEFF(-9905933), /* Filter:2, Coefficient: 64 */
CONVERT_COEFF(10252203), /* Filter:1, Coefficient: 63 */
CONVERT_COEFF(-56657), /* Filter:1, Coefficient: 64 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(84254), /* Filter:4, Coefficient: 65 */
CONVERT_COEFF(-206679), /* Filter:4, Coefficient: 66 */
CONVERT_COEFF(2330787), /* Filter:3, Coefficient: 65 */
CONVERT_COEFF(2593273), /* Filter:3, Coefficient: 66 */
CONVERT_COEFF(-6539839), /* Filter:2, Coefficient: 65 */
CONVERT_COEFF(-1656983), /* Filter:2, Coefficient: 66 */
CONVERT_COEFF(-8463246), /* Filter:1, Coefficient: 65 */
CONVERT_COEFF(-12588471), /* Filter:1, Coefficient: 66 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-380298), /* Filter:4, Coefficient: 67 */
CONVERT_COEFF(-399448), /* Filter:4, Coefficient: 68 */
CONVERT_COEFF(1966425), /* Filter:3, Coefficient: 67 */
CONVERT_COEFF(807090), /* Filter:3, Coefficient: 68 */
CONVERT_COEFF(2892064), /* Filter:2, Coefficient: 67 */
CONVERT_COEFF(5683926), /* Filter:2, Coefficient: 68 */
CONVERT_COEFF(-11859264), /* Filter:1, Coefficient: 67 */
CONVERT_COEFF(-7381325), /* Filter:1, Coefficient: 68 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-283699), /* Filter:4, Coefficient: 69 */
CONVERT_COEFF(-94156), /* Filter:4, Coefficient: 70 */
CONVERT_COEFF(-413134), /* Filter:3, Coefficient: 69 */
CONVERT_COEFF(-1281462), /* Filter:3, Coefficient: 70 */
CONVERT_COEFF(6114153), /* Filter:2, Coefficient: 69 */
CONVERT_COEFF(4458459), /* Filter:2, Coefficient: 70 */
CONVERT_COEFF(-1289801), /* Filter:1, Coefficient: 69 */
CONVERT_COEFF(4127659), /* Filter:1, Coefficient: 70 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(93448), /* Filter:4, Coefficient: 71 */
CONVERT_COEFF(216583), /* Filter:4, Coefficient: 72 */
CONVERT_COEFF(-1571374), /* Filter:3, Coefficient: 71 */
CONVERT_COEFF(-1287849), /* Filter:3, Coefficient: 72 */
CONVERT_COEFF(1633571), /* Filter:2, Coefficient: 71 */
CONVERT_COEFF(-1216156), /* Filter:2, Coefficient: 72 */
CONVERT_COEFF(7210746), /* Filter:1, Coefficient: 71 */
CONVERT_COEFF(7366642), /* Filter:1, Coefficient: 72 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(245013), /* Filter:4, Coefficient: 73 */
CONVERT_COEFF(185843), /* Filter:4, Coefficient: 74 */
CONVERT_COEFF(-627349), /* Filter:3, Coefficient: 73 */
CONVERT_COEFF(120817), /* Filter:3, Coefficient: 74 */
CONVERT_COEFF(-3140159), /* Filter:2, Coefficient: 73 */
CONVERT_COEFF(-3667371), /* Filter:2, Coefficient: 74 */
CONVERT_COEFF(5079392), /* Filter:1, Coefficient: 73 */
CONVERT_COEFF(1556950), /* Filter:1, Coefficient: 74 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(74882), /* Filter:4, Coefficient: 75 */
CONVERT_COEFF(-40571), /* Filter:4, Coefficient: 76 */
CONVERT_COEFF(688759), /* Filter:3, Coefficient: 75 */
CONVERT_COEFF(917942), /* Filter:3, Coefficient: 76 */
CONVERT_COEFF(-2881001), /* Filter:2, Coefficient: 75 */
CONVERT_COEFF(-1291563), /* Filter:2, Coefficient: 76 */
CONVERT_COEFF(-1803822), /* Filter:1, Coefficient: 75 */
CONVERT_COEFF(-3921315), /* Filter:1, Coefficient: 76 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-120129), /* Filter:4, Coefficient: 77 */
CONVERT_COEFF(-143205), /* Filter:4, Coefficient: 78 */
CONVERT_COEFF(794458), /* Filter:3, Coefficient: 77 */
CONVERT_COEFF(427950), /* Filter:3, Coefficient: 78 */
CONVERT_COEFF(414286), /* Filter:2, Coefficient: 77 */
CONVERT_COEFF(1640931), /* Filter:2, Coefficient: 78 */
CONVERT_COEFF(-4335656), /* Filter:1, Coefficient: 77 */
CONVERT_COEFF(-3247151), /* Filter:1, Coefficient: 78 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-112715), /* Filter:4, Coefficient: 79 */
CONVERT_COEFF(-49779), /* Filter:4, Coefficient: 80 */
CONVERT_COEFF(-8978), /* Filter:3, Coefficient: 79 */
CONVERT_COEFF(-352674), /* Filter:3, Coefficient: 80 */
CONVERT_COEFF(2071182), /* Filter:2, Coefficient: 79 */
CONVERT_COEFF(1722382), /* Filter:2, Coefficient: 80 */
CONVERT_COEFF(-1321521), /* Filter:1, Coefficient: 79 */
CONVERT_COEFF(627989), /* Filter:1, Coefficient: 80 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(17150), /* Filter:4, Coefficient: 81 */
CONVERT_COEFF(63819), /* Filter:4, Coefficient: 82 */
CONVERT_COEFF(-504123), /* Filter:3, Coefficient: 81 */
CONVERT_COEFF(-451338), /* Filter:3, Coefficient: 82 */
CONVERT_COEFF(875132), /* Filter:2, Coefficient: 81 */
CONVERT_COEFF(-76722), /* Filter:2, Coefficient: 82 */
CONVERT_COEFF(1947985), /* Filter:1, Coefficient: 81 */
CONVERT_COEFF(2336224), /* Filter:1, Coefficient: 82 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(78078), /* Filter:4, Coefficient: 83 */
CONVERT_COEFF(61882), /* Filter:4, Coefficient: 84 */
CONVERT_COEFF(-256412), /* Filter:3, Coefficient: 83 */
CONVERT_COEFF(-18431), /* Filter:3, Coefficient: 84 */
CONVERT_COEFF(-786703), /* Filter:2, Coefficient: 83 */
CONVERT_COEFF(-1067401), /* Filter:2, Coefficient: 84 */
CONVERT_COEFF(1872047), /* Filter:1, Coefficient: 83 */
CONVERT_COEFF(907040), /* Filter:1, Coefficient: 84 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(27740), /* Filter:4, Coefficient: 85 */
CONVERT_COEFF(-8034), /* Filter:4, Coefficient: 86 */
CONVERT_COEFF(169685), /* Filter:3, Coefficient: 85 */
CONVERT_COEFF(253408), /* Filter:3, Coefficient: 86 */
CONVERT_COEFF(-922590), /* Filter:2, Coefficient: 85 */
CONVERT_COEFF(-503998), /* Filter:2, Coefficient: 86 */
CONVERT_COEFF(-116914), /* Filter:1, Coefficient: 85 */
CONVERT_COEFF(-842108), /* Filter:1, Coefficient: 86 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-32141), /* Filter:4, Coefficient: 87 */
CONVERT_COEFF(-38602), /* Filter:4, Coefficient: 88 */
CONVERT_COEFF(228111), /* Filter:3, Coefficient: 87 */
CONVERT_COEFF(129707), /* Filter:3, Coefficient: 88 */
CONVERT_COEFF(-23864), /* Filter:2, Coefficient: 87 */
CONVERT_COEFF(335367), /* Filter:2, Coefficient: 88 */
CONVERT_COEFF(-1100770), /* Filter:1, Coefficient: 87 */
CONVERT_COEFF(-928696), /* Filter:1, Coefficient: 88 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(-29393), /* Filter:4, Coefficient: 89 */
CONVERT_COEFF(-11914), /* Filter:4, Coefficient: 90 */
CONVERT_COEFF(12179), /* Filter:3, Coefficient: 89 */
CONVERT_COEFF(-76760), /* Filter:3, Coefficient: 90 */
CONVERT_COEFF(480135), /* Filter:2, Coefficient: 89 */
CONVERT_COEFF(418338), /* Filter:2, Coefficient: 90 */
CONVERT_COEFF(-502241), /* Filter:1, Coefficient: 89 */
CONVERT_COEFF(-39321), /* Filter:1, Coefficient: 90 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(5087), /* Filter:4, Coefficient: 91 */
CONVERT_COEFF(15234), /* Filter:4, Coefficient: 92 */
CONVERT_COEFF(-112173), /* Filter:3, Coefficient: 91 */
CONVERT_COEFF(-95873), /* Filter:3, Coefficient: 92 */
CONVERT_COEFF(230980), /* Filter:2, Coefficient: 91 */
CONVERT_COEFF(22991), /* Filter:2, Coefficient: 92 */
CONVERT_COEFF(290352), /* Filter:1, Coefficient: 91 */
CONVERT_COEFF(414260), /* Filter:1, Coefficient: 92 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(16461), /* Filter:4, Coefficient: 93 */
CONVERT_COEFF(10800), /* Filter:4, Coefficient: 94 */
CONVERT_COEFF(-49051), /* Filter:3, Coefficient: 93 */
CONVERT_COEFF(991), /* Filter:3, Coefficient: 94 */
CONVERT_COEFF(-122969), /* Filter:2, Coefficient: 93 */
CONVERT_COEFF(-172363), /* Filter:2, Coefficient: 94 */
CONVERT_COEFF(356624), /* Filter:1, Coefficient: 93 */
CONVERT_COEFF(201071), /* Filter:1, Coefficient: 94 */

/* Filter #1, conversion from 48000 Hz to 11025 Hz */

CONVERT_COEFF(2550), /* Filter:4, Coefficient: 95 */
CONVERT_COEFF(-797683), /* Filter:4, Coefficient: 96 */
CONVERT_COEFF(33364), /* Filter:3, Coefficient: 95 */
CONVERT_COEFF(1235716), /* Filter:3, Coefficient: 96 */
CONVERT_COEFF(-138918), /* Filter:2, Coefficient: 95 */
CONVERT_COEFF(-375528), /* Filter:2, Coefficient: 96 */
CONVERT_COEFF(40502), /* Filter:1, Coefficient: 95 */
CONVERT_COEFF(-62504)

};
