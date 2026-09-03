/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2019 Intel Corporation. All rights reserved.
 */

/* Conversion from 24000 Hz to 8000 Hz */
/* NUM_FILTERS=4, FILTER_LENGTH=128, alpha=6.200000, gamma=0.454000 */

__cold_rodata static const int32_t coeff24000to08000[] = {
	/* Filter #4, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-10830), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(-19325), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(41187), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(5433), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(53742), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(102764), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(-40910), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(43191), /* Filter:1, Coefficient: 2 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-8706), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(17158), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(-57507), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(-85177), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(56825), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(-81566), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(132068), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(122684), /* Filter:1, Coefficient: 4 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(35023), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(21531), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(-28687), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(84875), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(-198613), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(-152429), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(-26903), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(-219188), /* Filter:1, Coefficient: 6 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-20607), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(-56412), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(153388), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(85079), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(77423), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(318659), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(-265221), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(-55019), /* Filter:1, Coefficient: 8 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-45627), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(16004), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(-97650), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(-243132), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(320857), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(-4716), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(292318), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(469916), /* Filter:1, Coefficient: 10 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(80964), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(83822), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(-188414), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(74163), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(-436161), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(-569989), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(238081), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(-305542), /* Filter:1, Coefficient: 12 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(3428), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(-103074), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(342026), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(348018), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(-178871), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(504223), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(-717573), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(-551012), /* Filter:1, Coefficient: 14 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-136759), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(-45156), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(12595), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(-425480), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(887721), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(512609), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(198162), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(961755), /* Filter:1, Coefficient: 16 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(113706), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(201400), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(-563391), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(-190843), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(-456956), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(-1233622), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(1003766), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(97129), /* Filter:1, Coefficient: 18 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(115814), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(-100685), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(456045), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(818713), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(-1020591), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(215831), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(-1125979), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(-1574771), /* Filter:1, Coefficient: 20 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-269690), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(-219149), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(483675), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(-385406), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(1533583), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(1697790), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(-640937), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(1106673), /* Filter:1, Coefficient: 22 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(49817), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(327706), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(-1078344), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(-901092), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(298404), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(-1679597), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(2199991), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(1469926), /* Filter:1, Coefficient: 24 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(353713), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(53122), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(159521), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(1284520), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(-2496622), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(-1148947), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(-783087), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(-2766581), /* Filter:1, Coefficient: 26 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-355599), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(-510586), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(1431454), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(273189), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(1536178), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(3316675), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(-2577984), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(34050), /* Filter:1, Coefficient: 28 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-219636), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(328523), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(-1358235), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(-2033639), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(2363113), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(-953893), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(3113447), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(3898837), /* Filter:1, Coefficient: 30 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(671539), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(455239), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(-951202), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(1203809), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(-4000055), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(-3912103), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(1239875), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(-3039958), /* Filter:1, Coefficient: 32 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-218603), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(-807891), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(2631188), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(1887961), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-210918), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(4333834), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(-5293214), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(-3091666), /* Filter:1, Coefficient: 34 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-756009), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(-2224), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(-716945), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(-3109267), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(5693246), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(2072560), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(2322327), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(6542868), /* Filter:1, Coefficient: 36 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(880151), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(1105224), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(-3059949), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(-204798), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(-4059383), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(-7516860), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(5504147), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-735062), /* Filter:1, Coefficient: 38 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(358874), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(-838150), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(3314225), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(4395729), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(-4691958), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(2885839), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(-7351776), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(-8370954), /* Filter:1, Coefficient: 40 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-1470082), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(-869768), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(1651041), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(-3053727), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(9097774), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(8056110), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(-1927609), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(7351404), /* Filter:1, Coefficient: 42 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(620664), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(1797644), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(-5765452), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(-3689804), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(-501272), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(-10047020), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(11484456), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(5838619), /* Filter:1, Coefficient: 44 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(1543877), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-152285), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(2092009), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(6966627), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-12064250), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(-3431170), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(-6100793), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(-14529711), /* Filter:1, Coefficient: 46 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-2007072), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(-2377760), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(6364024), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(-127424), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(9849385), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(16521192), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(-11146964), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(3059490), /* Filter:1, Coefficient: 48 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-667626), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(1968948), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(-7691699), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(-9695566), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(9318936), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(-7786997), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(17076149), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(18036447), /* Filter:1, Coefficient: 50 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(3348219), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(1996625), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(-3280325), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(7429429), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-21129476), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-17795597), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(2522929), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(-18539360), /* Filter:1, Coefficient: 52 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-1439811), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-4374392), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(13688967), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(8930617), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(2685075), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(25435789), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(-26909927), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-11976152), /* Filter:1, Coefficient: 54 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-4134491), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-185988), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(-5110430), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(-18240532), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(30208500), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(8011608), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(18016549), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(38981614), /* Filter:1, Coefficient: 56 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(5056345), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(7681958), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(-18700492), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(-2525039), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(-28470273), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(-50432144), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(28567790), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(-13547145), /* Filter:1, Coefficient: 58 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(5124031), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(-1687599), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(21825461), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(37870743), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(-32720110), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(25573931), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(-58824612), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(-64597690), /* Filter:1, Coefficient: 60 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-8594673), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(-10910995), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(32251799), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(4871577), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(95527010), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(134020246), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(-2840724), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(116347844), /* Filter:1, Coefficient: 62 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-6715326), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(1653573), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(-29612069), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(-50673264), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(111496535), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(33022726), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(244337981), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(319519294), /* Filter:1, Coefficient: 64 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(9058182), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(11001115), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(-45371944), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(-17020199), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(-62590522), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(-125985859), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(303533893), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(204637406), /* Filter:1, Coefficient: 66 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(6613114), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(-908182), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(17061394), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(37099013), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(-127527137), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(-74392732), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(72635229), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(-31218589), /* Filter:1, Coefficient: 68 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-6645584), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(-7338726), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(33592635), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(12488167), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(-3539197), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(43642084), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(-69423135), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(-46017033), /* Filter:1, Coefficient: 70 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-3379574), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(1862507), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(-10270513), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(-20289677), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(47036638), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(16924310), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(2774597), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(36162526), /* Filter:1, Coefficient: 72 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(4687214), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(3633943), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(-13928301), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(926804), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(-17765712), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(-31677983), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(34660987), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(7654480), /* Filter:1, Coefficient: 74 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(169088), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(-2783863), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(12081784), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(12208082), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(-19297249), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(5057264), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(-19463498), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(-26510886), /* Filter:1, Coefficient: 76 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-3155209), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(-1100082), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(3219751), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(-6628589), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(21084806), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(18281519), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(-12029861), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(9119834), /* Filter:1, Coefficient: 78 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(1452075), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(2496940), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(-9810802), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(-4999762), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(1997933), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(-13157294), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(19673432), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(13313145), /* Filter:1, Coefficient: 80 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(1470988), /* Filter:4, Coefficient: 81 */
	CONVERT_COEFF(-530883), /* Filter:4, Coefficient: 82 */
	CONVERT_COEFF(2887991), /* Filter:3, Coefficient: 81 */
	CONVERT_COEFF(7345400), /* Filter:3, Coefficient: 82 */
	CONVERT_COEFF(-15778100), /* Filter:2, Coefficient: 81 */
	CONVERT_COEFF(-5804379), /* Filter:2, Coefficient: 82 */
	CONVERT_COEFF(-2347060), /* Filter:1, Coefficient: 81 */
	CONVERT_COEFF(-13766705), /* Filter:1, Coefficient: 82 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-1826789), /* Filter:4, Coefficient: 83 */
	CONVERT_COEFF(-1519560), /* Filter:4, Coefficient: 84 */
	CONVERT_COEFF(5461142), /* Filter:3, Coefficient: 83 */
	CONVERT_COEFF(-374397), /* Filter:3, Coefficient: 84 */
	CONVERT_COEFF(7156002), /* Filter:2, Coefficient: 83 */
	CONVERT_COEFF(12632597), /* Filter:2, Coefficient: 84 */
	CONVERT_COEFF(-12757053), /* Filter:1, Coefficient: 83 */
	CONVERT_COEFF(-1966773), /* Filter:1, Coefficient: 84 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-77386), /* Filter:4, Coefficient: 85 */
	CONVERT_COEFF(1218353), /* Filter:4, Coefficient: 86 */
	CONVERT_COEFF(-5068027), /* Filter:3, Coefficient: 85 */
	CONVERT_COEFF(-5140982), /* Filter:3, Coefficient: 86 */
	CONVERT_COEFF(7480579), /* Filter:2, Coefficient: 85 */
	CONVERT_COEFF(-2749314), /* Filter:2, Coefficient: 86 */
	CONVERT_COEFF(8772201), /* Filter:1, Coefficient: 85 */
	CONVERT_COEFF(11107791), /* Filter:1, Coefficient: 86 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(1379966), /* Filter:4, Coefficient: 87 */
	CONVERT_COEFF(441511), /* Filter:4, Coefficient: 88 */
	CONVERT_COEFF(-1197542), /* Filter:3, Coefficient: 87 */
	CONVERT_COEFF(3116904), /* Filter:3, Coefficient: 88 */
	CONVERT_COEFF(-9360248), /* Filter:2, Coefficient: 87 */
	CONVERT_COEFF(-7716970), /* Filter:2, Coefficient: 88 */
	CONVERT_COEFF(4436017), /* Filter:1, Coefficient: 87 */
	CONVERT_COEFF(-4741987), /* Filter:1, Coefficient: 88 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-708796), /* Filter:4, Coefficient: 89 */
	CONVERT_COEFF(-1141122), /* Filter:4, Coefficient: 90 */
	CONVERT_COEFF(4381539), /* Filter:3, Coefficient: 89 */
	CONVERT_COEFF(2040448), /* Filter:3, Coefficient: 90 */
	CONVERT_COEFF(-281783), /* Filter:2, Coefficient: 89 */
	CONVERT_COEFF(6309527), /* Filter:2, Coefficient: 90 */
	CONVERT_COEFF(-8900882), /* Filter:1, Coefficient: 89 */
	CONVERT_COEFF(-5510132), /* Filter:1, Coefficient: 90 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-618337), /* Filter:4, Coefficient: 91 */
	CONVERT_COEFF(314070), /* Filter:4, Coefficient: 92 */
	CONVERT_COEFF(-1562338), /* Filter:3, Coefficient: 91 */
	CONVERT_COEFF(-3425561), /* Filter:3, Coefficient: 92 */
	CONVERT_COEFF(7023993), /* Filter:2, Coefficient: 91 */
	CONVERT_COEFF(2144389), /* Filter:2, Coefficient: 92 */
	CONVERT_COEFF(1698787), /* Filter:1, Coefficient: 91 */
	CONVERT_COEFF(6542354), /* Filter:1, Coefficient: 92 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(865964), /* Filter:4, Coefficient: 93 */
	CONVERT_COEFF(658712), /* Filter:4, Coefficient: 94 */
	CONVERT_COEFF(-2338169), /* Filter:3, Coefficient: 93 */
	CONVERT_COEFF(422162), /* Filter:3, Coefficient: 94 */
	CONVERT_COEFF(-3705979), /* Filter:2, Coefficient: 93 */
	CONVERT_COEFF(-5807407), /* Filter:2, Coefficient: 94 */
	CONVERT_COEFF(5575464), /* Filter:1, Coefficient: 93 */
	CONVERT_COEFF(397296), /* Filter:1, Coefficient: 94 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-34808), /* Filter:4, Coefficient: 95 */
	CONVERT_COEFF(-598408), /* Filter:4, Coefficient: 96 */
	CONVERT_COEFF(2447614), /* Filter:3, Coefficient: 95 */
	CONVERT_COEFF(2257011), /* Filter:3, Coefficient: 96 */
	CONVERT_COEFF(-3061584), /* Filter:2, Coefficient: 95 */
	CONVERT_COEFF(1669136), /* Filter:2, Coefficient: 96 */
	CONVERT_COEFF(-4329403), /* Filter:1, Coefficient: 95 */
	CONVERT_COEFF(-4978370), /* Filter:1, Coefficient: 96 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-608313), /* Filter:4, Coefficient: 97 */
	CONVERT_COEFF(-139661), /* Filter:4, Coefficient: 98 */
	CONVERT_COEFF(327984), /* Filter:3, Coefficient: 97 */
	CONVERT_COEFF(-1565787), /* Filter:3, Coefficient: 98 */
	CONVERT_COEFF(4387464), /* Filter:2, Coefficient: 97 */
	CONVERT_COEFF(3269084), /* Filter:2, Coefficient: 98 */
	CONVERT_COEFF(-1650694), /* Filter:1, Coefficient: 97 */
	CONVERT_COEFF(2456535), /* Filter:1, Coefficient: 98 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(366507), /* Filter:4, Coefficient: 99 */
	CONVERT_COEFF(506515), /* Filter:4, Coefficient: 100 */
	CONVERT_COEFF(-1944169), /* Filter:3, Coefficient: 99 */
	CONVERT_COEFF(-743409), /* Filter:3, Coefficient: 100 */
	CONVERT_COEFF(-227334), /* Filter:2, Coefficient: 99 */
	CONVERT_COEFF(-3001559), /* Filter:2, Coefficient: 100 */
	CONVERT_COEFF(4020324), /* Filter:1, Coefficient: 99 */
	CONVERT_COEFF(2215413), /* Filter:1, Coefficient: 100 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(227496), /* Filter:4, Coefficient: 101 */
	CONVERT_COEFF(-184800), /* Filter:4, Coefficient: 102 */
	CONVERT_COEFF(848717), /* Filter:3, Coefficient: 101 */
	CONVERT_COEFF(1522401), /* Filter:3, Coefficient: 102 */
	CONVERT_COEFF(-2999081), /* Filter:2, Coefficient: 101 */
	CONVERT_COEFF(-663371), /* Filter:2, Coefficient: 102 */
	CONVERT_COEFF(-1023079), /* Filter:1, Coefficient: 101 */
	CONVERT_COEFF(-2946059), /* Filter:1, Coefficient: 102 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-384947), /* Filter:4, Coefficient: 103 */
	CONVERT_COEFF(-250573), /* Filter:4, Coefficient: 104 */
	CONVERT_COEFF(897837), /* Filter:3, Coefficient: 103 */
	CONVERT_COEFF(-323140), /* Filter:3, Coefficient: 104 */
	CONVERT_COEFF(1805833), /* Filter:2, Coefficient: 103 */
	CONVERT_COEFF(2461354), /* Filter:2, Coefficient: 104 */
	CONVERT_COEFF(-2271916), /* Filter:1, Coefficient: 103 */
	CONVERT_COEFF(46810), /* Filter:1, Coefficient: 104 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(56883), /* Filter:4, Coefficient: 105 */
	CONVERT_COEFF(266613), /* Filter:4, Coefficient: 106 */
	CONVERT_COEFF(-1085470), /* Filter:3, Coefficient: 105 */
	CONVERT_COEFF(-870963), /* Filter:3, Coefficient: 106 */
	CONVERT_COEFF(1096398), /* Filter:2, Coefficient: 105 */
	CONVERT_COEFF(-881544), /* Filter:2, Coefficient: 106 */
	CONVERT_COEFF(1934524), /* Filter:1, Coefficient: 105 */
	CONVERT_COEFF(2002411), /* Filter:1, Coefficient: 106 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(231070), /* Filter:4, Coefficient: 107 */
	CONVERT_COEFF(21669), /* Filter:4, Coefficient: 108 */
	CONVERT_COEFF(-16819), /* Filter:3, Coefficient: 107 */
	CONVERT_COEFF(696469), /* Filter:3, Coefficient: 108 */
	CONVERT_COEFF(-1827647), /* Filter:2, Coefficient: 107 */
	CONVERT_COEFF(-1190629), /* Filter:2, Coefficient: 108 */
	CONVERT_COEFF(516537), /* Filter:1, Coefficient: 107 */
	CONVERT_COEFF(-1096900), /* Filter:1, Coefficient: 108 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-165914), /* Filter:4, Coefficient: 109 */
	CONVERT_COEFF(-188751), /* Filter:4, Coefficient: 110 */
	CONVERT_COEFF(737670), /* Filter:3, Coefficient: 109 */
	CONVERT_COEFF(199113), /* Filter:3, Coefficient: 110 */
	CONVERT_COEFF(247255), /* Filter:2, Coefficient: 109 */
	CONVERT_COEFF(1222537), /* Filter:2, Coefficient: 110 */
	CONVERT_COEFF(-1569451), /* Filter:1, Coefficient: 109 */
	CONVERT_COEFF(-750469), /* Filter:1, Coefficient: 110 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-60237), /* Filter:4, Coefficient: 111 */
	CONVERT_COEFF(89578), /* Filter:4, Coefficient: 112 */
	CONVERT_COEFF(-389567), /* Filter:3, Coefficient: 111 */
	CONVERT_COEFF(-560182), /* Filter:3, Coefficient: 112 */
	CONVERT_COEFF(1068384), /* Filter:2, Coefficient: 111 */
	CONVERT_COEFF(124598), /* Filter:2, Coefficient: 112 */
	CONVERT_COEFF(482448), /* Filter:1, Coefficient: 111 */
	CONVERT_COEFF(1101069), /* Filter:1, Coefficient: 112 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(139197), /* Filter:4, Coefficient: 113 */
	CONVERT_COEFF(70304), /* Filter:4, Coefficient: 114 */
	CONVERT_COEFF(-263233), /* Filter:3, Coefficient: 113 */
	CONVERT_COEFF(174746), /* Filter:3, Coefficient: 114 */
	CONVERT_COEFF(-721785), /* Filter:2, Coefficient: 113 */
	CONVERT_COEFF(-838118), /* Filter:2, Coefficient: 114 */
	CONVERT_COEFF(755091), /* Filter:1, Coefficient: 113 */
	CONVERT_COEFF(-90734), /* Filter:1, Coefficient: 114 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-38289), /* Filter:4, Coefficient: 115 */
	CONVERT_COEFF(-93047), /* Filter:4, Coefficient: 116 */
	CONVERT_COEFF(383750), /* Filter:3, Coefficient: 115 */
	CONVERT_COEFF(250958), /* Filter:3, Coefficient: 116 */
	CONVERT_COEFF(-289378), /* Filter:2, Coefficient: 115 */
	CONVERT_COEFF(357413), /* Filter:2, Coefficient: 116 */
	CONVERT_COEFF(-683828), /* Filter:1, Coefficient: 115 */
	CONVERT_COEFF(-627769), /* Filter:1, Coefficient: 116 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-63107), /* Filter:4, Coefficient: 117 */
	CONVERT_COEFF(8665), /* Filter:4, Coefficient: 118 */
	CONVERT_COEFF(-44424), /* Filter:3, Coefficient: 117 */
	CONVERT_COEFF(-235818), /* Filter:3, Coefficient: 118 */
	CONVERT_COEFF(583440), /* Filter:2, Coefficient: 117 */
	CONVERT_COEFF(312988), /* Filter:2, Coefficient: 118 */
	CONVERT_COEFF(-112449), /* Filter:1, Coefficient: 117 */
	CONVERT_COEFF(363474), /* Filter:1, Coefficient: 118 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(56144), /* Filter:4, Coefficient: 119 */
	CONVERT_COEFF(48031), /* Filter:4, Coefficient: 120 */
	CONVERT_COEFF(-199384), /* Filter:3, Coefficient: 119 */
	CONVERT_COEFF(-19310), /* Filter:3, Coefficient: 120 */
	CONVERT_COEFF(-127566), /* Filter:2, Coefficient: 119 */
	CONVERT_COEFF(-358789), /* Filter:2, Coefficient: 120 */
	CONVERT_COEFF(449326), /* Filter:1, Coefficient: 119 */
	CONVERT_COEFF(178527), /* Filter:1, Coefficient: 120 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(4767), /* Filter:4, Coefficient: 121 */
	CONVERT_COEFF(-30353), /* Filter:4, Coefficient: 122 */
	CONVERT_COEFF(128040), /* Filter:3, Coefficient: 121 */
	CONVERT_COEFF(136746), /* Filter:3, Coefficient: 122 */
	CONVERT_COEFF(-257969), /* Filter:2, Coefficient: 121 */
	CONVERT_COEFF(8641), /* Filter:2, Coefficient: 122 */
	CONVERT_COEFF(-151547), /* Filter:1, Coefficient: 121 */
	CONVERT_COEFF(-276720), /* Filter:1, Coefficient: 122 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(-31778), /* Filter:4, Coefficient: 123 */
	CONVERT_COEFF(-7893), /* Filter:4, Coefficient: 124 */
	CONVERT_COEFF(38083), /* Filter:3, Coefficient: 123 */
	CONVERT_COEFF(-60147), /* Filter:3, Coefficient: 124 */
	CONVERT_COEFF(190921), /* Filter:2, Coefficient: 123 */
	CONVERT_COEFF(174318), /* Filter:2, Coefficient: 124 */
	CONVERT_COEFF(-161692), /* Filter:1, Coefficient: 123 */
	CONVERT_COEFF(35536), /* Filter:1, Coefficient: 124 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(14756), /* Filter:4, Coefficient: 125 */
	CONVERT_COEFF(18247), /* Filter:4, Coefficient: 126 */
	CONVERT_COEFF(-80967), /* Filter:3, Coefficient: 125 */
	CONVERT_COEFF(-32124), /* Filter:3, Coefficient: 126 */
	CONVERT_COEFF(32782), /* Filter:2, Coefficient: 125 */
	CONVERT_COEFF(-84503), /* Filter:2, Coefficient: 126 */
	CONVERT_COEFF(141820), /* Filter:1, Coefficient: 125 */
	CONVERT_COEFF(108395), /* Filter:1, Coefficient: 126 */

	/* Filter #1, conversion from 24000 Hz to 8000 Hz */

	CONVERT_COEFF(5816), /* Filter:4, Coefficient: 127 */
	CONVERT_COEFF(-573080), /* Filter:4, Coefficient: 128 */
	CONVERT_COEFF(24552), /* Filter:3, Coefficient: 127 */
	CONVERT_COEFF(872700), /* Filter:3, Coefficient: 128 */
	CONVERT_COEFF(-95337), /* Filter:2, Coefficient: 127 */
	CONVERT_COEFF(-244664), /* Filter:2, Coefficient: 128 */
	CONVERT_COEFF(10015), /* Filter:1, Coefficient: 127 */
	CONVERT_COEFF(-54956)
};
