/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 48000 Hz to 24000 Hz */
/* NUM_FILTERS=5, FILTER_LENGTH=80, alpha=7.500000, gamma=0.440000 */

__cold_rodata static const int32_t coeff48000to24000[] = {

	/* Filter #1, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-13838), /* Filter:5, Coefficient: 1 */
	CONVERT_COEFF(-4878), /* Filter:5, Coefficient: 2 */
	CONVERT_COEFF(27346), /* Filter:5, Coefficient: 3 */
	CONVERT_COEFF(15920), /* Filter:5, Coefficient: 4 */
	CONVERT_COEFF(25265), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(-42584), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(-60777), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(71524), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(47328), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(43014), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(-104992), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(-127217), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(-2834), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(111653), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(50477), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(-231338), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(-41434), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(14488), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(121697), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(33752), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-45854), /* Filter:5, Coefficient: 5 */
	CONVERT_COEFF(-42865), /* Filter:5, Coefficient: 6 */
	CONVERT_COEFF(63570), /* Filter:5, Coefficient: 7 */
	CONVERT_COEFF(93651), /* Filter:5, Coefficient: 8 */
	CONVERT_COEFF(137676), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(-84397), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(-270825), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(41689), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(165291), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(305244), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(-174342), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(-598509), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(-207390), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(350931), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(536033), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(-368031), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(-237368), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(-187652), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(341275), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(495730), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-67575), /* Filter:5, Coefficient: 9 */
	CONVERT_COEFF(-172357), /* Filter:5, Coefficient: 10 */
	CONVERT_COEFF(37425), /* Filter:5, Coefficient: 11 */
	CONVERT_COEFF(273544), /* Filter:5, Coefficient: 12 */
	CONVERT_COEFF(458722), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(112302), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(-667913), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(-436742), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(41098), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(983891), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(349373), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(-1364077), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(-1063316), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(121078), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(1731952), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(580588), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(-335483), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(-966591), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(78327), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(1529224), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(52090), /* Filter:5, Coefficient: 13 */
	CONVERT_COEFF(-376509), /* Filter:5, Coefficient: 14 */
	CONVERT_COEFF(-224228), /* Filter:5, Coefficient: 15 */
	CONVERT_COEFF(441602), /* Filter:5, Coefficient: 16 */
	CONVERT_COEFF(820085), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(970404), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(-788941), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(-1699573), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(-1101019), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(1549721), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(2252110), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(-1267504), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(-2355777), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(-1892010), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(2600412), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(3840272), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(582559), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(-2002140), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(-1750602), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(2088832), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(489839), /* Filter:5, Coefficient: 17 */
	CONVERT_COEFF(-411054), /* Filter:5, Coefficient: 18 */
	CONVERT_COEFF(-834753), /* Filter:5, Coefficient: 19 */
	CONVERT_COEFF(216224), /* Filter:5, Coefficient: 20 */
	CONVERT_COEFF(413580), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(2524893), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(467752), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(-3237432), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(-3710975), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(204721), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(5200041), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(1907427), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(-2010349), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(-6224958), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(95035), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(8541845), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(3403760), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(-1414199), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(-5320802), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(-392742), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(1207772), /* Filter:5, Coefficient: 21 */
	CONVERT_COEFF(208047), /* Filter:5, Coefficient: 22 */
	CONVERT_COEFF(-1512464), /* Filter:5, Coefficient: 23 */
	CONVERT_COEFF(-902856), /* Filter:5, Coefficient: 24 */
	CONVERT_COEFF(-1964673), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(3514611), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(4069138), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(-2943356), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(-6229468), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(-5181060), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(6118238), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(9448813), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(3529031), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(-9961767), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(-8961434), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(9386380), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(7035593), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(3578394), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(-7842078), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(-8128913), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(1605200), /* Filter:5, Coefficient: 25 */
	CONVERT_COEFF(1860328), /* Filter:5, Coefficient: 26 */
	CONVERT_COEFF(-1299691), /* Filter:5, Coefficient: 27 */
	CONVERT_COEFF(-2995019), /* Filter:5, Coefficient: 28 */
	CONVERT_COEFF(-6587556), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(1071316), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(9078533), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(2520726), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(-4069677), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(-14153956), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(-708399), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(18265964), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(15841264), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(-5580394), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(-23206259), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(-2651769), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(6860332), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(13650089), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(-3152739), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(-19289299), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(374618), /* Filter:5, Coefficient: 29 */
	CONVERT_COEFF(4104773), /* Filter:5, Coefficient: 30 */
	CONVERT_COEFF(1423065), /* Filter:5, Coefficient: 31 */
	CONVERT_COEFF(-4786018), /* Filter:5, Coefficient: 32 */
	CONVERT_COEFF(-10790451), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(-8135144), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(10552917), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(15849886), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(8867184), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(-20196992), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(-20805992), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(17591439), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(29398714), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(16316208), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(-31955417), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(-36235851), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(-4149557), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(23701422), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(15790875), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(-24995516), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-4380083), /* Filter:5, Coefficient: 33 */
	CONVERT_COEFF(4154074), /* Filter:5, Coefficient: 34 */
	CONVERT_COEFF(8675375), /* Filter:5, Coefficient: 35 */
	CONVERT_COEFF(314997), /* Filter:5, Coefficient: 36 */
	CONVERT_COEFF(-6457951), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(-25205239), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(-5222436), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(33153210), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(36619130), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(-6512823), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-55966649), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(-22123969), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(27200860), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(63486136), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(-8378210), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(-101100781), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(-32577317), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(20405426), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(56329747), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(-4562349), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-11223389), /* Filter:5, Coefficient: 37 */
	CONVERT_COEFF(-9182954), /* Filter:5, Coefficient: 38 */
	CONVERT_COEFF(5881106), /* Filter:5, Coefficient: 39 */
	CONVERT_COEFF(14830455), /* Filter:5, Coefficient: 40 */
	CONVERT_COEFF(32686336), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(-17828627), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(-56842565), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(-29576137), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(75895153), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(107387729), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(3315284), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(-129456983), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(-44707785), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(159981825), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(284418268), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(144185861), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(-94322531), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-41673825), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(198691815), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(435480713), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(5793826), /* Filter:5, Coefficient: 41 */
	CONVERT_COEFF(-9270194), /* Filter:5, Coefficient: 42 */
	CONVERT_COEFF(-11239929), /* Filter:5, Coefficient: 43 */
	CONVERT_COEFF(350438), /* Filter:5, Coefficient: 44 */
	CONVERT_COEFF(33611108), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(54752705), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(12202297), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(-34504953), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(-132194211), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(-1313049), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(106654220), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(79294325), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(-144007288), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(-284533495), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(-160261654), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(44620548), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(435480713), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(198691815), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(-41673825), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(-94322531), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(8697892), /* Filter:5, Coefficient: 45 */
	CONVERT_COEFF(4139411), /* Filter:5, Coefficient: 46 */
	CONVERT_COEFF(-4400335), /* Filter:5, Coefficient: 47 */
	CONVERT_COEFF(-4782540), /* Filter:5, Coefficient: 48 */
	CONVERT_COEFF(-29508465), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(8636575), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(24014024), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(3272779), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(-19581710), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(-57247113), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-9049303), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(36450428), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(101282205), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(8546018), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(-63545873), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(-27357901), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(-4562349), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(56329747), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(20405426), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(-32577317), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(1438893), /* Filter:5, Coefficient: 49 */
	CONVERT_COEFF(4107485), /* Filter:5, Coefficient: 50 */
	CONVERT_COEFF(363683), /* Filter:5, Coefficient: 51 */
	CONVERT_COEFF(-3000681), /* Filter:5, Coefficient: 52 */
	CONVERT_COEFF(-16278067), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(-8279099), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(9318126), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(9464322), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(19409638), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(-19986418), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(-21274397), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(7861648), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(36215318), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(32067664), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(-16258232), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-29464286), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(-24995516), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(15790875), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(23701422), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(-4149557), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-1293262), /* Filter:5, Coefficient: 53 */
	CONVERT_COEFF(1866746), /* Filter:5, Coefficient: 54 */
	CONVERT_COEFF(1602403), /* Filter:5, Coefficient: 55 */
	CONVERT_COEFF(-908649), /* Filter:5, Coefficient: 56 */
	CONVERT_COEFF(-3897952), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-8522376), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(177415), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(6565670), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(18743832), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(224054), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(-14211714), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(-4803433), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(2584064), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(23233877), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(5641875), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(-15842519), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(-19289299), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-3152739), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(13650089), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(6860332), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-1512218), /* Filter:5, Coefficient: 57 */
	CONVERT_COEFF(212504), /* Filter:5, Coefficient: 58 */
	CONVERT_COEFF(1209017), /* Filter:5, Coefficient: 59 */
	CONVERT_COEFF(213290), /* Filter:5, Coefficient: 60 */
	CONVERT_COEFF(1976159), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(-4356506), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(-2866105), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(2379900), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(9257256), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(6616934), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(-4879764), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(-6512863), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(-9434058), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(8947402), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(9993781), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(-3508647), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(-8128913), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(-7842078), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(3578394), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(7035593), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-836606), /* Filter:5, Coefficient: 61 */
	CONVERT_COEFF(-409479), /* Filter:5, Coefficient: 62 */
	CONVERT_COEFF(491682), /* Filter:5, Coefficient: 63 */
	CONVERT_COEFF(441038), /* Filter:5, Coefficient: 64 */
	CONVERT_COEFF(2873370), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(-885436), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(-2375971), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(-64342), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(1595238), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(5317156), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(469755), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(-3719261), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(-8559856), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(-115583), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(6232362), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(2027557), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(-392742), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(-5320802), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(-1414199), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(3403760), /* Filter:1, Coefficient: 64 */

	/* Filter #17, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(-225715), /* Filter:5, Coefficient: 65 */
	CONVERT_COEFF(-376564), /* Filter:5, Coefficient: 66 */
	CONVERT_COEFF(53103), /* Filter:5, Coefficient: 67 */
	CONVERT_COEFF(273883), /* Filter:5, Coefficient: 68 */
	CONVERT_COEFF(1688791), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(534773), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(-1030744), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(-657535), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(-1461589), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(2203284), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(1673250), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(-1033553), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(-3840853), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(-2612954), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(1889068), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(2363811), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(2088832), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(-1750602), /* Filter:1, Coefficient: 66 */
	CONVERT_COEFF(-2002140), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(582559), /* Filter:1, Coefficient: 68 */

	/* Filter #18, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(36846), /* Filter:5, Coefficient: 69 */
	CONVERT_COEFF(-172746), /* Filter:5, Coefficient: 70 */
	CONVERT_COEFF(-67313), /* Filter:5, Coefficient: 71 */
	CONVERT_COEFF(93968), /* Filter:5, Coefficient: 72 */
	CONVERT_COEFF(519728), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(577634), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(-189254), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(-416848), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(-1430962), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(286656), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(1012557), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(88679), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(-576512), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(-1736423), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(-124869), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(1065395), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(1529224), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(78327), /* Filter:1, Coefficient: 70 */
	CONVERT_COEFF(-966591), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(-335483), /* Filter:1, Coefficient: 72 */

	/* Filter #19, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(63497), /* Filter:5, Coefficient: 73 */
	CONVERT_COEFF(-43069), /* Filter:5, Coefficient: 74 */
	CONVERT_COEFF(-45864), /* Filter:5, Coefficient: 75 */
	CONVERT_COEFF(16027), /* Filter:5, Coefficient: 76 */
	CONVERT_COEFF(16898), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(256277), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(45646), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(-135456), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(-605787), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(-205368), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(303359), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(183044), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(370923), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(-536759), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(-352848), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(207503), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(495730), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(341275), /* Filter:1, Coefficient: 74 */
	CONVERT_COEFF(-187652), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(-237368), /* Filter:1, Coefficient: 76 */

	/* Filter #20, conversion from 48000 Hz to 24000 Hz */

	CONVERT_COEFF(27375), /* Filter:5, Coefficient: 77 */
	CONVERT_COEFF(-4922), /* Filter:5, Coefficient: 78 */
	CONVERT_COEFF(-13857), /* Filter:5, Coefficient: 79 */
	CONVERT_COEFF(-763703), /* Filter:5, Coefficient: 80 */
	CONVERT_COEFF(-48618), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(62211), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(30113), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(1343965), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(-123289), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(-114093), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(40096), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(-641120), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(232473), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(-50406), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(-112271), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(102292), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(33752), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(121697), /* Filter:1, Coefficient: 78 */
	CONVERT_COEFF(14488), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(-41434), /* Filter:1, Coefficient: 80 */
};
