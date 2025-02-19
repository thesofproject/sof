/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2019 Intel Corporation. All rights reserved.
 */

/* Conversion from 48000 Hz to 48000 Hz */
/* NUM_FILTERS=7, FILTER_LENGTH=48, alpha=7.150000, gamma=0.458000 */

__cold_rodata static const int32_t coeff48000to48000[] = {
/* Filter #7, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(201584), /* Filter:7, Coefficient: 1 */
CONVERT_COEFF(-337772), /* Filter:7, Coefficient: 2 */
CONVERT_COEFF(-509717), /* Filter:6, Coefficient: 1 */
CONVERT_COEFF(901735), /* Filter:6, Coefficient: 2 */
CONVERT_COEFF(-76220), /* Filter:5, Coefficient: 1 */
CONVERT_COEFF(108575), /* Filter:5, Coefficient: 2 */
CONVERT_COEFF(427848), /* Filter:4, Coefficient: 1 */
CONVERT_COEFF(-890617), /* Filter:4, Coefficient: 2 */
CONVERT_COEFF(450982), /* Filter:3, Coefficient: 1 */
CONVERT_COEFF(-810046), /* Filter:3, Coefficient: 2 */
CONVERT_COEFF(-134205), /* Filter:2, Coefficient: 1 */
CONVERT_COEFF(408052), /* Filter:2, Coefficient: 2 */
CONVERT_COEFF(-119772), /* Filter:1, Coefficient: 1 */
CONVERT_COEFF(240527), /* Filter:1, Coefficient: 2 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(532323), /* Filter:7, Coefficient: 3 */
CONVERT_COEFF(-781809), /* Filter:7, Coefficient: 4 */
CONVERT_COEFF(-1563722), /* Filter:6, Coefficient: 3 */
CONVERT_COEFF(2576238), /* Filter:6, Coefficient: 4 */
CONVERT_COEFF(38232), /* Filter:5, Coefficient: 3 */
CONVERT_COEFF(-557092), /* Filter:5, Coefficient: 4 */
CONVERT_COEFF(1688361), /* Filter:4, Coefficient: 3 */
CONVERT_COEFF(-2889171), /* Filter:4, Coefficient: 4 */
CONVERT_COEFF(1143995), /* Filter:3, Coefficient: 3 */
CONVERT_COEFF(-1251295), /* Filter:3, Coefficient: 4 */
CONVERT_COEFF(-969157), /* Filter:2, Coefficient: 3 */
CONVERT_COEFF(1915703), /* Filter:2, Coefficient: 4 */
CONVERT_COEFF(-379589), /* Filter:1, Coefficient: 3 */
CONVERT_COEFF(490498), /* Filter:1, Coefficient: 4 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(1063090), /* Filter:7, Coefficient: 5 */
CONVERT_COEFF(-1325755), /* Filter:7, Coefficient: 6 */
CONVERT_COEFF(-3983820), /* Filter:6, Coefficient: 5 */
CONVERT_COEFF(5758133), /* Filter:6, Coefficient: 6 */
CONVERT_COEFF(1699253), /* Filter:5, Coefficient: 5 */
CONVERT_COEFF(-3746237), /* Filter:5, Coefficient: 6 */
CONVERT_COEFF(4484829), /* Filter:4, Coefficient: 5 */
CONVERT_COEFF(-6340864), /* Filter:4, Coefficient: 6 */
CONVERT_COEFF(828449), /* Filter:3, Coefficient: 5 */
CONVERT_COEFF(513188), /* Filter:3, Coefficient: 6 */
CONVERT_COEFF(-3299444), /* Filter:2, Coefficient: 5 */
CONVERT_COEFF(5081025), /* Filter:2, Coefficient: 6 */
CONVERT_COEFF(-496983), /* Filter:1, Coefficient: 5 */
CONVERT_COEFF(295408), /* Filter:1, Coefficient: 6 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(1486665), /* Filter:7, Coefficient: 7 */
CONVERT_COEFF(-1428619), /* Filter:7, Coefficient: 8 */
CONVERT_COEFF(-7757839), /* Filter:6, Coefficient: 7 */
CONVERT_COEFF(9692081), /* Filter:6, Coefficient: 8 */
CONVERT_COEFF(6958850), /* Filter:5, Coefficient: 7 */
CONVERT_COEFF(-11504364), /* Filter:5, Coefficient: 8 */
CONVERT_COEFF(8150817), /* Filter:4, Coefficient: 7 */
CONVERT_COEFF(-9406444), /* Filter:4, Coefficient: 8 */
CONVERT_COEFF(-3201273), /* Filter:3, Coefficient: 7 */
CONVERT_COEFF(7625293), /* Filter:3, Coefficient: 8 */
CONVERT_COEFF(-7084841), /* Filter:2, Coefficient: 7 */
CONVERT_COEFF(8963125), /* Filter:2, Coefficient: 8 */
CONVERT_COEFF(234925), /* Filter:1, Coefficient: 7 */
CONVERT_COEFF(-1212831), /* Filter:1, Coefficient: 8 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(1005001), /* Filter:7, Coefficient: 9 */
CONVERT_COEFF(-51772), /* Filter:7, Coefficient: 10 */
CONVERT_COEFF(-11095974), /* Filter:6, Coefficient: 9 */
CONVERT_COEFF(11326651), /* Filter:6, Coefficient: 10 */
CONVERT_COEFF(17367482), /* Filter:5, Coefficient: 9 */
CONVERT_COEFF(-24255226), /* Filter:5, Coefficient: 10 */
CONVERT_COEFF(9395942), /* Filter:4, Coefficient: 9 */
CONVERT_COEFF(-7240294), /* Filter:4, Coefficient: 10 */
CONVERT_COEFF(-14032432), /* Filter:3, Coefficient: 9 */
CONVERT_COEFF(22406684), /* Filter:3, Coefficient: 10 */
CONVERT_COEFF(-10179847), /* Filter:2, Coefficient: 9 */
CONVERT_COEFF(10023910), /* Filter:2, Coefficient: 10 */
CONVERT_COEFF(2728545), /* Filter:1, Coefficient: 9 */
CONVERT_COEFF(-4811819), /* Filter:1, Coefficient: 10 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-1592653), /* Filter:7, Coefficient: 11 */
CONVERT_COEFF(4060025), /* Filter:7, Coefficient: 12 */
CONVERT_COEFF(-9587117), /* Filter:6, Coefficient: 11 */
CONVERT_COEFF(4982353), /* Filter:6, Coefficient: 12 */
CONVERT_COEFF(31509193), /* Filter:5, Coefficient: 11 */
CONVERT_COEFF(-38039723), /* Filter:5, Coefficient: 12 */
CONVERT_COEFF(1973428), /* Filter:4, Coefficient: 11 */
CONVERT_COEFF(7334367), /* Filter:4, Coefficient: 12 */
CONVERT_COEFF(-32346679), /* Filter:3, Coefficient: 11 */
CONVERT_COEFF(42960394), /* Filter:3, Coefficient: 12 */
CONVERT_COEFF(-7657809), /* Filter:2, Coefficient: 11 */
CONVERT_COEFF(2202705), /* Filter:2, Coefficient: 12 */
CONVERT_COEFF(7398958), /* Filter:1, Coefficient: 11 */
CONVERT_COEFF(-10303827), /* Filter:1, Coefficient: 12 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-7418549), /* Filter:7, Coefficient: 13 */
CONVERT_COEFF(11634082), /* Filter:7, Coefficient: 14 */
CONVERT_COEFF(3391495), /* Filter:6, Coefficient: 13 */
CONVERT_COEFF(-16327279), /* Filter:6, Coefficient: 14 */
CONVERT_COEFF(42294622), /* Filter:5, Coefficient: 13 */
CONVERT_COEFF(-42269640), /* Filter:5, Coefficient: 14 */
CONVERT_COEFF(-21417196), /* Filter:4, Coefficient: 13 */
CONVERT_COEFF(40631722), /* Filter:4, Coefficient: 14 */
CONVERT_COEFF(-52793804), /* Filter:3, Coefficient: 13 */
CONVERT_COEFF(59804363), /* Filter:3, Coefficient: 14 */
CONVERT_COEFF(7145457), /* Filter:2, Coefficient: 13 */
CONVERT_COEFF(-20980397), /* Filter:2, Coefficient: 14 */
CONVERT_COEFF(13197765), /* Filter:1, Coefficient: 13 */
CONVERT_COEFF(-15601953), /* Filter:1, Coefficient: 14 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-16526119), /* Filter:7, Coefficient: 15 */
CONVERT_COEFF(21713991), /* Filter:7, Coefficient: 16 */
CONVERT_COEFF(34359416), /* Filter:6, Coefficient: 15 */
CONVERT_COEFF(-57585131), /* Filter:6, Coefficient: 16 */
CONVERT_COEFF(35559100), /* Filter:5, Coefficient: 15 */
CONVERT_COEFF(-19433648), /* Filter:5, Coefficient: 16 */
CONVERT_COEFF(-64768213), /* Filter:4, Coefficient: 15 */
CONVERT_COEFF(92865281), /* Filter:4, Coefficient: 16 */
CONVERT_COEFF(-61377672), /* Filter:3, Coefficient: 15 */
CONVERT_COEFF(54364939), /* Filter:3, Coefficient: 16 */
CONVERT_COEFF(39546773), /* Filter:2, Coefficient: 15 */
CONVERT_COEFF(-62615800), /* Filter:2, Coefficient: 16 */
CONVERT_COEFF(16892782), /* Filter:1, Coefficient: 15 */
CONVERT_COEFF(-16315753), /* Filter:1, Coefficient: 16 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-26537520), /* Filter:7, Coefficient: 17 */
CONVERT_COEFF(29908749), /* Filter:7, Coefficient: 18 */
CONVERT_COEFF(85425357), /* Filter:6, Coefficient: 17 */
CONVERT_COEFF(-116220246), /* Filter:6, Coefficient: 18 */
CONVERT_COEFF(-9079150), /* Filter:5, Coefficient: 17 */
CONVERT_COEFF(53136657), /* Filter:5, Coefficient: 18 */
CONVERT_COEFF(-123020997), /* Filter:4, Coefficient: 17 */
CONVERT_COEFF(152146065), /* Filter:4, Coefficient: 18 */
CONVERT_COEFF(-35083980), /* Filter:3, Coefficient: 17 */
CONVERT_COEFF(-839184), /* Filter:3, Coefficient: 18 */
CONVERT_COEFF(89388345), /* Filter:2, Coefficient: 17 */
CONVERT_COEFF(-118423648), /* Filter:2, Coefficient: 18 */
CONVERT_COEFF(12995329), /* Filter:1, Coefficient: 17 */
CONVERT_COEFF(-5913276), /* Filter:1, Coefficient: 18 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-29980490), /* Filter:7, Coefficient: 19 */
CONVERT_COEFF(23337872), /* Filter:7, Coefficient: 20 */
CONVERT_COEFF(146323990), /* Filter:6, Coefficient: 19 */
CONVERT_COEFF(-167617073), /* Filter:6, Coefficient: 20 */
CONVERT_COEFF(-116017890), /* Filter:5, Coefficient: 19 */
CONVERT_COEFF(200714461), /* Filter:5, Coefficient: 20 */
CONVERT_COEFF(-175480494), /* Filter:4, Coefficient: 19 */
CONVERT_COEFF(185308827), /* Filter:4, Coefficient: 20 */
CONVERT_COEFF(59061656), /* Filter:3, Coefficient: 19 */
CONVERT_COEFF(-148239694), /* Filter:3, Coefficient: 20 */
CONVERT_COEFF(147552056), /* Filter:2, Coefficient: 19 */
CONVERT_COEFF(-173608580), /* Filter:2, Coefficient: 20 */
CONVERT_COEFF(-6205575), /* Filter:1, Coefficient: 19 */
CONVERT_COEFF(25256070), /* Filter:1, Coefficient: 20 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-3217307), /* Filter:7, Coefficient: 21 */
CONVERT_COEFF(-37038723), /* Filter:7, Coefficient: 22 */
CONVERT_COEFF(160008482), /* Filter:6, Coefficient: 21 */
CONVERT_COEFF(-78261417), /* Filter:6, Coefficient: 22 */
CONVERT_COEFF(-307335294), /* Filter:5, Coefficient: 21 */
CONVERT_COEFF(424736994), /* Filter:5, Coefficient: 22 */
CONVERT_COEFF(-166691617), /* Filter:4, Coefficient: 21 */
CONVERT_COEFF(75780494), /* Filter:4, Coefficient: 22 */
CONVERT_COEFF(284938963), /* Filter:3, Coefficient: 21 */
CONVERT_COEFF(-508669005), /* Filter:3, Coefficient: 22 */
CONVERT_COEFF(191344927), /* Filter:2, Coefficient: 21 */
CONVERT_COEFF(-188272598), /* Filter:2, Coefficient: 22 */
CONVERT_COEFF(-54854236), /* Filter:1, Coefficient: 21 */
CONVERT_COEFF(104205542), /* Filter:1, Coefficient: 22 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(86353009), /* Filter:7, Coefficient: 23 */
CONVERT_COEFF(-97721939), /* Filter:7, Coefficient: 24 */
CONVERT_COEFF(-112362175), /* Filter:6, Coefficient: 23 */
CONVERT_COEFF(293242487), /* Filter:6, Coefficient: 24 */
CONVERT_COEFF(-438040128), /* Filter:5, Coefficient: 23 */
CONVERT_COEFF(198203604), /* Filter:5, Coefficient: 24 */
CONVERT_COEFF(306273762), /* Filter:4, Coefficient: 23 */
CONVERT_COEFF(-885042156), /* Filter:4, Coefficient: 24 */
CONVERT_COEFF(934119707), /* Filter:3, Coefficient: 23 */
CONVERT_COEFF(-607381991), /* Filter:3, Coefficient: 24 */
CONVERT_COEFF(107814255), /* Filter:2, Coefficient: 23 */
CONVERT_COEFF(1098624520), /* Filter:2, Coefficient: 24 */
CONVERT_COEFF(-207541862), /* Filter:1, Coefficient: 23 */
CONVERT_COEFF(676692041), /* Filter:1, Coefficient: 24 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(85978275), /* Filter:7, Coefficient: 25 */
CONVERT_COEFF(-36956143), /* Filter:7, Coefficient: 26 */
CONVERT_COEFF(-404403423), /* Filter:6, Coefficient: 25 */
CONVERT_COEFF(300207629), /* Filter:6, Coefficient: 26 */
CONVERT_COEFF(293327128), /* Filter:5, Coefficient: 25 */
CONVERT_COEFF(-521588890), /* Filter:5, Coefficient: 26 */
CONVERT_COEFF(844058593), /* Filter:4, Coefficient: 25 */
CONVERT_COEFF(-251982826), /* Filter:4, Coefficient: 26 */
CONVERT_COEFF(-603949609), /* Filter:3, Coefficient: 25 */
CONVERT_COEFF(929187271), /* Filter:3, Coefficient: 26 */
CONVERT_COEFF(-1099221720), /* Filter:2, Coefficient: 25 */
CONVERT_COEFF(-107131260), /* Filter:2, Coefficient: 26 */
CONVERT_COEFF(676692041), /* Filter:1, Coefficient: 25 */
CONVERT_COEFF(-207541862), /* Filter:1, Coefficient: 26 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-3244216), /* Filter:7, Coefficient: 27 */
CONVERT_COEFF(23345067), /* Filter:7, Coefficient: 28 */
CONVERT_COEFF(-140639902), /* Filter:6, Coefficient: 27 */
CONVERT_COEFF(27599531), /* Filter:6, Coefficient: 28 */
CONVERT_COEFF(444290293), /* Filter:5, Coefficient: 27 */
CONVERT_COEFF(-287297861), /* Filter:5, Coefficient: 28 */
CONVERT_COEFF(-139427582), /* Filter:4, Coefficient: 27 */
CONVERT_COEFF(221132516), /* Filter:4, Coefficient: 28 */
CONVERT_COEFF(-507498526), /* Filter:3, Coefficient: 27 */
CONVERT_COEFF(285983393), /* Filter:3, Coefficient: 28 */
CONVERT_COEFF(187466276), /* Filter:2, Coefficient: 27 */
CONVERT_COEFF(-190655157), /* Filter:2, Coefficient: 28 */
CONVERT_COEFF(104205542), /* Filter:1, Coefficient: 27 */
CONVERT_COEFF(-54854236), /* Filter:1, Coefficient: 28 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-29977934), /* Filter:7, Coefficient: 29 */
CONVERT_COEFF(29900921), /* Filter:7, Coefficient: 30 */
CONVERT_COEFF(33517333), /* Filter:6, Coefficient: 29 */
CONVERT_COEFF(-63177333), /* Filter:6, Coefficient: 30 */
CONVERT_COEFF(165959941), /* Filter:5, Coefficient: 29 */
CONVERT_COEFF(-79430320), /* Filter:5, Coefficient: 30 */
CONVERT_COEFF(-224053962), /* Filter:4, Coefficient: 29 */
CONVERT_COEFF(199362700), /* Filter:4, Coefficient: 30 */
CONVERT_COEFF(-150024800), /* Filter:3, Coefficient: 29 */
CONVERT_COEFF(60887712), /* Filter:3, Coefficient: 30 */
CONVERT_COEFF(173118469), /* Filter:2, Coefficient: 29 */
CONVERT_COEFF(-147250722), /* Filter:2, Coefficient: 30 */
CONVERT_COEFF(25256070), /* Filter:1, Coefficient: 29 */
CONVERT_COEFF(-6205575), /* Filter:1, Coefficient: 30 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-26527103), /* Filter:7, Coefficient: 31 */
CONVERT_COEFF(21702815), /* Filter:7, Coefficient: 32 */
CONVERT_COEFF(73741881), /* Filter:6, Coefficient: 31 */
CONVERT_COEFF(-72644258), /* Filter:6, Coefficient: 32 */
CONVERT_COEFF(20091574), /* Filter:5, Coefficient: 31 */
CONVERT_COEFF(18247620), /* Filter:5, Coefficient: 32 */
CONVERT_COEFF(-164221867), /* Filter:4, Coefficient: 31 */
CONVERT_COEFF(126509506), /* Filter:4, Coefficient: 32 */
CONVERT_COEFF(-2449324), /* Filter:3, Coefficient: 31 */
CONVERT_COEFF(-33779561), /* Filter:3, Coefficient: 32 */
CONVERT_COEFF(118271995), /* Filter:2, Coefficient: 31 */
CONVERT_COEFF(-89345383), /* Filter:2, Coefficient: 32 */
CONVERT_COEFF(-5913276), /* Filter:1, Coefficient: 31 */
CONVERT_COEFF(12995329), /* Filter:1, Coefficient: 32 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-16515450), /* Filter:7, Coefficient: 33 */
CONVERT_COEFF(11624746), /* Filter:7, Coefficient: 34 */
CONVERT_COEFF(64749828), /* Filter:6, Coefficient: 33 */
CONVERT_COEFF(-53438753), /* Filter:6, Coefficient: 34 */
CONVERT_COEFF(-40444649), /* Filter:5, Coefficient: 33 */
CONVERT_COEFF(50530543), /* Filter:5, Coefficient: 34 */
CONVERT_COEFF(-90610096), /* Filter:4, Coefficient: 33 */
CONVERT_COEFF(59102310), /* Filter:4, Coefficient: 34 */
CONVERT_COEFF(53381614), /* Filter:3, Coefficient: 33 */
CONVERT_COEFF(-60692487), /* Filter:3, Coefficient: 34 */
CONVERT_COEFF(62645405), /* Filter:2, Coefficient: 33 */
CONVERT_COEFF(-39619353), /* Filter:2, Coefficient: 34 */
CONVERT_COEFF(-16315753), /* Filter:1, Coefficient: 33 */
CONVERT_COEFF(16892782), /* Filter:1, Coefficient: 34 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-7410994), /* Filter:7, Coefficient: 35 */
CONVERT_COEFF(4054390), /* Filter:7, Coefficient: 36 */
CONVERT_COEFF(41090862), /* Filter:6, Coefficient: 35 */
CONVERT_COEFF(-29322557), /* Filter:6, Coefficient: 36 */
CONVERT_COEFF(-51969177), /* Filter:5, Coefficient: 35 */
CONVERT_COEFF(47732391), /* Filter:5, Coefficient: 36 */
CONVERT_COEFF(-33359574), /* Filter:4, Coefficient: 35 */
CONVERT_COEFF(13843008), /* Filter:4, Coefficient: 36 */
CONVERT_COEFF(59374033), /* Filter:3, Coefficient: 35 */
CONVERT_COEFF(-52565904), /* Filter:3, Coefficient: 36 */
CONVERT_COEFF(21073095), /* Filter:2, Coefficient: 35 */
CONVERT_COEFF(-7241772), /* Filter:2, Coefficient: 36 */
CONVERT_COEFF(-15601953), /* Filter:1, Coefficient: 35 */
CONVERT_COEFF(13197765), /* Filter:1, Coefficient: 36 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(-1588840), /* Filter:7, Coefficient: 37 */
CONVERT_COEFF(-54023), /* Filter:7, Coefficient: 38 */
CONVERT_COEFF(19130847), /* Filter:6, Coefficient: 37 */
CONVERT_COEFF(-11009977), /* Filter:6, Coefficient: 38 */
CONVERT_COEFF(-40290924), /* Filter:5, Coefficient: 37 */
CONVERT_COEFF(31587996), /* Filter:5, Coefficient: 38 */
CONVERT_COEFF(-316683), /* Filter:4, Coefficient: 37 */
CONVERT_COEFF(-7949710), /* Filter:4, Coefficient: 38 */
CONVERT_COEFF(42881174), /* Filter:3, Coefficient: 37 */
CONVERT_COEFF(-32366569), /* Filter:3, Coefficient: 38 */
CONVERT_COEFF(-2113613), /* Filter:2, Coefficient: 37 */
CONVERT_COEFF(7582042), /* Filter:2, Coefficient: 38 */
CONVERT_COEFF(-10303827), /* Filter:1, Coefficient: 37 */
CONVERT_COEFF(7398958), /* Filter:1, Coefficient: 38 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(1006037), /* Filter:7, Coefficient: 39 */
CONVERT_COEFF(-1428806), /* Filter:7, Coefficient: 40 */
CONVERT_COEFF(5064348), /* Filter:6, Coefficient: 39 */
CONVERT_COEFF(-1121566), /* Filter:6, Coefficient: 40 */
CONVERT_COEFF(-23032529), /* Filter:5, Coefficient: 39 */
CONVERT_COEFF(15527527), /* Filter:5, Coefficient: 40 */
CONVERT_COEFF(11982264), /* Filter:4, Coefficient: 39 */
CONVERT_COEFF(-12919307), /* Filter:4, Coefficient: 40 */
CONVERT_COEFF(22483813), /* Filter:3, Coefficient: 39 */
CONVERT_COEFF(-14134393), /* Filter:3, Coefficient: 40 */
CONVERT_COEFF(-9963872), /* Filter:2, Coefficient: 39 */
CONVERT_COEFF(10135303), /* Filter:2, Coefficient: 40 */
CONVERT_COEFF(-4811819), /* Filter:1, Coefficient: 39 */
CONVERT_COEFF(2728545), /* Filter:1, Coefficient: 40 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(1486340), /* Filter:7, Coefficient: 41 */
CONVERT_COEFF(-1325194), /* Filter:7, Coefficient: 42 */
CONVERT_COEFF(-1159486), /* Filter:6, Coefficient: 41 */
CONVERT_COEFF(2193279), /* Filter:6, Coefficient: 42 */
CONVERT_COEFF(-9534209), /* Filter:5, Coefficient: 41 */
CONVERT_COEFF(5163088), /* Filter:5, Coefficient: 42 */
CONVERT_COEFF(11857857), /* Filter:4, Coefficient: 41 */
CONVERT_COEFF(-9741790), /* Filter:4, Coefficient: 42 */
CONVERT_COEFF(7729413), /* Filter:3, Coefficient: 41 */
CONVERT_COEFF(-3293739), /* Filter:3, Coefficient: 42 */
CONVERT_COEFF(-8932187), /* Filter:2, Coefficient: 41 */
CONVERT_COEFF(7064806), /* Filter:2, Coefficient: 42 */
CONVERT_COEFF(-1212831), /* Filter:1, Coefficient: 41 */
CONVERT_COEFF(234925), /* Filter:1, Coefficient: 42 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(1062490), /* Filter:7, Coefficient: 43 */
CONVERT_COEFF(-781289), /* Filter:7, Coefficient: 44 */
CONVERT_COEFF(-2391815), /* Filter:6, Coefficient: 43 */
CONVERT_COEFF(2112274), /* Filter:6, Coefficient: 44 */
CONVERT_COEFF(-2278340), /* Filter:5, Coefficient: 43 */
CONVERT_COEFF(600969), /* Filter:5, Coefficient: 44 */
CONVERT_COEFF(7296922), /* Filter:4, Coefficient: 43 */
CONVERT_COEFF(-5011117), /* Filter:4, Coefficient: 44 */
CONVERT_COEFF(587429), /* Filter:3, Coefficient: 43 */
CONVERT_COEFF(773723), /* Filter:3, Coefficient: 44 */
CONVERT_COEFF(-5069022), /* Filter:2, Coefficient: 43 */
CONVERT_COEFF(3292866), /* Filter:2, Coefficient: 44 */
CONVERT_COEFF(295408), /* Filter:1, Coefficient: 43 */
CONVERT_COEFF(-496983), /* Filter:1, Coefficient: 44 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(531937), /* Filter:7, Coefficient: 45 */
CONVERT_COEFF(-337523), /* Filter:7, Coefficient: 46 */
CONVERT_COEFF(-1628548), /* Filter:6, Coefficient: 45 */
CONVERT_COEFF(1123843), /* Filter:6, Coefficient: 46 */
CONVERT_COEFF(201557), /* Filter:5, Coefficient: 45 */
CONVERT_COEFF(-447451), /* Filter:5, Coefficient: 46 */
CONVERT_COEFF(3151503), /* Filter:4, Coefficient: 45 */
CONVERT_COEFF(-1807037), /* Filter:4, Coefficient: 46 */
CONVERT_COEFF(-1214047), /* Filter:3, Coefficient: 45 */
CONVERT_COEFF(1120557), /* Filter:3, Coefficient: 46 */
CONVERT_COEFF(-1912448), /* Filter:2, Coefficient: 45 */
CONVERT_COEFF(967701), /* Filter:2, Coefficient: 46 */
CONVERT_COEFF(490498), /* Filter:1, Coefficient: 45 */
CONVERT_COEFF(-379589), /* Filter:1, Coefficient: 46 */

/* Filter #1, conversion from 48000 Hz to 48000 Hz */

CONVERT_COEFF(201449), /* Filter:7, Coefficient: 47 */
CONVERT_COEFF(-12084314), /* Filter:7, Coefficient: 48 */
CONVERT_COEFF(-699211), /* Filter:6, Coefficient: 47 */
CONVERT_COEFF(35988267), /* Filter:6, Coefficient: 48 */
CONVERT_COEFF(397902), /* Filter:5, Coefficient: 47 */
CONVERT_COEFF(-39316590), /* Filter:5, Coefficient: 48 */
CONVERT_COEFF(943390), /* Filter:4, Coefficient: 47 */
CONVERT_COEFF(18887301), /* Filter:4, Coefficient: 48 */
CONVERT_COEFF(-796401), /* Filter:3, Coefficient: 47 */
CONVERT_COEFF(-3816331), /* Filter:3, Coefficient: 48 */
CONVERT_COEFF(-407415), /* Filter:2, Coefficient: 47 */
CONVERT_COEFF(461439), /* Filter:2, Coefficient: 48 */
CONVERT_COEFF(240527), /* Filter:1, Coefficient: 47 */
CONVERT_COEFF(-119772)

};
