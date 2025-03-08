/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 48000 Hz to 12000 Hz */
/* NUM_FILTERS=4, FILTER_LENGTH=96, alpha=5.700000, gamma=0.424000 */

__cold_rodata static const int32_t coeff48000to12000[] = {

	/* Filter #1, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(9466), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(18324), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(18675), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(6681), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(-44320), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(-15044), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(41846), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(99856), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(-108866), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(-168122), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(-143205), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(-4840), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(18979), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(-124739), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(-289576), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(-372252), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-14873), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(-36092), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(-43775), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(-28417), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(120645), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(74642), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(-36940), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(-172008), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(212485), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(406758), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(446885), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(243397), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(-270550), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(47706), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(493005), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(859159), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(8921), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(53986), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(83668), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(76633), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(-259236), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(-230916), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(-63713), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(194231), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(-181655), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(-668246), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(-964705), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(-841844), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(902116), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(470138), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(-375031), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(-1319757), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(25833), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(-53679), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(-127150), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(-153896), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(429216), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(506489), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(338544), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(-54472), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(-229172), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(697753), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(1541275), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(1833706), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(-1890703), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(-1664796), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(-514224), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(1238423), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-107640), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(6019), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(144977), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(244094), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(-527257), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(-854646), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(-830345), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(-379134), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(1268127), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(-96526), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(-1771933), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(-2986182), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(2863711), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(3496880), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(2551682), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(94379), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(243618), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(120959), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(-89812), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(-303143), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(373317), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(1118639), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(1481524), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(1194410), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(-3011832), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(-1547918), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(1028425), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(3697569), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(-3026789), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(-5421590), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(-5729809), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(-3309613), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-414069), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(-346290), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(-95407), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(254838), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(254718), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(-1017842), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(-2072112), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(-2348186), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(5163786), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(4438011), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(1391449), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(-2999213), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(1279200), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(6283525), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(9357238), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(8581017), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(556057), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(654070), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(461041), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(8985), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(-1547890), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(169371), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(2173059), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(3569126), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(-6896234), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(-8311637), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(-6031532), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(-353097), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(3488394), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(-4399594), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(-11887580), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(-15284742), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-543384), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(-960956), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(-1025319), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(-634838), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(3569496), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(1878806), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-1076651), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(-4206990), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(6748994), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(12209118), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(13074826), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(7888560), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(-12059514), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(-2284367), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(10842409), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(21814880), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(131905), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(1022024), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(1689633), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(1826448), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(-6121966), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(-5683979), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(-2536796), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(2619817), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(-2343046), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(-14088848), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(-22312461), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(-22299178), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(24861172), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(16527773), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(-2222990), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(-25382165), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(1291803), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(187823), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(-1156014), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(-2289689), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(8156827), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(12037112), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(12558708), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(9041064), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(-11640045), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(8421631), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(32901411), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(54414451), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(-43234314), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(-45424925), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(-24777921), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(19525839), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-2804099), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-2486101), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(-1405857), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(99935), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(2176743), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(-6112957), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-13291600), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(-17077204), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(65561608), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(61534767), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(41975533), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(11356417), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(80690239), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(145621917), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(198554116), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(225828200), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(1544944), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(2466704), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(2590632), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(1922395), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(-16247012), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(-11073761), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(-3227323), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(4820544), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-22315217), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(-50044994), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(-64751780), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-63486697), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(220203456), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(183182934), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(124528682), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(59139166), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(735067), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(-540990), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(-1480192), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-1812185), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(10661840), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(12763578), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(10929621), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(6261251), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(-48196279), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(-24804128), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(-1006930), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(16367589), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(2395365), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-34403398), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(-46984107), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(-38540927), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-1504699), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(-752135), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(118595), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(782220), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(660837), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(-3907345), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(-6107752), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(-5624729), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(23479491), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(20364635), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(10388086), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(-1396259), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(-17723958), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(4911585), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(20616375), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(25014862), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(1034412), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(852013), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(377238), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(-159449), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(-3138072), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(63987), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(2647613), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(3737272), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(-10266950), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(-13455602), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(-10821182), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(-4453036), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(18775762), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(6405039), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(-6134454), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(-13930540), /* Filter:1, Coefficient: 64 */

	/* Filter #17, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-540437), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(-643355), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(-474537), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(-146712), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(3175158), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(1467919), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(-516205), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(-1946005), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(2499446), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(7214349), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(8236197), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(5814958), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(-14805492), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(-9671153), /* Filter:1, Coefficient: 66 */
	CONVERT_COEFF(-1632211), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(5613145), /* Filter:1, Coefficient: 68 */

	/* Filter #18, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(182504), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(380962), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(391293), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(242079), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(-2350019), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(-1745607), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(-551374), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(648570), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(1519255), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(-2610059), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(-4955468), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(-4899346), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(9335222), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(8686809), /* Filter:1, Coefficient: 70 */
	CONVERT_COEFF(4712022), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(-403519), /* Filter:1, Coefficient: 72 */

	/* Filter #19, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(22015), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(-166585), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(-252342), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(-219066), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(1369416), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(1406290), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(869654), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(84471), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(-2899640), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(-115895), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(2186507), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(3171366), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(-4412139), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(-5920243), /* Filter:1, Coefficient: 74 */
	CONVERT_COEFF(-4796349), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(-1992494), /* Filter:1, Coefficient: 76 */

	/* Filter #20, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-102810), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(32566), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(127289), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(150143), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(-582238), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(-881098), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(-762309), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(-358757), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(2695169), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(1237005), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(-416702), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(-1556295), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(1044258), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(3054325), /* Filter:1, Coefficient: 78 */
	CONVERT_COEFF(3442737), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(2390972), /* Filter:1, Coefficient: 80 */

	/* Filter #21, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(106283), /* Filter:4, Coefficient: 81 */
	CONVERT_COEFF(27996), /* Filter:4, Coefficient: 82 */
	CONVERT_COEFF(-44477), /* Filter:4, Coefficient: 83 */
	CONVERT_COEFF(-81857), /* Filter:4, Coefficient: 84 */
	CONVERT_COEFF(104868), /* Filter:3, Coefficient: 81 */
	CONVERT_COEFF(424671), /* Filter:3, Coefficient: 82 */
	CONVERT_COEFF(499517), /* Filter:3, Coefficient: 83 */
	CONVERT_COEFF(352818), /* Filter:3, Coefficient: 84 */
	CONVERT_COEFF(-1827601), /* Filter:2, Coefficient: 81 */
	CONVERT_COEFF(-1307300), /* Filter:2, Coefficient: 82 */
	CONVERT_COEFF(-381962), /* Filter:2, Coefficient: 83 */
	CONVERT_COEFF(479247), /* Filter:2, Coefficient: 84 */
	CONVERT_COEFF(626053), /* Filter:1, Coefficient: 81 */
	CONVERT_COEFF(-990379), /* Filter:1, Coefficient: 82 */
	CONVERT_COEFF(-1844980), /* Filter:1, Coefficient: 83 */
	CONVERT_COEFF(-1771871), /* Filter:1, Coefficient: 84 */

	/* Filter #22, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-76222), /* Filter:4, Coefficient: 85 */
	CONVERT_COEFF(-39943), /* Filter:4, Coefficient: 86 */
	CONVERT_COEFF(4124), /* Filter:4, Coefficient: 87 */
	CONVERT_COEFF(35042), /* Filter:4, Coefficient: 88 */
	CONVERT_COEFF(96315), /* Filter:3, Coefficient: 85 */
	CONVERT_COEFF(-136873), /* Filter:3, Coefficient: 86 */
	CONVERT_COEFF(-254389), /* Filter:3, Coefficient: 87 */
	CONVERT_COEFF(-235483), /* Filter:3, Coefficient: 88 */
	CONVERT_COEFF(939561), /* Filter:2, Coefficient: 85 */
	CONVERT_COEFF(907226), /* Filter:2, Coefficient: 86 */
	CONVERT_COEFF(518385), /* Filter:2, Coefficient: 87 */
	CONVERT_COEFF(25446), /* Filter:2, Coefficient: 88 */
	CONVERT_COEFF(-1021645), /* Filter:1, Coefficient: 85 */
	CONVERT_COEFF(-61990), /* Filter:1, Coefficient: 86 */
	CONVERT_COEFF(668408), /* Filter:1, Coefficient: 87 */
	CONVERT_COEFF(936512), /* Filter:1, Coefficient: 88 */

	/* Filter #23, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(42668), /* Filter:4, Coefficient: 89 */
	CONVERT_COEFF(29706), /* Filter:4, Coefficient: 90 */
	CONVERT_COEFF(7441), /* Filter:4, Coefficient: 91 */
	CONVERT_COEFF(-11516), /* Filter:4, Coefficient: 92 */
	CONVERT_COEFF(-123440), /* Filter:3, Coefficient: 89 */
	CONVERT_COEFF(8781), /* Filter:3, Coefficient: 90 */
	CONVERT_COEFF(98290), /* Filter:3, Coefficient: 91 */
	CONVERT_COEFF(118044), /* Filter:3, Coefficient: 92 */
	CONVERT_COEFF(-339383), /* Filter:2, Coefficient: 89 */
	CONVERT_COEFF(-459507), /* Filter:2, Coefficient: 90 */
	CONVERT_COEFF(-355164), /* Filter:2, Coefficient: 91 */
	CONVERT_COEFF(-138328), /* Filter:2, Coefficient: 92 */
	CONVERT_COEFF(761504), /* Filter:1, Coefficient: 89 */
	CONVERT_COEFF(341342), /* Filter:1, Coefficient: 90 */
	CONVERT_COEFF(-79676), /* Filter:1, Coefficient: 91 */
	CONVERT_COEFF(-329103), /* Filter:1, Coefficient: 92 */

	/* Filter #24, conversion from 48000 Hz to 12000 Hz */

	CONVERT_COEFF(-19411), /* Filter:4, Coefficient: 93 */
	CONVERT_COEFF(-15780), /* Filter:4, Coefficient: 94 */
	CONVERT_COEFF(-5811), /* Filter:4, Coefficient: 95 */
	CONVERT_COEFF(758261), /* Filter:4, Coefficient: 96 */
	CONVERT_COEFF(79948), /* Filter:3, Coefficient: 93 */
	CONVERT_COEFF(19085), /* Filter:3, Coefficient: 94 */
	CONVERT_COEFF(-29023), /* Filter:3, Coefficient: 95 */
	CONVERT_COEFF(-1187553), /* Filter:3, Coefficient: 96 */
	CONVERT_COEFF(62290), /* Filter:2, Coefficient: 93 */
	CONVERT_COEFF(164272), /* Filter:2, Coefficient: 94 */
	CONVERT_COEFF(156134), /* Filter:2, Coefficient: 95 */
	CONVERT_COEFF(378483), /* Filter:2, Coefficient: 96 */
	CONVERT_COEFF(-360897), /* Filter:1, Coefficient: 93 */
	CONVERT_COEFF(-238066), /* Filter:1, Coefficient: 94 */
	CONVERT_COEFF(-70489), /* Filter:1, Coefficient: 95 */
	CONVERT_COEFF(50810), /* Filter:1, Coefficient: 96 */
};
