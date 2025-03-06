/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 44100 Hz to 48000 Hz */
/* NUM_FILTERS=7, FILTER_LENGTH=64, alpha=7.800000, gamma=0.459000 */

__cold_rodata static const int32_t coeff44100to48000[] = {

	/* Filter #1, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-36104), /* Filter:7, Coefficient: 1 */
	CONVERT_COEFF(50872), /* Filter:7, Coefficient: 2 */
	CONVERT_COEFF(-56564), /* Filter:7, Coefficient: 3 */
	CONVERT_COEFF(40229), /* Filter:7, Coefficient: 4 */
	CONVERT_COEFF(215090), /* Filter:6, Coefficient: 1 */
	CONVERT_COEFF(-343808), /* Filter:6, Coefficient: 2 */
	CONVERT_COEFF(497081), /* Filter:6, Coefficient: 3 */
	CONVERT_COEFF(-646143), /* Filter:6, Coefficient: 4 */
	CONVERT_COEFF(-224265), /* Filter:5, Coefficient: 1 */
	CONVERT_COEFF(396780), /* Filter:5, Coefficient: 2 */
	CONVERT_COEFF(-680396), /* Filter:5, Coefficient: 3 */
	CONVERT_COEFF(1099778), /* Filter:5, Coefficient: 4 */
	CONVERT_COEFF(-156406), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(276746), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(-402187), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(483398), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(48725), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(-150519), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(376286), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(-778623), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(140598), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(-270647), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(433819), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(-598685), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(12068), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(-294), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(-40869), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(127167), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(15714), /* Filter:7, Coefficient: 5 */
	CONVERT_COEFF(-132261), /* Filter:7, Coefficient: 6 */
	CONVERT_COEFF(331265), /* Filter:7, Coefficient: 7 */
	CONVERT_COEFF(-631432), /* Filter:7, Coefficient: 8 */
	CONVERT_COEFF(739353), /* Filter:6, Coefficient: 5 */
	CONVERT_COEFF(-699893), /* Filter:6, Coefficient: 6 */
	CONVERT_COEFF(427714), /* Filter:6, Coefficient: 7 */
	CONVERT_COEFF(192832), /* Filter:6, Coefficient: 8 */
	CONVERT_COEFF(-1660521), /* Filter:5, Coefficient: 5 */
	CONVERT_COEFF(2335311), /* Filter:5, Coefficient: 6 */
	CONVERT_COEFF(-3050136), /* Filter:5, Coefficient: 7 */
	CONVERT_COEFF(3673357), /* Filter:5, Coefficient: 8 */
	CONVERT_COEFF(-443096), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(177837), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(433397), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(-1512737), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(1399489), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(-2253057), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(3306250), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(-4459986), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(708253), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(-678784), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(404012), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(233922), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(-272876), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(486311), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(-764527), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(1087962), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(1043073), /* Filter:7, Coefficient: 9 */
	CONVERT_COEFF(-1562022), /* Filter:7, Coefficient: 10 */
	CONVERT_COEFF(2163475), /* Filter:7, Coefficient: 11 */
	CONVERT_COEFF(-2796684), /* Filter:7, Coefficient: 12 */
	CONVERT_COEFF(-1278823), /* Filter:6, Coefficient: 9 */
	CONVERT_COEFF(2928044), /* Filter:6, Coefficient: 10 */
	CONVERT_COEFF(-5192839), /* Filter:6, Coefficient: 11 */
	CONVERT_COEFF(8050849), /* Filter:6, Coefficient: 12 */
	CONVERT_COEFF(-4010927), /* Filter:5, Coefficient: 9 */
	CONVERT_COEFF(3811024), /* Filter:5, Coefficient: 10 */
	CONVERT_COEFF(-2780782), /* Filter:5, Coefficient: 11 */
	CONVERT_COEFF(616611), /* Filter:5, Coefficient: 12 */
	CONVERT_COEFF(3159157), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(-5417939), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(8246975), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(-11484454), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(5535069), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(-6267047), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(6314112), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(-5281074), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(-1346387), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(3013765), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(-5256640), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(8006079), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(-1416065), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(1685077), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(-1809078), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(1685203), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(3381591), /* Filter:7, Coefficient: 13 */
	CONVERT_COEFF(-3808499), /* Filter:7, Coefficient: 14 */
	CONVERT_COEFF(3941685), /* Filter:7, Coefficient: 15 */
	CONVERT_COEFF(-3627554), /* Filter:7, Coefficient: 16 */
	CONVERT_COEFF(-11376082), /* Filter:6, Coefficient: 13 */
	CONVERT_COEFF(14914588), /* Filter:6, Coefficient: 14 */
	CONVERT_COEFF(-18269373), /* Filter:6, Coefficient: 15 */
	CONVERT_COEFF(20898922), /* Filter:6, Coefficient: 16 */
	CONVERT_COEFF(2952090), /* Filter:5, Coefficient: 13 */
	CONVERT_COEFF(-8108091), /* Filter:5, Coefficient: 14 */
	CONVERT_COEFF(14884712), /* Filter:5, Coefficient: 15 */
	CONVERT_COEFF(-23103615), /* Filter:5, Coefficient: 16 */
	CONVERT_COEFF(14823547), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(-17800007), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(19798175), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(-20079410), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(2760719), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(1608543), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(-8072426), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(16684120), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(-11077473), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(14152940), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(-16777161), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(18370557), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(-1203455), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(260933), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(1220394), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(-3273956), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(2707504), /* Filter:7, Coefficient: 17 */
	CONVERT_COEFF(-1035138), /* Filter:7, Coefficient: 18 */
	CONVERT_COEFF(-1502971), /* Filter:7, Coefficient: 19 */
	CONVERT_COEFF(4964055), /* Filter:7, Coefficient: 20 */
	CONVERT_COEFF(-22132797), /* Filter:6, Coefficient: 17 */
	CONVERT_COEFF(21206202), /* Filter:6, Coefficient: 18 */
	CONVERT_COEFF(-17313533), /* Filter:6, Coefficient: 19 */
	CONVERT_COEFF(9678851), /* Filter:6, Coefficient: 20 */
	CONVERT_COEFF(32319346), /* Filter:5, Coefficient: 17 */
	CONVERT_COEFF(-41778175), /* Filter:5, Coefficient: 18 */
	CONVERT_COEFF(50397724), /* Filter:5, Coefficient: 19 */
	CONVERT_COEFF(-56771470), /* Filter:5, Coefficient: 20 */
	CONVERT_COEFF(17834747), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(-12260704), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(2654160), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(11480470), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(-27229456), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(39162945), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(-51563303), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(63114743), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(-18262033), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(15741168), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(-10127070), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(848490), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(5868998), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(-8893591), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(12142571), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(-15312249), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-9324436), /* Filter:7, Coefficient: 21 */
	CONVERT_COEFF(14453459), /* Filter:7, Coefficient: 22 */
	CONVERT_COEFF(-20084074), /* Filter:7, Coefficient: 23 */
	CONVERT_COEFF(25773112), /* Filter:7, Coefficient: 24 */
	CONVERT_COEFF(2360201), /* Filter:6, Coefficient: 21 */
	CONVERT_COEFF(-19258639), /* Filter:6, Coefficient: 22 */
	CONVERT_COEFF(41160997), /* Filter:6, Coefficient: 23 */
	CONVERT_COEFF(-67779055), /* Filter:6, Coefficient: 24 */
	CONVERT_COEFF(59198429), /* Filter:5, Coefficient: 21 */
	CONVERT_COEFF(-55733258), /* Filter:5, Coefficient: 22 */
	CONVERT_COEFF(44245892), /* Filter:5, Coefficient: 23 */
	CONVERT_COEFF(-22473133), /* Filter:5, Coefficient: 24 */
	CONVERT_COEFF(-30320143), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(53616304), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(-80596906), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(109873203), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(-72116055), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(76512776), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(-73937881), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(61731000), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(12472346), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(-29940877), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(51324643), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(-76006737), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(18002687), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(-19726749), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(19922792), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(-17964334), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-30833137), /* Filter:7, Coefficient: 25 */
	CONVERT_COEFF(34190653), /* Filter:7, Coefficient: 26 */
	CONVERT_COEFF(-34055766), /* Filter:7, Coefficient: 27 */
	CONVERT_COEFF(27111184), /* Filter:7, Coefficient: 28 */
	CONVERT_COEFF(98212836), /* Filter:6, Coefficient: 25 */
	CONVERT_COEFF(-130600585), /* Filter:6, Coefficient: 26 */
	CONVERT_COEFF(161254866), /* Filter:6, Coefficient: 27 */
	CONVERT_COEFF(-182201584), /* Filter:6, Coefficient: 28 */
	CONVERT_COEFF(-11960805), /* Filter:5, Coefficient: 25 */
	CONVERT_COEFF(61573102), /* Filter:5, Coefficient: 26 */
	CONVERT_COEFF(-129062026), /* Filter:5, Coefficient: 27 */
	CONVERT_COEFF(217007008), /* Filter:5, Coefficient: 28 */
	CONVERT_COEFF(-139327475), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(165912061), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(-185169687), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(189903320), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(-36878749), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(-4239017), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(66555533), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(-158147683), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(102957685), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(-130710897), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(157290278), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(-179922417), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(13153909), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(-4675683), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(-8550270), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(28262612), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-6732875), /* Filter:7, Coefficient: 29 */
	CONVERT_COEFF(-33513548), /* Filter:7, Coefficient: 30 */
	CONVERT_COEFF(81968959), /* Filter:7, Coefficient: 31 */
	CONVERT_COEFF(-126044370), /* Filter:7, Coefficient: 32 */
	CONVERT_COEFF(173711794), /* Filter:6, Coefficient: 29 */
	CONVERT_COEFF(-91263367), /* Filter:6, Coefficient: 30 */
	CONVERT_COEFF(-97880606), /* Filter:6, Coefficient: 31 */
	CONVERT_COEFF(376032608), /* Filter:6, Coefficient: 32 */
	CONVERT_COEFF(-325394901), /* Filter:5, Coefficient: 29 */
	CONVERT_COEFF(443482836), /* Filter:5, Coefficient: 30 */
	CONVERT_COEFF(-458466415), /* Filter:5, Coefficient: 31 */
	CONVERT_COEFF(111467000), /* Filter:5, Coefficient: 32 */
	CONVERT_COEFF(-165805866), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(69571438), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(317915452), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(-847016031), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(295236856), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(-517257770), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(938951919), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(-618933202), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(193893264), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(-187311577), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(104180214), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(1104501607), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(-57986908), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(106920163), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(-209369472), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(677292439), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(86233108), /* Filter:7, Coefficient: 33 */
	CONVERT_COEFF(-35501375), /* Filter:7, Coefficient: 34 */
	CONVERT_COEFF(-5636059), /* Filter:7, Coefficient: 35 */
	CONVERT_COEFF(26510404), /* Filter:7, Coefficient: 36 */
	CONVERT_COEFF(-406938858), /* Filter:6, Coefficient: 33 */
	CONVERT_COEFF(298668575), /* Filter:6, Coefficient: 34 */
	CONVERT_COEFF(-136959173), /* Filter:6, Coefficient: 35 */
	CONVERT_COEFF(21661015), /* Filter:6, Coefficient: 36 */
	CONVERT_COEFF(296331993), /* Filter:5, Coefficient: 33 */
	CONVERT_COEFF(-523052704), /* Filter:5, Coefficient: 34 */
	CONVERT_COEFF(446708617), /* Filter:5, Coefficient: 35 */
	CONVERT_COEFF(-290145245), /* Filter:5, Coefficient: 36 */
	CONVERT_COEFF(848040024), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(-256558412), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(-137639539), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(223543416), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(-605929650), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(936110335), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-517814517), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(297832194), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(-1104400882), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(-103375582), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(186432948), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(-193151946), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(677292439), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(-209369472), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(106920163), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(-57986908), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-33774390), /* Filter:7, Coefficient: 37 */
	CONVERT_COEFF(34125210), /* Filter:7, Coefficient: 38 */
	CONVERT_COEFF(-30912242), /* Filter:7, Coefficient: 39 */
	CONVERT_COEFF(25943030), /* Filter:7, Coefficient: 40 */
	CONVERT_COEFF(41951855), /* Filter:6, Coefficient: 37 */
	CONVERT_COEFF(-74109079), /* Filter:6, Coefficient: 38 */
	CONVERT_COEFF(86833071), /* Filter:6, Coefficient: 39 */
	CONVERT_COEFF(-87225838), /* Filter:6, Coefficient: 40 */
	CONVERT_COEFF(168023601), /* Filter:5, Coefficient: 37 */
	CONVERT_COEFF(-79384353), /* Filter:5, Coefficient: 38 */
	CONVERT_COEFF(16820075), /* Filter:5, Coefficient: 39 */
	CONVERT_COEFF(25433754), /* Filter:5, Coefficient: 40 */
	CONVERT_COEFF(-231084433), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(210602792), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(-178568996), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(142372278), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(-161325042), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(69605744), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(-6892281), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(-34721861), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(179395431), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(-156965780), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(130550114), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(-102919808), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(28262612), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-8550270), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(-4675683), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(13153909), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-20303032), /* Filter:7, Coefficient: 41 */
	CONVERT_COEFF(14689217), /* Filter:7, Coefficient: 42 */
	CONVERT_COEFF(-9553062), /* Filter:7, Coefficient: 43 */
	CONVERT_COEFF(5169062), /* Filter:7, Coefficient: 44 */
	CONVERT_COEFF(79900129), /* Filter:6, Coefficient: 41 */
	CONVERT_COEFF(-68107389), /* Filter:6, Coefficient: 42 */
	CONVERT_COEFF(54242139), /* Filter:6, Coefficient: 43 */
	CONVERT_COEFF(-40072964), /* Filter:6, Coefficient: 44 */
	CONVERT_COEFF(-51687526), /* Filter:5, Coefficient: 41 */
	CONVERT_COEFF(65404355), /* Filter:5, Coefficient: 42 */
	CONVERT_COEFF(-69552118), /* Filter:5, Coefficient: 43 */
	CONVERT_COEFF(66752497), /* Filter:5, Coefficient: 44 */
	CONVERT_COEFF(-106166575), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(72576377), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(-43273377), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(19208349), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(60089315), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(-72784419), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(75792430), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(-71757314), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(76055038), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(-51427903), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(30073627), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(-12614739), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(-17964334), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(19922792), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(-19726749), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(18002687), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-1674444), /* Filter:7, Coefficient: 45 */
	CONVERT_COEFF(-901531), /* Filter:7, Coefficient: 46 */
	CONVERT_COEFF(2611633), /* Filter:7, Coefficient: 47 */
	CONVERT_COEFF(-3566019), /* Filter:7, Coefficient: 48 */
	CONVERT_COEFF(26858516), /* Filter:6, Coefficient: 45 */
	CONVERT_COEFF(-15420460), /* Filter:6, Coefficient: 46 */
	CONVERT_COEFF(6205578), /* Filter:6, Coefficient: 47 */
	CONVERT_COEFF(650178), /* Filter:6, Coefficient: 48 */
	CONVERT_COEFF(-59316886), /* Filter:5, Coefficient: 45 */
	CONVERT_COEFF(49231067), /* Filter:5, Coefficient: 46 */
	CONVERT_COEFF(-38126705), /* Filter:5, Coefficient: 47 */
	CONVERT_COEFF(27261665), /* Filter:5, Coefficient: 48 */
	CONVERT_COEFF(-740031), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-12258104), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(20288296), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(-24113150), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(63038805), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(-51691475), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(39422440), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(-27557546), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(-711004), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(10004241), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(-15638587), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(18181880), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(-15312249), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(12142571), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(-8893591), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(5868998), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(3908988), /* Filter:7, Coefficient: 49 */
	CONVERT_COEFF(-3798121), /* Filter:7, Coefficient: 50 */
	CONVERT_COEFF(3386896), /* Filter:7, Coefficient: 51 */
	CONVERT_COEFF(-2811604), /* Filter:7, Coefficient: 52 */
	CONVERT_COEFF(-5252911), /* Filter:6, Coefficient: 49 */
	CONVERT_COEFF(7879560), /* Filter:6, Coefficient: 50 */
	CONVERT_COEFF(-8908772), /* Filter:6, Coefficient: 51 */
	CONVERT_COEFF(8758751), /* Filter:6, Coefficient: 52 */
	CONVERT_COEFF(-17520204), /* Filter:5, Coefficient: 49 */
	CONVERT_COEFF(9436366), /* Filter:5, Coefficient: 50 */
	CONVERT_COEFF(-3238487), /* Filter:5, Coefficient: 51 */
	CONVERT_COEFF(-1090753), /* Filter:5, Coefficient: 52 */
	CONVERT_COEFF(24640267), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(-22815396), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(19532216), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(-15564685), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(17030626), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(-8400622), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(1894708), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(2528815), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-18312401), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(16738756), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(-14130963), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(11068153), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(-3273956), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(1220394), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(260933), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(-1203455), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(2182958), /* Filter:7, Coefficient: 53 */
	CONVERT_COEFF(-1582255), /* Filter:7, Coefficient: 54 */
	CONVERT_COEFF(1061509), /* Filter:7, Coefficient: 55 */
	CONVERT_COEFF(-646664), /* Filter:7, Coefficient: 56 */
	CONVERT_COEFF(-7836150), /* Filter:6, Coefficient: 53 */
	CONVERT_COEFF(6498637), /* Filter:6, Coefficient: 54 */
	CONVERT_COEFF(-5031961), /* Filter:6, Coefficient: 55 */
	CONVERT_COEFF(3640700), /* Filter:6, Coefficient: 56 */
	CONVERT_COEFF(3746098), /* Filter:5, Coefficient: 53 */
	CONVERT_COEFF(-5030962), /* Filter:5, Coefficient: 54 */
	CONVERT_COEFF(5294951), /* Filter:5, Coefficient: 55 */
	CONVERT_COEFF(-4882739), /* Filter:5, Coefficient: 56 */
	CONVERT_COEFF(11524851), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-7845878), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(4787215), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-2456850), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(-5106362), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(6192755), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(-6190960), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(5494166), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(-8005697), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(5261877), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(-3021912), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(1355425), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(1685203), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-1809078), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(1685077), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(-1416065), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(342806), /* Filter:7, Coefficient: 57 */
	CONVERT_COEFF(-140284), /* Filter:7, Coefficient: 58 */
	CONVERT_COEFF(20786), /* Filter:7, Coefficient: 59 */
	CONVERT_COEFF(37371), /* Filter:7, Coefficient: 60 */
	CONVERT_COEFF(-2450499), /* Filter:6, Coefficient: 57 */
	CONVERT_COEFF(1518734), /* Filter:6, Coefficient: 58 */
	CONVERT_COEFF(-850198), /* Filter:6, Coefficient: 59 */
	CONVERT_COEFF(414533), /* Filter:6, Coefficient: 60 */
	CONVERT_COEFF(4097237), /* Filter:5, Coefficient: 57 */
	CONVERT_COEFF(-3177789), /* Filter:5, Coefficient: 58 */
	CONVERT_COEFF(2292208), /* Filter:5, Coefficient: 59 */
	CONVERT_COEFF(-1540003), /* Filter:5, Coefficient: 60 */
	CONVERT_COEFF(844451), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(140882), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(-633875), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(781606), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(-4443987), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(3305948), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(-2261053), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(1410461), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(-242505), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(-396647), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(672943), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(-703923), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(1087962), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(-764527), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(486311), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(-272876), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 44100 Hz to 48000 Hz */

	CONVERT_COEFF(-55188), /* Filter:7, Coefficient: 61 */
	CONVERT_COEFF(50357), /* Filter:7, Coefficient: 62 */
	CONVERT_COEFF(-35994), /* Filter:7, Coefficient: 63 */
	CONVERT_COEFF(18259534), /* Filter:7, Coefficient: 64 */
	CONVERT_COEFF(-162721), /* Filter:6, Coefficient: 61 */
	CONVERT_COEFF(40706), /* Filter:6, Coefficient: 62 */
	CONVERT_COEFF(881), /* Filter:6, Coefficient: 63 */
	CONVERT_COEFF(-52642287), /* Filter:6, Coefficient: 64 */
	CONVERT_COEFF(963380), /* Filter:5, Coefficient: 61 */
	CONVERT_COEFF(-562368), /* Filter:5, Coefficient: 62 */
	CONVERT_COEFF(310806), /* Filter:5, Coefficient: 63 */
	CONVERT_COEFF(55998086), /* Filter:5, Coefficient: 64 */
	CONVERT_COEFF(-719947), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(558884), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(-376234), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(-26993761), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(-789225), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(384817), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(-156478), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(5955995), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(595664), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(-431821), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(269381), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(-589635), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(127167), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(-40869), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(-294), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(12068), /* Filter:1, Coefficient: 64 */
};
