/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 24000 Hz to 16000 Hz */
/* NUM_FILTERS=6, FILTER_LENGTH=80, alpha=6.800000, gamma=0.460000 */

__cold_rodata static const int32_t coeff24000to16000[] = {

	/* Filter #1, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-26295), /* Filter:6, Coefficient: 1 */
	CONVERT_COEFF(-6174), /* Filter:6, Coefficient: 2 */
	CONVERT_COEFF(59137), /* Filter:6, Coefficient: 3 */
	CONVERT_COEFF(-55792), /* Filter:6, Coefficient: 4 */
	CONVERT_COEFF(95136), /* Filter:5, Coefficient: 1 */
	CONVERT_COEFF(-90674), /* Filter:5, Coefficient: 2 */
	CONVERT_COEFF(-78081), /* Filter:5, Coefficient: 3 */
	CONVERT_COEFF(262291), /* Filter:5, Coefficient: 4 */
	CONVERT_COEFF(20739), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(149775), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(-239278), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(-22446), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(-144563), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(217789), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(67657), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(-521235), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(-120977), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(-98594), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(393503), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(-206930), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(42248), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(-133707), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(38413), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(241342), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-44498), /* Filter:6, Coefficient: 5 */
	CONVERT_COEFF(140021), /* Filter:6, Coefficient: 6 */
	CONVERT_COEFF(-65538), /* Filter:6, Coefficient: 7 */
	CONVERT_COEFF(-159309), /* Filter:6, Coefficient: 8 */
	CONVERT_COEFF(-143888), /* Filter:5, Coefficient: 5 */
	CONVERT_COEFF(-302689), /* Filter:5, Coefficient: 6 */
	CONVERT_COEFF(539007), /* Filter:5, Coefficient: 7 */
	CONVERT_COEFF(-33282), /* Filter:5, Coefficient: 8 */
	CONVERT_COEFF(472981), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(-445555), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(-364304), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(1077686), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(409104), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(529589), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(-1203596), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(246572), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(-546538), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(894742), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(104473), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(-1568720), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(-302760), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(-155593), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(660489), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(-329457), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(250380), /* Filter:6, Coefficient: 9 */
	CONVERT_COEFF(11147), /* Filter:6, Coefficient: 10 */
	CONVERT_COEFF(-380641), /* Filter:6, Coefficient: 11 */
	CONVERT_COEFF(321305), /* Filter:6, Coefficient: 12 */
	CONVERT_COEFF(-810002), /* Filter:5, Coefficient: 9 */
	CONVERT_COEFF(794826), /* Filter:5, Coefficient: 10 */
	CONVERT_COEFF(490903), /* Filter:5, Coefficient: 11 */
	CONVERT_COEFF(-1598783), /* Filter:5, Coefficient: 12 */
	CONVERT_COEFF(-425238), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(-1277747), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(1786094), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(314731), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(1691746), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(-1890565), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(-897425), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(3576853), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(1232437), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(1350041), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(-3034301), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(596978), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(-766480), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(1172797), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(160492), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(-1874803), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(263297), /* Filter:6, Coefficient: 13 */
	CONVERT_COEFF(-680359), /* Filter:6, Coefficient: 14 */
	CONVERT_COEFF(215469), /* Filter:6, Coefficient: 15 */
	CONVERT_COEFF(752593), /* Filter:6, Coefficient: 16 */
	CONVERT_COEFF(693774), /* Filter:5, Coefficient: 13 */
	CONVERT_COEFF(1664970), /* Filter:5, Coefficient: 14 */
	CONVERT_COEFF(-2393909), /* Filter:5, Coefficient: 15 */
	CONVERT_COEFF(-270022), /* Filter:5, Coefficient: 16 */
	CONVERT_COEFF(-2867333), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(2015640), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(2321674), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(-4757264), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(-1759642), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(-3621615), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(5521571), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(433087), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(3906959), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(-4133829), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(-2063561), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(7454382), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(1336229), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(1573222), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(-3181847), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(419381), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-924495), /* Filter:6, Coefficient: 17 */
	CONVERT_COEFF(-243808), /* Filter:6, Coefficient: 18 */
	CONVERT_COEFF(1435511), /* Filter:6, Coefficient: 19 */
	CONVERT_COEFF(-854950), /* Filter:6, Coefficient: 20 */
	CONVERT_COEFF(3494050), /* Filter:5, Coefficient: 17 */
	CONVERT_COEFF(-2545027), /* Filter:5, Coefficient: 18 */
	CONVERT_COEFF(-2574960), /* Filter:5, Coefficient: 19 */
	CONVERT_COEFF(5494742), /* Filter:5, Coefficient: 20 */
	CONVERT_COEFF(788251), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(5808654), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(-5844798), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(-2965912), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(-7943091), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(5939150), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(5840605), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(-12672998), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(-3286678), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(-7450030), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(10475967), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(1477617), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(4031999), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(-3839814), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(-2330782), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(7001267), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-1191753), /* Filter:6, Coefficient: 21 */
	CONVERT_COEFF(2084337), /* Filter:6, Coefficient: 22 */
	CONVERT_COEFF(-128069), /* Filter:6, Coefficient: 23 */
	CONVERT_COEFF(-2601191), /* Filter:6, Coefficient: 24 */
	CONVERT_COEFF(-1086047), /* Filter:5, Coefficient: 21 */
	CONVERT_COEFF(-6323279), /* Filter:5, Coefficient: 22 */
	CONVERT_COEFF(6516645), /* Filter:5, Coefficient: 23 */
	CONVERT_COEFF(2986602), /* Filter:5, Coefficient: 24 */
	CONVERT_COEFF(10152857), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(-4315876), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(-9807613), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(13486977), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(2509829), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(14660956), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(-15044393), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(-7085983), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(-15070746), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(10141650), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(11626122), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(-22492705), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(-2520134), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(-7205710), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(9041722), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(2204328), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(2240070), /* Filter:6, Coefficient: 25 */
	CONVERT_COEFF(1583859), /* Filter:6, Coefficient: 26 */
	CONVERT_COEFF(-4145189), /* Filter:6, Coefficient: 27 */
	CONVERT_COEFF(1204705), /* Filter:6, Coefficient: 28 */
	CONVERT_COEFF(-10826352), /* Filter:5, Coefficient: 25 */
	CONVERT_COEFF(4740217), /* Filter:5, Coefficient: 26 */
	CONVERT_COEFF(10247700), /* Filter:5, Coefficient: 27 */
	CONVERT_COEFF(-14202210), /* Filter:5, Coefficient: 28 */
	CONVERT_COEFF(1961386), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(-19034008), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(12554808), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(14736870), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(25196436), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(-10843609), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(-24055393), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(33134987), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(2787191), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(26962307), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(-25014527), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(-15146780), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(-13501440), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(7856982), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(11265305), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(-19146543), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(4445165), /* Filter:6, Coefficient: 29 */
	CONVERT_COEFF(-5022270), /* Filter:6, Coefficient: 30 */
	CONVERT_COEFF(-1998803), /* Filter:6, Coefficient: 31 */
	CONVERT_COEFF(8226514), /* Filter:6, Coefficient: 32 */
	CONVERT_COEFF(-2184106), /* Filter:5, Coefficient: 29 */
	CONVERT_COEFF(20242040), /* Filter:5, Coefficient: 30 */
	CONVERT_COEFF(-12782630), /* Filter:5, Coefficient: 31 */
	CONVERT_COEFF(-17153461), /* Filter:5, Coefficient: 32 */
	CONVERT_COEFF(-27924414), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(2706581), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(34610785), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(-30620463), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(5074883), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(-47384059), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(30833566), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(38864431), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(44598021), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(-15639102), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(-46416323), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(58085501), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(581006), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(24589586), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(-20506417), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(-16259182), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-3439899), /* Filter:6, Coefficient: 33 */
	CONVERT_COEFF(-8925448), /* Filter:6, Coefficient: 34 */
	CONVERT_COEFF(11104297), /* Filter:6, Coefficient: 35 */
	CONVERT_COEFF(6265801), /* Filter:6, Coefficient: 36 */
	CONVERT_COEFF(30279913), /* Filter:5, Coefficient: 33 */
	CONVERT_COEFF(1264181), /* Filter:5, Coefficient: 34 */
	CONVERT_COEFF(-44000805), /* Filter:5, Coefficient: 35 */
	CONVERT_COEFF(27404677), /* Filter:5, Coefficient: 36 */
	CONVERT_COEFF(-22972428), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(60020710), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(-13003893), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(-79732586), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(-72814450), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(3812134), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(102230911), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(-87524750), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(16356030), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(-94361261), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(53986557), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(98938922), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(41141721), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(-11448663), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(-49636394), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(60678285), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-19358245), /* Filter:6, Coefficient: 37 */
	CONVERT_COEFF(-1448253), /* Filter:6, Coefficient: 38 */
	CONVERT_COEFF(26658987), /* Filter:6, Coefficient: 39 */
	CONVERT_COEFF(1437558), /* Filter:6, Coefficient: 40 */
	CONVERT_COEFF(51523005), /* Filter:5, Coefficient: 37 */
	CONVERT_COEFF(-60986563), /* Filter:5, Coefficient: 38 */
	CONVERT_COEFF(-58253729), /* Filter:5, Coefficient: 39 */
	CONVERT_COEFF(69683724), /* Filter:5, Coefficient: 40 */
	CONVERT_COEFF(76987483), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(99162074), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(-138037476), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(-114554861), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(-100746036), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(243760389), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(161666543), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(-338656732), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(-174705650), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(-35750635), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(498608183), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(308015331), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(26029323), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-140264601), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(104468300), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(595087393), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-20425535), /* Filter:6, Coefficient: 41 */
	CONVERT_COEFF(6567320), /* Filter:6, Coefficient: 42 */
	CONVERT_COEFF(15743139), /* Filter:6, Coefficient: 43 */
	CONVERT_COEFF(-9427264), /* Filter:6, Coefficient: 44 */
	CONVERT_COEFF(36693971), /* Filter:5, Coefficient: 41 */
	CONVERT_COEFF(-76752453), /* Filter:5, Coefficient: 42 */
	CONVERT_COEFF(-18978257), /* Filter:5, Coefficient: 43 */
	CONVERT_COEFF(58790681), /* Filter:5, Coefficient: 44 */
	CONVERT_COEFF(160865448), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(112183817), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(-122474297), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(-52614939), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(-258230261), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(238070373), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(183898488), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(-140152136), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(-427437883), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(-416558699), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(122106215), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(125076115), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(520991913), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(12457163), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(-124027598), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(56265475), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-7663681), /* Filter:6, Coefficient: 45 */
	CONVERT_COEFF(9874963), /* Filter:6, Coefficient: 46 */
	CONVERT_COEFF(564421), /* Filter:6, Coefficient: 47 */
	CONVERT_COEFF(-7491119), /* Filter:6, Coefficient: 48 */
	CONVERT_COEFF(-6293486), /* Filter:5, Coefficient: 45 */
	CONVERT_COEFF(-36428312), /* Filter:5, Coefficient: 46 */
	CONVERT_COEFF(22206365), /* Filter:5, Coefficient: 47 */
	CONVERT_COEFF(14384112), /* Filter:5, Coefficient: 48 */
	CONVERT_COEFF(87304224), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-5399444), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(-55884369), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(33711827), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(-42369119), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(106491355), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-28115935), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(-58576468), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(-125087622), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(-11228768), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(89164835), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(-43176336), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(37936440), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(-56171034), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(7138480), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(35072417), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(3743976), /* Filter:6, Coefficient: 49 */
	CONVERT_COEFF(3498372), /* Filter:6, Coefficient: 50 */
	CONVERT_COEFF(-4901576), /* Filter:6, Coefficient: 51 */
	CONVERT_COEFF(234077), /* Filter:6, Coefficient: 52 */
	CONVERT_COEFF(-24281078), /* Filter:5, Coefficient: 49 */
	CONVERT_COEFF(3132387), /* Filter:5, Coefficient: 50 */
	CONVERT_COEFF(16560249), /* Filter:5, Coefficient: 51 */
	CONVERT_COEFF(-12239715), /* Filter:5, Coefficient: 52 */
	CONVERT_COEFF(21599169), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(-37380560), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(5717759), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(25010112), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(53813676), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(11709605), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(-47320948), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(19538940), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-39017312), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(55027501), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(-3760701), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-39483388), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(-26074542), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(-10215709), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(25770583), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(-7934321), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(3638857), /* Filter:6, Coefficient: 53 */
	CONVERT_COEFF(-2392834), /* Filter:6, Coefficient: 54 */
	CONVERT_COEFF(-1352878), /* Filter:6, Coefficient: 55 */
	CONVERT_COEFF(2683085), /* Filter:6, Coefficient: 56 */
	CONVERT_COEFF(-5372325), /* Filter:5, Coefficient: 53 */
	CONVERT_COEFF(12708978), /* Filter:5, Coefficient: 54 */
	CONVERT_COEFF(-3609816), /* Filter:5, Coefficient: 55 */
	CONVERT_COEFF(-7612571), /* Filter:5, Coefficient: 56 */
	CONVERT_COEFF(-19441937), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-7452690), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(19552722), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-6222289), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(24367027), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(-29849864), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(-549202), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(23365978), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(26883932), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(13942943), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(-29246879), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(7153500), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(-14873710), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(15201245), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(2157693), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(-13047847), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-610388), /* Filter:6, Coefficient: 57 */
	CONVERT_COEFF(-1688412), /* Filter:6, Coefficient: 58 */
	CONVERT_COEFF(1513662), /* Filter:6, Coefficient: 59 */
	CONVERT_COEFF(349912), /* Filter:6, Coefficient: 60 */
	CONVERT_COEFF(7476439), /* Filter:5, Coefficient: 57 */
	CONVERT_COEFF(1248705), /* Filter:5, Coefficient: 58 */
	CONVERT_COEFF(-6509356), /* Filter:5, Coefficient: 59 */
	CONVERT_COEFF(3072466), /* Filter:5, Coefficient: 60 */
	CONVERT_COEFF(-11268706), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(11724570), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(1431814), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(-9841071), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(-13902547), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(-9448707), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(16453057), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(-2865391), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(18159363), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(-16617074), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(-3757540), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(14970543), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(6319609), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(6173527), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(-8607053), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(524563), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-1364557), /* Filter:6, Coefficient: 61 */
	CONVERT_COEFF(582053), /* Filter:6, Coefficient: 62 */
	CONVERT_COEFF(658744), /* Filter:6, Coefficient: 63 */
	CONVERT_COEFF(-838099), /* Filter:6, Coefficient: 64 */
	CONVERT_COEFF(3042256), /* Filter:5, Coefficient: 61 */
	CONVERT_COEFF(-4197678), /* Filter:5, Coefficient: 62 */
	CONVERT_COEFF(315404), /* Filter:5, Coefficient: 63 */
	CONVERT_COEFF(2919373), /* Filter:5, Coefficient: 64 */
	CONVERT_COEFF(4965896), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(4379351), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(-6407530), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(667422), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(-10736941), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(8712511), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(2733105), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(-8122284), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(-6264447), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(-7478462), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(9209549), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(-6635), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(6210778), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(-4146852), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(-2148993), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(4360107), /* Filter:1, Coefficient: 64 */

	/* Filter #17, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(34742), /* Filter:6, Coefficient: 65 */
	CONVERT_COEFF(585847), /* Filter:6, Coefficient: 66 */
	CONVERT_COEFF(-385112), /* Filter:6, Coefficient: 67 */
	CONVERT_COEFF(-177297), /* Filter:6, Coefficient: 68 */
	CONVERT_COEFF(-2028360), /* Filter:5, Coefficient: 65 */
	CONVERT_COEFF(-875673), /* Filter:5, Coefficient: 66 */
	CONVERT_COEFF(1992549), /* Filter:5, Coefficient: 67 */
	CONVERT_COEFF(-596942), /* Filter:5, Coefficient: 68 */
	CONVERT_COEFF(4308258), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(-3107701), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(-1215261), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(2940913), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(2901374), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(4157080), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(-4574441), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(-218386), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(-6756530), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(4038899), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(2447917), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(-4302235), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(-1020076), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(-2560492), /* Filter:1, Coefficient: 66 */
	CONVERT_COEFF(2237872), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(503503), /* Filter:1, Coefficient: 68 */

	/* Filter #18, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(376287), /* Filter:6, Coefficient: 69 */
	CONVERT_COEFF(-108531), /* Filter:6, Coefficient: 70 */
	CONVERT_COEFF(-186360), /* Filter:6, Coefficient: 71 */
	CONVERT_COEFF(184575), /* Filter:6, Coefficient: 72 */
	CONVERT_COEFF(-1027019), /* Filter:5, Coefficient: 69 */
	CONVERT_COEFF(1033388), /* Filter:5, Coefficient: 70 */
	CONVERT_COEFF(66645), /* Filter:5, Coefficient: 71 */
	CONVERT_COEFF(-722338), /* Filter:5, Coefficient: 72 */
	CONVERT_COEFF(-918864), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(-1474180), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(1499478), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(87118), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(3322300), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(-1807064), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(-1180769), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(1876974), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(815530), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(2473272), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(-1974352), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(-499153), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(-1850371), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(717835), /* Filter:1, Coefficient: 70 */
	CONVERT_COEFF(834686), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(-940634), /* Filter:1, Coefficient: 72 */

	/* Filter #19, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(7527), /* Filter:6, Coefficient: 73 */
	CONVERT_COEFF(-119787), /* Filter:6, Coefficient: 74 */
	CONVERT_COEFF(68147), /* Filter:6, Coefficient: 75 */
	CONVERT_COEFF(28404), /* Filter:6, Coefficient: 76 */
	CONVERT_COEFF(391591), /* Filter:5, Coefficient: 73 */
	CONVERT_COEFF(218433), /* Filter:5, Coefficient: 74 */
	CONVERT_COEFF(-369238), /* Filter:5, Coefficient: 75 */
	CONVERT_COEFF(93076), /* Filter:5, Coefficient: 76 */
	CONVERT_COEFF(-1021014), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(543575), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(306112), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(-496977), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(-320463), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(-991740), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(753601), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(156968), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(1548719), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(-552872), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(-629254), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(660050), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(-13456), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(592881), /* Filter:1, Coefficient: 74 */
	CONVERT_COEFF(-309498), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(-180123), /* Filter:1, Coefficient: 76 */

	/* Filter #20, conversion from 24000 Hz to 16000 Hz */

	CONVERT_COEFF(-55547), /* Filter:6, Coefficient: 77 */
	CONVERT_COEFF(19487), /* Filter:6, Coefficient: 78 */
	CONVERT_COEFF(15420), /* Filter:6, Coefficient: 79 */
	CONVERT_COEFF(-8783213), /* Filter:6, Coefficient: 80 */
	CONVERT_COEFF(151753), /* Filter:5, Coefficient: 77 */
	CONVERT_COEFF(-141825), /* Filter:5, Coefficient: 78 */
	CONVERT_COEFF(11411), /* Filter:5, Coefficient: 79 */
	CONVERT_COEFF(21748306), /* Filter:5, Coefficient: 80 */
	CONVERT_COEFF(114656), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(198467), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(-171608), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(-18242535), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(-504421), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(191996), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(140005), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(5774023), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(-3503), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(-338534), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(171077), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(-556806), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(261387), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(-35673), /* Filter:1, Coefficient: 78 */
	CONVERT_COEFF(-106078), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(60225), /* Filter:1, Coefficient: 80 */
};
