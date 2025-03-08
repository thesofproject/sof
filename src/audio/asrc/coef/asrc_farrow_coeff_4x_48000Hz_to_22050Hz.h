/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 48000 Hz to 22050 Hz */
/* NUM_FILTERS=5, FILTER_LENGTH=80, alpha=6.900000, gamma=0.440000 */

__cold_rodata static const int32_t coeff48000to22050[] = {

	/* Filter #1, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(7662), /* Filter:5, Coefficient: 1 */
	CONVERT_COEFF(-17184), /* Filter:5, Coefficient: 2 */
	CONVERT_COEFF(-22717), /* Filter:5, Coefficient: 3 */
	CONVERT_COEFF(18493), /* Filter:5, Coefficient: 4 */
	CONVERT_COEFF(26582), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(59229), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(-23847), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(-124610), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(-47392), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(71132), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(144232), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(-51617), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(-97554), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(-81873), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(168609), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(294217), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(-7478), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(-118181), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(-86878), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(179401), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(50021), /* Filter:5, Coefficient: 5 */
	CONVERT_COEFF(-716), /* Filter:5, Coefficient: 6 */
	CONVERT_COEFF(-81566), /* Filter:5, Coefficient: 7 */
	CONVERT_COEFF(-50232), /* Filter:5, Coefficient: 8 */
	CONVERT_COEFF(-31457), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(195123), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(176350), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(-200655), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(-305430), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(-114428), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(443237), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(491935), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(-107851), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(-611698), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(-258982), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(827677), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(315885), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(-78832), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(-610555), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(-331518), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(93705), /* Filter:5, Coefficient: 9 */
	CONVERT_COEFF(135899), /* Filter:5, Coefficient: 10 */
	CONVERT_COEFF(-50936), /* Filter:5, Coefficient: 11 */
	CONVERT_COEFF(-230545), /* Filter:5, Coefficient: 12 */
	CONVERT_COEFF(-407292), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(40422), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(637092), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(360246), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(-369778), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(-1017042), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(-134226), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(1419254), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(1008537), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(-574204), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(-1940858), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(-505832), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(737212), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(1062392), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(-352536), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(-1841477), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-76801), /* Filter:5, Coefficient: 13 */
	CONVERT_COEFF(275249), /* Filter:5, Coefficient: 14 */
	CONVERT_COEFF(286242), /* Filter:5, Coefficient: 15 */
	CONVERT_COEFF(-192062), /* Filter:5, Coefficient: 16 */
	CONVERT_COEFF(-683487), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(-963806), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(323515), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(1549537), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(1160591), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(-1248908), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(-2484661), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(65823), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(2484992), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(2450305), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(-1828062), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(-4678056), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(-798359), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(2086950), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(2599807), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(-1103167), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-515072), /* Filter:5, Coefficient: 17 */
	CONVERT_COEFF(-77740), /* Filter:5, Coefficient: 18 */
	CONVERT_COEFF(637488), /* Filter:5, Coefficient: 19 */
	CONVERT_COEFF(520979), /* Filter:5, Coefficient: 20 */
	CONVERT_COEFF(579066), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(-1719643), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(-1913093), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(1028444), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(3467676), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(2238991), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(-3179440), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(-5124630), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(-677091), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(5922919), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(4937918), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(-4589554), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(-4357955), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(-1503387), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(4861173), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(5344084), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-497338), /* Filter:5, Coefficient: 21 */
	CONVERT_COEFF(-1009488), /* Filter:5, Coefficient: 22 */
	CONVERT_COEFF(-19056), /* Filter:5, Coefficient: 23 */
	CONVERT_COEFF(1296890), /* Filter:5, Coefficient: 24 */
	CONVERT_COEFF(3210464), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(772866), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(-3684403), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(-3455526), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(793879), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(7290357), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(3834155), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(-6951870), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(-9665238), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(-459773), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(12380065), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(8936328), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(-2820697), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(-8978993), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(-2385048), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(10125784), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(889191), /* Filter:5, Coefficient: 25 */
	CONVERT_COEFF(-1077548), /* Filter:5, Coefficient: 26 */
	CONVERT_COEFF(-1876055), /* Filter:5, Coefficient: 27 */
	CONVERT_COEFF(109024), /* Filter:5, Coefficient: 28 */
	CONVERT_COEFF(2472912), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(6129880), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(938394), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(-7290689), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(-9648642), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(2539229), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(14225977), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(6351241), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(-10107211), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(-18425284), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(684847), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(24410695), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(9951675), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(-6442120), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(-17275963), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(-3302824), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(2503811), /* Filter:5, Coefficient: 29 */
	CONVERT_COEFF(1622255), /* Filter:5, Coefficient: 30 */
	CONVERT_COEFF(-2108058), /* Filter:5, Coefficient: 31 */
	CONVERT_COEFF(-3706384), /* Filter:5, Coefficient: 32 */
	CONVERT_COEFF(-6226493), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(5152980), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(11810681), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(1756465), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(-14169848), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(-18068824), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(6045860), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(28349964), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(15712903), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(-21218811), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(-35403029), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(3599978), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(20277588), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(18098088), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(-14414412), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(-34069195), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-43231), /* Filter:5, Coefficient: 33 */
	CONVERT_COEFF(4988198), /* Filter:5, Coefficient: 34 */
	CONVERT_COEFF(4446245), /* Filter:5, Coefficient: 35 */
	CONVERT_COEFF(-2462458), /* Filter:5, Coefficient: 36 */
	CONVERT_COEFF(-14505985), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(-13737342), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(8767251), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(27475802), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(12429936), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(-29867986), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-41267927), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(9703094), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(50670832), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(31896562), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(-48962542), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(-87373984), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(-4069199), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(44482662), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(37762356), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(-39254890), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-8050820), /* Filter:5, Coefficient: 37 */
	CONVERT_COEFF(-4999058), /* Filter:5, Coefficient: 38 */
	CONVERT_COEFF(4105217), /* Filter:5, Coefficient: 39 */
	CONVERT_COEFF(9073725), /* Filter:5, Coefficient: 40 */
	CONVERT_COEFF(15338177), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(-19743794), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(-39156404), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(-18221320), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(75644534), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(74121388), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(-12477230), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(-103703844), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(4474816), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(169430567), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(238526201), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(112848617), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(-91913076), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-4506400), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(214304195), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(405304801), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(4173646), /* Filter:5, Coefficient: 41 */
	CONVERT_COEFF(-4920958), /* Filter:5, Coefficient: 42 */
	CONVERT_COEFF(-8020196), /* Filter:5, Coefficient: 43 */
	CONVERT_COEFF(-2482049), /* Filter:5, Coefficient: 44 */
	CONVERT_COEFF(22486980), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(39548952), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(16825069), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(-17561438), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(-105059398), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(-14951358), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(73361822), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(77292504), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(-112603327), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(-238487201), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(-169572730), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(-4590557), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(405304801), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(214304195), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(-4506400), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(-91913076), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(4418383), /* Filter:5, Coefficient: 45 */
	CONVERT_COEFF(4986040), /* Filter:5, Coefficient: 46 */
	CONVERT_COEFF(-25488), /* Filter:5, Coefficient: 47 */
	CONVERT_COEFF(-3695594), /* Filter:5, Coefficient: 48 */
	CONVERT_COEFF(-26497283), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-6227664), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(14635866), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(13055417), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(11676560), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(-41128822), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-31313215), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(11383073), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(87419323), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(49090442), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(-31848996), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(-50742654), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(-39254890), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(37762356), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(44482662), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(-4069199), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-2115328), /* Filter:5, Coefficient: 49 */
	CONVERT_COEFF(1610445), /* Filter:5, Coefficient: 50 */
	CONVERT_COEFF(2503111), /* Filter:5, Coefficient: 51 */
	CONVERT_COEFF(117640), /* Filter:5, Coefficient: 52 */
	CONVERT_COEFF(-3354395), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(-11619407), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(-3795045), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(6834164), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(28805923), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(7109971), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(-17815824), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(-14850789), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-3681316), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(35411365), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(21287118), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-15681405), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(-34069195), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(-14414412), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(18098088), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(20277588), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-1871020), /* Filter:5, Coefficient: 53 */
	CONVERT_COEFF(-1081544), /* Filter:5, Coefficient: 54 */
	CONVERT_COEFF(883235), /* Filter:5, Coefficient: 55 */
	CONVERT_COEFF(1296873), /* Filter:5, Coefficient: 56 */
	CONVERT_COEFF(6559989), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-1807040), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(-6018602), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-1736019), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(5784968), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(14451451), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(3098823), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(-9531104), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(-24446956), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(-728979), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(18430270), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(10144287), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(-3302824), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-17275963), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(-6442120), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(9951675), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-14486), /* Filter:5, Coefficient: 57 */
	CONVERT_COEFF(-1007116), /* Filter:5, Coefficient: 58 */
	CONVERT_COEFF(-499607), /* Filter:5, Coefficient: 59 */
	CONVERT_COEFF(518015), /* Filter:5, Coefficient: 60 */
	CONVERT_COEFF(3750042), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(3262673), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(-1214266), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(-3107008), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(-7325337), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(3551588), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(7434859), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(1083621), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(-8921035), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(-12401028), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(437331), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(9670117), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(10125784), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(-2385048), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(-8978993), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(-2820697), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(637690), /* Filter:5, Coefficient: 61 */
	CONVERT_COEFF(-75416), /* Filter:5, Coefficient: 62 */
	CONVERT_COEFF(-514050), /* Filter:5, Coefficient: 63 */
	CONVERT_COEFF(-193261), /* Filter:5, Coefficient: 64 */
	CONVERT_COEFF(-639301), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(2025342), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(1480378), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(-777830), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(-5090473), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(-3382465), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(2113958), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(3558926), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(4609140), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(-4932011), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(-5934824), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(666961), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(5344084), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(4861173), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(-1503387), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(-4357955), /* Filter:1, Coefficient: 64 */

	/* Filter #17, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(284900), /* Filter:5, Coefficient: 65 */
	CONVERT_COEFF(275450), /* Filter:5, Coefficient: 66 */
	CONVERT_COEFF(-75748), /* Filter:5, Coefficient: 67 */
	CONVERT_COEFF(-230175), /* Filter:5, Coefficient: 68 */
	CONVERT_COEFF(-1466138), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(-138526), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(988361), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(561735), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(202115), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(-2487174), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(-1348983), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(1116387), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(4682079), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(1837378), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(-2448933), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(-2491052), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(-1103167), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(2599807), /* Filter:1, Coefficient: 66 */
	CONVERT_COEFF(2086950), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(-798359), /* Filter:1, Coefficient: 68 */

	/* Filter #18, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-51484), /* Filter:5, Coefficient: 69 */
	CONVERT_COEFF(135388), /* Filter:5, Coefficient: 70 */
	CONVERT_COEFF(93830), /* Filter:5, Coefficient: 71 */
	CONVERT_COEFF(-49843), /* Filter:5, Coefficient: 72 */
	CONVERT_COEFF(-431830), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(-583157), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(31866), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(400739), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(1470096), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(-80799), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(-1028729), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(-410822), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(502162), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(1943488), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(577848), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(-1008802), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(-1841477), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(-352536), /* Filter:1, Coefficient: 70 */
	CONVERT_COEFF(1062392), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(737212), /* Filter:1, Coefficient: 72 */

	/* Filter #19, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-81469), /* Filter:5, Coefficient: 73 */
	CONVERT_COEFF(-912), /* Filter:5, Coefficient: 74 */
	CONVERT_COEFF(49880), /* Filter:5, Coefficient: 75 */
	CONVERT_COEFF(18545), /* Filter:5, Coefficient: 76 */
	CONVERT_COEFF(149914), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(-191736), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(-168399), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(50437), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(482726), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(466197), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(-99774), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(-314278), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(-830203), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(258176), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(613009), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(108811), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(-331518), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(-610555), /* Filter:1, Coefficient: 74 */
	CONVERT_COEFF(-78832), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(315885), /* Filter:1, Coefficient: 76 */

	/* Filter #20, conversion from 48000 Hz to 22050 Hz */

	CONVERT_COEFF(-22620), /* Filter:5, Coefficient: 77 */
	CONVERT_COEFF(-17175), /* Filter:5, Coefficient: 78 */
	CONVERT_COEFF(7621), /* Filter:5, Coefficient: 79 */
	CONVERT_COEFF(1139618), /* Filter:5, Coefficient: 80 */
	CONVERT_COEFF(114508), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(9531), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(-57126), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(-2091381), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(-63466), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(145663), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(78240), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(1014579), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(-294701), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(-169322), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(81968), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(-55338), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(179401), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(-86878), /* Filter:1, Coefficient: 78 */
	CONVERT_COEFF(-118181), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(-7478), /* Filter:1, Coefficient: 80 */
};
