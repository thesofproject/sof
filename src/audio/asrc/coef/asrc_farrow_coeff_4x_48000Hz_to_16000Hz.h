/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 48000 Hz to 16000 Hz */
/* NUM_FILTERS=5, FILTER_LENGTH=96, alpha=5.700000, gamma=0.443000 */

__cold_rodata static const int32_t coeff48000to16000[] = {

	/* Filter #1, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(6003), /* Filter:5, Coefficient: 1 */
	CONVERT_COEFF(-2286), /* Filter:5, Coefficient: 2 */
	CONVERT_COEFF(-11917), /* Filter:5, Coefficient: 3 */
	CONVERT_COEFF(-12263), /* Filter:5, Coefficient: 4 */
	CONVERT_COEFF(16701), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(40694), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(27405), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(-25736), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(-56844), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(26944), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(132074), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(141901), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(-147894), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(-187545), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(-21075), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(277248), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(14698), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(-167333), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(-289523), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(-163034), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(1010), /* Filter:5, Coefficient: 5 */
	CONVERT_COEFF(18750), /* Filter:5, Coefficient: 6 */
	CONVERT_COEFF(23487), /* Filter:5, Coefficient: 7 */
	CONVERT_COEFF(5356), /* Filter:5, Coefficient: 8 */
	CONVERT_COEFF(-75665), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(-64460), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(21621), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(120266), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(-5491), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(-220739), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(-298783), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(-96954), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(434836), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(201545), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(-357476), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(-796006), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(218113), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(572796), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(507886), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(-103264), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-24865), /* Filter:5, Coefficient: 9 */
	CONVERT_COEFF(-40246), /* Filter:5, Coefficient: 10 */
	CONVERT_COEFF(-20333), /* Filter:5, Coefficient: 11 */
	CONVERT_COEFF(26070), /* Filter:5, Coefficient: 12 */
	CONVERT_COEFF(132288), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(13456), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(-160847), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(-233861), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(286982), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(528042), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(330026), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(-261850), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(-608575), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(261248), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(1195810), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(1292859), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(-870589), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(-1084744), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(-322241), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(1022402), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(61074), /* Filter:5, Coefficient: 13 */
	CONVERT_COEFF(47049), /* Filter:5, Coefficient: 14 */
	CONVERT_COEFF(-16265), /* Filter:5, Coefficient: 15 */
	CONVERT_COEFF(-81433), /* Filter:5, Coefficient: 16 */
	CONVERT_COEFF(-100488), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(171929), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(359676), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(257462), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(-794371), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(-728986), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(53574), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(1014174), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(174255), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(-1469546), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(-2223834), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(-1105863), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(1845595), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(1186048), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(-793496), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(-2620309), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-86732), /* Filter:5, Coefficient: 17 */
	CONVERT_COEFF(-11725), /* Filter:5, Coefficient: 18 */
	CONVERT_COEFF(93077), /* Filter:5, Coefficient: 19 */
	CONVERT_COEFF(136905), /* Filter:5, Coefficient: 20 */
	CONVERT_COEFF(-117314), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(-482097), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(-490512), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(-44622), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(1290134), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(433539), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(-1052827), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(-1945766), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(1365285), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(3245541), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(2622791), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(-576159), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(-2535935), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(-84562), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(3100653), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(4273125), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(64518), /* Filter:5, Coefficient: 21 */
	CONVERT_COEFF(-84123), /* Filter:5, Coefficient: 22 */
	CONVERT_COEFF(-189618), /* Filter:5, Coefficient: 23 */
	CONVERT_COEFF(-145818), /* Filter:5, Coefficient: 24 */
	CONVERT_COEFF(552271), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(783867), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(351343), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(-501169), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(-1269113), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(735909), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(2545279), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(2459933), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(-4050257), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(-4676104), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(-1197064), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(4181701), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(1843458), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(-2859084), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(-6099452), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(-4589449), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(40024), /* Filter:5, Coefficient: 25 */
	CONVERT_COEFF(229992), /* Filter:5, Coefficient: 26 */
	CONVERT_COEFF(253530), /* Filter:5, Coefficient: 27 */
	CONVERT_COEFF(54594), /* Filter:5, Coefficient: 28 */
	CONVERT_COEFF(-1090434), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(-823146), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(245247), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(1324309), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(124819), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(-2847668), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(-3912612), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(-1694303), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(7014764), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(4162168), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(-3070757), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(-9141382), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(1405180), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(7494249), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(8215482), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(1730865), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-235096), /* Filter:5, Coefficient: 29 */
	CONVERT_COEFF(-376320), /* Filter:5, Coefficient: 30 */
	CONVERT_COEFF(-213387), /* Filter:5, Coefficient: 31 */
	CONVERT_COEFF(172458), /* Filter:5, Coefficient: 32 */
	CONVERT_COEFF(1448025), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(304353), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(-1354202), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(-2164152), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(2525824), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(5399928), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(4073459), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(-1173325), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(-8346879), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(91892), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(10287607), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(13522720), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(-7725810), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(-12333767), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(-6913819), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(5879577), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(488565), /* Filter:5, Coefficient: 33 */
	CONVERT_COEFF(444779), /* Filter:5, Coefficient: 34 */
	CONVERT_COEFF(3934), /* Filter:5, Coefficient: 35 */
	CONVERT_COEFF(-538903), /* Filter:5, Coefficient: 36 */
	CONVERT_COEFF(-1231572), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(991042), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(2832294), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(2605690), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(-6524989), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(-7262601), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-1715100), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(6645311), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(5393659), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(-9375299), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(-19144348), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(-14082933), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(16237057), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(14362523), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(-839543), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(-18862506), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-743652), /* Filter:5, Coefficient: 37 */
	CONVERT_COEFF(-364983), /* Filter:5, Coefficient: 38 */
	CONVERT_COEFF(412423), /* Filter:5, Coefficient: 39 */
	CONVERT_COEFF(1049680), /* Filter:5, Coefficient: 40 */
	CONVERT_COEFF(51807), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(-3160575), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(-4453939), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(-2290054), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(11124179), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(6869327), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(-4598379), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(-15255006), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(4836350), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(24247448), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(27060755), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(6196430), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(-24233009), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-8964202), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(18626760), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(37047113), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(1021779), /* Filter:5, Coefficient: 41 */
	CONVERT_COEFF(223192), /* Filter:5, Coefficient: 42 */
	CONVERT_COEFF(-900926), /* Filter:5, Coefficient: 43 */
	CONVERT_COEFF(-1626034), /* Filter:5, Coefficient: 44 */
	CONVERT_COEFF(2394425), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(6494128), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(6785094), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(2350160), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(-15741799), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(-2587077), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(17884441), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(32479337), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(-26945239), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(-47161583), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(-32017732), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(20427044), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(26747798), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(-12522864), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(-55553443), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(-63801692), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-1450725), /* Filter:5, Coefficient: 45 */
	CONVERT_COEFF(-470498), /* Filter:5, Coefficient: 46 */
	CONVERT_COEFF(661749), /* Filter:5, Coefficient: 47 */
	CONVERT_COEFF(1177778), /* Filter:5, Coefficient: 48 */
	CONVERT_COEFF(-4430794), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-9217671), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(-8589764), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(-2482951), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(29620390), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(7771362), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-22319484), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(-43669888), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(85909205), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(126153193), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(112402280), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(44979247), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(-10171046), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(99475669), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(223708992), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(305859586), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(729289), /* Filter:5, Coefficient: 49 */
	CONVERT_COEFF(-368490), /* Filter:5, Coefficient: 50 */
	CONVERT_COEFF(-1360978), /* Filter:5, Coefficient: 51 */
	CONVERT_COEFF(-1582611), /* Filter:5, Coefficient: 52 */
	CONVERT_COEFF(5588041), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(10764075), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(10021818), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(4102867), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(-43706637), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(-22383172), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(7781413), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(29772819), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-44758225), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(-112244374), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(-126089107), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-85924595), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(305859586), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(223708992), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(99475669), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(-10171046), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-908464), /* Filter:5, Coefficient: 53 */
	CONVERT_COEFF(187247), /* Filter:5, Coefficient: 54 */
	CONVERT_COEFF(989311), /* Filter:5, Coefficient: 55 */
	CONVERT_COEFF(1041404), /* Filter:5, Coefficient: 56 */
	CONVERT_COEFF(-3113458), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-7286202), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(-6424181), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-1920843), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(32745687), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(18147500), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(-2455994), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(-15794058), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(-20476276), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(31981862), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(47161892), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(26973318), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(-63801692), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-55553443), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(-12522864), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(26747798), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(427927), /* Filter:5, Coefficient: 57 */
	CONVERT_COEFF(-342034), /* Filter:5, Coefficient: 58 */
	CONVERT_COEFF(-731079), /* Filter:5, Coefficient: 59 */
	CONVERT_COEFF(-543506), /* Filter:5, Coefficient: 60 */
	CONVERT_COEFF(2748904), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(4570973), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(2912804), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(-421780), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(-15430280), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(-4768911), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(6810261), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(11194720), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(-6166649), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(-27051114), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(-24261125), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(-4859188), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(37047113), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(18626760), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(-8964202), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(-24233009), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-11110), /* Filter:5, Coefficient: 61 */
	CONVERT_COEFF(432181), /* Filter:5, Coefficient: 62 */
	CONVERT_COEFF(487097), /* Filter:5, Coefficient: 63 */
	CONVERT_COEFF(181137), /* Filter:5, Coefficient: 64 */
	CONVERT_COEFF(-2809542), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(-2751305), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(-733429), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(1447308), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(6774473), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(-1626617), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(-7269584), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(-6605985), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(14069130), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(19148004), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(9390672), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(-5379859), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(-18862506), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(-839543), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(14362523), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(16237057), /* Filter:1, Coefficient: 64 */

	/* Filter #17, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-202750), /* Filter:5, Coefficient: 65 */
	CONVERT_COEFF(-371867), /* Filter:5, Coefficient: 66 */
	CONVERT_COEFF(-238987), /* Filter:5, Coefficient: 67 */
	CONVERT_COEFF(46710), /* Filter:5, Coefficient: 68 */
	CONVERT_COEFF(2187189), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(1200707), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(-491006), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(-1524430), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(-1257592), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(4047456), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(5441306), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(2592862), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(-13520338), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(-10296413), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(-103461), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(8341557), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(5879577), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(-6913819), /* Filter:1, Coefficient: 66 */
	CONVERT_COEFF(-12333767), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(-7725810), /* Filter:1, Coefficient: 68 */

	/* Filter #18, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(248164), /* Filter:5, Coefficient: 69 */
	CONVERT_COEFF(230638), /* Filter:5, Coefficient: 70 */
	CONVERT_COEFF(45115), /* Filter:5, Coefficient: 71 */
	CONVERT_COEFF(-140841), /* Filter:5, Coefficient: 72 */
	CONVERT_COEFF(-1253197), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(-105103), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(916311), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(1076028), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(-1655380), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(-3925882), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(-2894376), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(85264), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(9145142), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(3079217), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(-4156100), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(-7015142), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(1730865), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(8215482), /* Filter:1, Coefficient: 70 */
	CONVERT_COEFF(7494249), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(1405180), /* Filter:1, Coefficient: 72 */

	/* Filter #19, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-188394), /* Filter:5, Coefficient: 73 */
	CONVERT_COEFF(-86846), /* Filter:5, Coefficient: 74 */
	CONVERT_COEFF(60603), /* Filter:5, Coefficient: 75 */
	CONVERT_COEFF(134904), /* Filter:5, Coefficient: 76 */
	CONVERT_COEFF(409479), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(-438001), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(-802163), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(-501779), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(2455894), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(2573565), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(769517), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(-1256461), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(-4187065), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(1191611), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(4674610), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(4053062), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(-4589449), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(-6099452), /* Filter:1, Coefficient: 74 */
	CONVERT_COEFF(-2859084), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(1843458), /* Filter:1, Coefficient: 76 */

	/* Filter #20, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(94077), /* Filter:5, Coefficient: 77 */
	CONVERT_COEFF(-9066), /* Filter:5, Coefficient: 78 */
	CONVERT_COEFF(-84702), /* Filter:5, Coefficient: 79 */
	CONVERT_COEFF(-81382), /* Filter:5, Coefficient: 80 */
	CONVERT_COEFF(112941), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(522408), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(461411), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(70472), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(-1959787), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(-1077960), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(418428), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(1294567), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(580341), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(-2620598), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(-3246544), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(-1368066), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(4273125), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(3100653), /* Filter:1, Coefficient: 78 */
	CONVERT_COEFF(-84562), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(-2535935), /* Filter:1, Coefficient: 80 */

	/* Filter #21, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-17797), /* Filter:5, Coefficient: 81 */
	CONVERT_COEFF(45404), /* Filter:5, Coefficient: 82 */
	CONVERT_COEFF(60538), /* Filter:5, Coefficient: 83 */
	CONVERT_COEFF(26766), /* Filter:5, Coefficient: 84 */
	CONVERT_COEFF(-290075), /* Filter:4, Coefficient: 81 */
	CONVERT_COEFF(-357062), /* Filter:4, Coefficient: 82 */
	CONVERT_COEFF(-144107), /* Filter:4, Coefficient: 83 */
	CONVERT_COEFF(126933), /* Filter:4, Coefficient: 84 */
	CONVERT_COEFF(1030932), /* Filter:3, Coefficient: 81 */
	CONVERT_COEFF(67431), /* Filter:3, Coefficient: 82 */
	CONVERT_COEFF(-727987), /* Filter:3, Coefficient: 83 */
	CONVERT_COEFF(-804281), /* Filter:3, Coefficient: 84 */
	CONVERT_COEFF(1103742), /* Filter:2, Coefficient: 81 */
	CONVERT_COEFF(2223786), /* Filter:2, Coefficient: 82 */
	CONVERT_COEFF(1471128), /* Filter:2, Coefficient: 83 */
	CONVERT_COEFF(-172596), /* Filter:2, Coefficient: 84 */
	CONVERT_COEFF(-2620309), /* Filter:1, Coefficient: 81 */
	CONVERT_COEFF(-793496), /* Filter:1, Coefficient: 82 */
	CONVERT_COEFF(1186048), /* Filter:1, Coefficient: 83 */
	CONVERT_COEFF(1845595), /* Filter:1, Coefficient: 84 */

	/* Filter #22, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-19212), /* Filter:5, Coefficient: 85 */
	CONVERT_COEFF(-39619), /* Filter:5, Coefficient: 86 */
	CONVERT_COEFF(-25052), /* Filter:5, Coefficient: 87 */
	CONVERT_COEFF(4714), /* Filter:5, Coefficient: 88 */
	CONVERT_COEFF(239663), /* Filter:4, Coefficient: 85 */
	CONVERT_COEFF(146928), /* Filter:4, Coefficient: 86 */
	CONVERT_COEFF(-31586), /* Filter:4, Coefficient: 87 */
	CONVERT_COEFF(-139998), /* Filter:4, Coefficient: 88 */
	CONVERT_COEFF(-272737), /* Filter:3, Coefficient: 85 */
	CONVERT_COEFF(326716), /* Filter:3, Coefficient: 86 */
	CONVERT_COEFF(533127), /* Filter:3, Coefficient: 87 */
	CONVERT_COEFF(294587), /* Filter:3, Coefficient: 88 */
	CONVERT_COEFF(-1292361), /* Filter:2, Coefficient: 85 */
	CONVERT_COEFF(-1196542), /* Filter:2, Coefficient: 86 */
	CONVERT_COEFF(-262345), /* Filter:2, Coefficient: 87 */
	CONVERT_COEFF(608020), /* Filter:2, Coefficient: 88 */
	CONVERT_COEFF(1022402), /* Filter:1, Coefficient: 85 */
	CONVERT_COEFF(-322241), /* Filter:1, Coefficient: 86 */
	CONVERT_COEFF(-1084744), /* Filter:1, Coefficient: 87 */
	CONVERT_COEFF(-870589), /* Filter:1, Coefficient: 88 */

	/* Filter #23, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(22982), /* Filter:5, Coefficient: 89 */
	CONVERT_COEFF(18703), /* Filter:5, Coefficient: 90 */
	CONVERT_COEFF(1310), /* Filter:5, Coefficient: 91 */
	CONVERT_COEFF(-11946), /* Filter:5, Coefficient: 92 */
	CONVERT_COEFF(-114763), /* Filter:4, Coefficient: 89 */
	CONVERT_COEFF(-10932), /* Filter:4, Coefficient: 90 */
	CONVERT_COEFF(70685), /* Filter:4, Coefficient: 91 */
	CONVERT_COEFF(74147), /* Filter:4, Coefficient: 92 */
	CONVERT_COEFF(-93308), /* Filter:3, Coefficient: 89 */
	CONVERT_COEFF(-300945), /* Filter:3, Coefficient: 90 */
	CONVERT_COEFF(-225549), /* Filter:3, Coefficient: 91 */
	CONVERT_COEFF(-8488), /* Filter:3, Coefficient: 92 */
	CONVERT_COEFF(796245), /* Filter:2, Coefficient: 89 */
	CONVERT_COEFF(358093), /* Filter:2, Coefficient: 90 */
	CONVERT_COEFF(-201126), /* Filter:2, Coefficient: 91 */
	CONVERT_COEFF(-434862), /* Filter:2, Coefficient: 92 */
	CONVERT_COEFF(-103264), /* Filter:1, Coefficient: 89 */
	CONVERT_COEFF(507886), /* Filter:1, Coefficient: 90 */
	CONVERT_COEFF(572796), /* Filter:1, Coefficient: 91 */
	CONVERT_COEFF(218113), /* Filter:1, Coefficient: 92 */

	/* Filter #24, conversion from 48000 Hz to 16000 Hz */

	CONVERT_COEFF(-11816), /* Filter:5, Coefficient: 93 */
	CONVERT_COEFF(-2391), /* Filter:5, Coefficient: 94 */
	CONVERT_COEFF(5852), /* Filter:5, Coefficient: 95 */
	CONVERT_COEFF(2789237), /* Filter:5, Coefficient: 96 */
	CONVERT_COEFF(20271), /* Filter:4, Coefficient: 93 */
	CONVERT_COEFF(-31141), /* Filter:4, Coefficient: 94 */
	CONVERT_COEFF(-40353), /* Filter:4, Coefficient: 95 */
	CONVERT_COEFF(-5145454), /* Filter:4, Coefficient: 96 */
	CONVERT_COEFF(142594), /* Filter:3, Coefficient: 93 */
	CONVERT_COEFF(134883), /* Filter:3, Coefficient: 94 */
	CONVERT_COEFF(29007), /* Filter:3, Coefficient: 95 */
	CONVERT_COEFF(2574030), /* Filter:3, Coefficient: 96 */
	CONVERT_COEFF(-277542), /* Filter:2, Coefficient: 93 */
	CONVERT_COEFF(20836), /* Filter:2, Coefficient: 94 */
	CONVERT_COEFF(187526), /* Filter:2, Coefficient: 95 */
	CONVERT_COEFF(-232512), /* Filter:2, Coefficient: 96 */
	CONVERT_COEFF(-163034), /* Filter:1, Coefficient: 93 */
	CONVERT_COEFF(-289523), /* Filter:1, Coefficient: 94 */
	CONVERT_COEFF(-167333), /* Filter:1, Coefficient: 95 */
	CONVERT_COEFF(14698), /* Filter:1, Coefficient: 96 */
};
