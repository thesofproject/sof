/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2025 Intel Corporation.
 */

/* Conversion from 48000 Hz to 8000 Hz */
/* NUM_FILTERS=4, FILTER_LENGTH=128, alpha=5.600000, gamma=0.415500 */

__cold_rodata static const int32_t coeff48000to08000[] = {

	/* Filter #1, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-3301), /* Filter:4, Coefficient: 1 */
	CONVERT_COEFF(-3924), /* Filter:4, Coefficient: 2 */
	CONVERT_COEFF(-3547), /* Filter:4, Coefficient: 3 */
	CONVERT_COEFF(-2001), /* Filter:4, Coefficient: 4 */
	CONVERT_COEFF(3989), /* Filter:3, Coefficient: 1 */
	CONVERT_COEFF(-5958), /* Filter:3, Coefficient: 2 */
	CONVERT_COEFF(-17787), /* Filter:3, Coefficient: 3 */
	CONVERT_COEFF(-28475), /* Filter:3, Coefficient: 4 */
	CONVERT_COEFF(65517), /* Filter:2, Coefficient: 1 */
	CONVERT_COEFF(63506), /* Filter:2, Coefficient: 2 */
	CONVERT_COEFF(39846), /* Filter:2, Coefficient: 3 */
	CONVERT_COEFF(-6206), /* Filter:2, Coefficient: 4 */
	CONVERT_COEFF(88725), /* Filter:1, Coefficient: 1 */
	CONVERT_COEFF(154917), /* Filter:1, Coefficient: 2 */
	CONVERT_COEFF(208524), /* Filter:1, Coefficient: 3 */
	CONVERT_COEFF(227017), /* Filter:1, Coefficient: 4 */

	/* Filter #2, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(592), /* Filter:4, Coefficient: 5 */
	CONVERT_COEFF(3785), /* Filter:4, Coefficient: 6 */
	CONVERT_COEFF(6857), /* Filter:4, Coefficient: 7 */
	CONVERT_COEFF(8947), /* Filter:4, Coefficient: 8 */
	CONVERT_COEFF(-34495), /* Filter:3, Coefficient: 5 */
	CONVERT_COEFF(-32695), /* Filter:3, Coefficient: 6 */
	CONVERT_COEFF(-21285), /* Filter:3, Coefficient: 7 */
	CONVERT_COEFF(-655), /* Filter:3, Coefficient: 8 */
	CONVERT_COEFF(-68868), /* Filter:2, Coefficient: 5 */
	CONVERT_COEFF(-135707), /* Filter:2, Coefficient: 6 */
	CONVERT_COEFF(-189365), /* Filter:2, Coefficient: 7 */
	CONVERT_COEFF(-211084), /* Filter:2, Coefficient: 8 */
	CONVERT_COEFF(190319), /* Filter:1, Coefficient: 5 */
	CONVERT_COEFF(87541), /* Filter:1, Coefficient: 6 */
	CONVERT_COEFF(-77070), /* Filter:1, Coefficient: 7 */
	CONVERT_COEFF(-280839), /* Filter:1, Coefficient: 8 */

	/* Filter #3, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(9255), /* Filter:4, Coefficient: 9 */
	CONVERT_COEFF(7264), /* Filter:4, Coefficient: 10 */
	CONVERT_COEFF(2938), /* Filter:4, Coefficient: 11 */
	CONVERT_COEFF(-3172), /* Filter:4, Coefficient: 12 */
	CONVERT_COEFF(26209), /* Filter:3, Coefficient: 9 */
	CONVERT_COEFF(53920), /* Filter:3, Coefficient: 10 */
	CONVERT_COEFF(75559), /* Filter:3, Coefficient: 11 */
	CONVERT_COEFF(84121), /* Filter:3, Coefficient: 12 */
	CONVERT_COEFF(-185467), /* Filter:2, Coefficient: 9 */
	CONVERT_COEFF(-105464), /* Filter:2, Coefficient: 10 */
	CONVERT_COEFF(23711), /* Filter:2, Coefficient: 11 */
	CONVERT_COEFF(182957), /* Filter:2, Coefficient: 12 */
	CONVERT_COEFF(-483590), /* Filter:1, Coefficient: 9 */
	CONVERT_COEFF(-633541), /* Filter:1, Coefficient: 10 */
	CONVERT_COEFF(-677765), /* Filter:1, Coefficient: 11 */
	CONVERT_COEFF(-575509), /* Filter:1, Coefficient: 12 */

	/* Filter #4, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-9948), /* Filter:4, Coefficient: 13 */
	CONVERT_COEFF(-15871), /* Filter:4, Coefficient: 14 */
	CONVERT_COEFF(-19318), /* Filter:4, Coefficient: 15 */
	CONVERT_COEFF(-18942), /* Filter:4, Coefficient: 16 */
	CONVERT_COEFF(74293), /* Filter:3, Coefficient: 13 */
	CONVERT_COEFF(44149), /* Filter:3, Coefficient: 14 */
	CONVERT_COEFF(-3655), /* Filter:3, Coefficient: 15 */
	CONVERT_COEFF(-61592), /* Filter:3, Coefficient: 16 */
	CONVERT_COEFF(340885), /* Filter:2, Coefficient: 13 */
	CONVERT_COEFF(458887), /* Filter:2, Coefficient: 14 */
	CONVERT_COEFF(499081), /* Filter:2, Coefficient: 15 */
	CONVERT_COEFF(433742), /* Filter:2, Coefficient: 16 */
	CONVERT_COEFF(-311577), /* Filter:1, Coefficient: 13 */
	CONVERT_COEFF(93646), /* Filter:1, Coefficient: 14 */
	CONVERT_COEFF(580763), /* Filter:1, Coefficient: 15 */
	CONVERT_COEFF(1056783), /* Filter:1, Coefficient: 16 */

	/* Filter #5, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-14049), /* Filter:4, Coefficient: 17 */
	CONVERT_COEFF(-4887), /* Filter:4, Coefficient: 18 */
	CONVERT_COEFF(7242), /* Filter:4, Coefficient: 19 */
	CONVERT_COEFF(20103), /* Filter:4, Coefficient: 20 */
	CONVERT_COEFF(-118123), /* Filter:3, Coefficient: 17 */
	CONVERT_COEFF(-159680), /* Filter:3, Coefficient: 18 */
	CONVERT_COEFF(-173513), /* Filter:3, Coefficient: 19 */
	CONVERT_COEFF(-150856), /* Filter:3, Coefficient: 20 */
	CONVERT_COEFF(254175), /* Filter:2, Coefficient: 17 */
	CONVERT_COEFF(-23258), /* Filter:2, Coefficient: 18 */
	CONVERT_COEFF(-355924), /* Filter:2, Coefficient: 19 */
	CONVERT_COEFF(-679709), /* Filter:2, Coefficient: 20 */
	CONVERT_COEFF(1409874), /* Filter:1, Coefficient: 17 */
	CONVERT_COEFF(1531750), /* Filter:1, Coefficient: 18 */
	CONVERT_COEFF(1343813), /* Filter:1, Coefficient: 19 */
	CONVERT_COEFF(821550), /* Filter:1, Coefficient: 20 */

	/* Filter #6, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(30884), /* Filter:4, Coefficient: 21 */
	CONVERT_COEFF(36752), /* Filter:4, Coefficient: 22 */
	CONVERT_COEFF(35500), /* Filter:4, Coefficient: 23 */
	CONVERT_COEFF(26152), /* Filter:4, Coefficient: 24 */
	CONVERT_COEFF(-89716), /* Filter:3, Coefficient: 21 */
	CONVERT_COEFF(3439), /* Filter:3, Coefficient: 22 */
	CONVERT_COEFF(113663), /* Filter:3, Coefficient: 23 */
	CONVERT_COEFF(219469), /* Filter:3, Coefficient: 24 */
	CONVERT_COEFF(-919754), /* Filter:2, Coefficient: 21 */
	CONVERT_COEFF(-1005668), /* Filter:2, Coefficient: 22 */
	CONVERT_COEFF(-888441), /* Filter:2, Coefficient: 23 */
	CONVERT_COEFF(-555451), /* Filter:2, Coefficient: 24 */
	CONVERT_COEFF(11087), /* Filter:1, Coefficient: 21 */
	CONVERT_COEFF(-967418), /* Filter:1, Coefficient: 22 */
	CONVERT_COEFF(-1932735), /* Filter:1, Coefficient: 23 */
	CONVERT_COEFF(-2671792), /* Filter:1, Coefficient: 24 */

	/* Filter #7, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(9393), /* Filter:4, Coefficient: 25 */
	CONVERT_COEFF(-12331), /* Filter:4, Coefficient: 26 */
	CONVERT_COEFF(-35074), /* Filter:4, Coefficient: 27 */
	CONVERT_COEFF(-54036), /* Filter:4, Coefficient: 28 */
	CONVERT_COEFF(296574), /* Filter:3, Coefficient: 25 */
	CONVERT_COEFF(322901), /* Filter:3, Coefficient: 26 */
	CONVERT_COEFF(283864), /* Filter:3, Coefficient: 27 */
	CONVERT_COEFF(176829), /* Filter:3, Coefficient: 28 */
	CONVERT_COEFF(-39793), /* Filter:2, Coefficient: 25 */
	CONVERT_COEFF(579126), /* Filter:2, Coefficient: 26 */
	CONVERT_COEFF(1185270), /* Filter:2, Coefficient: 27 */
	CONVERT_COEFF(1645395), /* Filter:2, Coefficient: 28 */
	CONVERT_COEFF(-2981375), /* Filter:1, Coefficient: 25 */
	CONVERT_COEFF(-2714977), /* Filter:1, Coefficient: 26 */
	CONVERT_COEFF(-1825131), /* Filter:1, Coefficient: 27 */
	CONVERT_COEFF(-391039), /* Filter:1, Coefficient: 28 */

	/* Filter #8, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-64488), /* Filter:4, Coefficient: 29 */
	CONVERT_COEFF(-62823), /* Filter:4, Coefficient: 30 */
	CONVERT_COEFF(-47496), /* Filter:4, Coefficient: 31 */
	CONVERT_COEFF(-19664), /* Filter:4, Coefficient: 32 */
	CONVERT_COEFF(13593), /* Filter:3, Coefficient: 29 */
	CONVERT_COEFF(-179927), /* Filter:3, Coefficient: 30 */
	CONVERT_COEFF(-367152), /* Filter:3, Coefficient: 31 */
	CONVERT_COEFF(-507125), /* Filter:3, Coefficient: 32 */
	CONVERT_COEFF(1835407), /* Filter:2, Coefficient: 29 */
	CONVERT_COEFF(1668894), /* Filter:2, Coefficient: 30 */
	CONVERT_COEFF(1121896), /* Filter:2, Coefficient: 31 */
	CONVERT_COEFF(247943), /* Filter:2, Coefficient: 32 */
	CONVERT_COEFF(1377035), /* Filter:1, Coefficient: 29 */
	CONVERT_COEFF(3161285), /* Filter:1, Coefficient: 30 */
	CONVERT_COEFF(4587051), /* Filter:1, Coefficient: 31 */
	CONVERT_COEFF(5293861), /* Filter:1, Coefficient: 32 */

	/* Filter #9, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(16678), /* Filter:4, Coefficient: 33 */
	CONVERT_COEFF(55149), /* Filter:4, Coefficient: 34 */
	CONVERT_COEFF(87997), /* Filter:4, Coefficient: 35 */
	CONVERT_COEFF(107539), /* Filter:4, Coefficient: 36 */
	CONVERT_COEFF(-562646), /* Filter:3, Coefficient: 33 */
	CONVERT_COEFF(-508764), /* Filter:3, Coefficient: 34 */
	CONVERT_COEFF(-339863), /* Filter:3, Coefficient: 35 */
	CONVERT_COEFF(-73626), /* Filter:3, Coefficient: 36 */
	CONVERT_COEFF(-821316), /* Filter:2, Coefficient: 33 */
	CONVERT_COEFF(-1892108), /* Filter:2, Coefficient: 34 */
	CONVERT_COEFF(-2740106), /* Filter:2, Coefficient: 35 */
	CONVERT_COEFF(-3153045), /* Filter:2, Coefficient: 36 */
	CONVERT_COEFF(5014601), /* Filter:1, Coefficient: 33 */
	CONVERT_COEFF(3647017), /* Filter:1, Coefficient: 34 */
	CONVERT_COEFF(1301187), /* Filter:1, Coefficient: 35 */
	CONVERT_COEFF(-1690645), /* Filter:1, Coefficient: 36 */

	/* Filter #10, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(107770), /* Filter:4, Coefficient: 37 */
	CONVERT_COEFF(85829), /* Filter:4, Coefficient: 38 */
	CONVERT_COEFF(42999), /* Filter:4, Coefficient: 39 */
	CONVERT_COEFF(-15023), /* Filter:4, Coefficient: 40 */
	CONVERT_COEFF(249339), /* Filter:3, Coefficient: 37 */
	CONVERT_COEFF(570716), /* Filter:3, Coefficient: 38 */
	CONVERT_COEFF(824034), /* Filter:3, Coefficient: 39 */
	CONVERT_COEFF(947166), /* Filter:3, Coefficient: 40 */
	CONVERT_COEFF(-2976939), /* Filter:2, Coefficient: 37 */
	CONVERT_COEFF(-2156713), /* Filter:2, Coefficient: 38 */
	CONVERT_COEFF(-762064), /* Filter:2, Coefficient: 39 */
	CONVERT_COEFF(1008723), /* Filter:2, Coefficient: 40 */
	CONVERT_COEFF(-4809380), /* Filter:1, Coefficient: 37 */
	CONVERT_COEFF(-7428596), /* Filter:1, Coefficient: 38 */
	CONVERT_COEFF(-8928025), /* Filter:1, Coefficient: 39 */
	CONVERT_COEFF(-8822328), /* Filter:1, Coefficient: 40 */

	/* Filter #11, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-78706), /* Filter:4, Coefficient: 41 */
	CONVERT_COEFF(-136113), /* Filter:4, Coefficient: 42 */
	CONVERT_COEFF(-174958), /* Filter:4, Coefficient: 43 */
	CONVERT_COEFF(-184971), /* Filter:4, Coefficient: 44 */
	CONVERT_COEFF(895533), /* Filter:3, Coefficient: 41 */
	CONVERT_COEFF(653455), /* Filter:3, Coefficient: 42 */
	CONVERT_COEFF(241140), /* Filter:3, Coefficient: 43 */
	CONVERT_COEFF(-284575), /* Filter:3, Coefficient: 44 */
	CONVERT_COEFF(2850676), /* Filter:2, Coefficient: 41 */
	CONVERT_COEFF(4398591), /* Filter:2, Coefficient: 42 */
	CONVERT_COEFF(5291841), /* Filter:2, Coefficient: 43 */
	CONVERT_COEFF(5246926), /* Filter:2, Coefficient: 44 */
	CONVERT_COEFF(-6880893), /* Filter:1, Coefficient: 41 */
	CONVERT_COEFF(-3213125), /* Filter:1, Coefficient: 42 */
	CONVERT_COEFF(1702668), /* Filter:1, Coefficient: 43 */
	CONVERT_COEFF(7060108), /* Filter:1, Coefficient: 44 */

	/* Filter #12, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-160114), /* Filter:4, Coefficient: 45 */
	CONVERT_COEFF(-100215), /* Filter:4, Coefficient: 46 */
	CONVERT_COEFF(-11645), /* Filter:4, Coefficient: 47 */
	CONVERT_COEFF(93176), /* Filter:4, Coefficient: 48 */
	CONVERT_COEFF(-836548), /* Filter:3, Coefficient: 45 */
	CONVERT_COEFF(-1310257), /* Filter:3, Coefficient: 46 */
	CONVERT_COEFF(-1601506), /* Filter:3, Coefficient: 47 */
	CONVERT_COEFF(-1625994), /* Filter:3, Coefficient: 48 */
	CONVERT_COEFF(4124428), /* Filter:2, Coefficient: 45 */
	CONVERT_COEFF(1976711), /* Filter:2, Coefficient: 46 */
	CONVERT_COEFF(-935042), /* Filter:2, Coefficient: 47 */
	CONVERT_COEFF(-4161104), /* Filter:2, Coefficient: 48 */
	CONVERT_COEFF(11836509), /* Filter:1, Coefficient: 45 */
	CONVERT_COEFF(14963038), /* Filter:1, Coefficient: 46 */
	CONVERT_COEFF(15527995), /* Filter:1, Coefficient: 47 */
	CONVERT_COEFF(12978729), /* Filter:1, Coefficient: 48 */

	/* Filter #13, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(197453), /* Filter:4, Coefficient: 49 */
	CONVERT_COEFF(282648), /* Filter:4, Coefficient: 50 */
	CONVERT_COEFF(331646), /* Filter:4, Coefficient: 51 */
	CONVERT_COEFF(331926), /* Filter:4, Coefficient: 52 */
	CONVERT_COEFF(-1337215), /* Filter:3, Coefficient: 49 */
	CONVERT_COEFF(-739195), /* Filter:3, Coefficient: 50 */
	CONVERT_COEFF(108785), /* Filter:3, Coefficient: 51 */
	CONVERT_COEFF(1096994), /* Filter:3, Coefficient: 52 */
	CONVERT_COEFF(-7120987), /* Filter:2, Coefficient: 49 */
	CONVERT_COEFF(-9191912), /* Filter:2, Coefficient: 50 */
	CONVERT_COEFF(-9814753), /* Filter:2, Coefficient: 51 */
	CONVERT_COEFF(-8599910), /* Filter:2, Coefficient: 52 */
	CONVERT_COEFF(7284205), /* Filter:1, Coefficient: 49 */
	CONVERT_COEFF(-976463), /* Filter:1, Coefficient: 50 */
	CONVERT_COEFF(-10624045), /* Filter:1, Coefficient: 51 */
	CONVERT_COEFF(-19996714), /* Filter:1, Coefficient: 52 */

	/* Filter #14, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(278128), /* Filter:4, Coefficient: 53 */
	CONVERT_COEFF(173526), /* Filter:4, Coefficient: 54 */
	CONVERT_COEFF(30081), /* Filter:4, Coefficient: 55 */
	CONVERT_COEFF(-133038), /* Filter:4, Coefficient: 56 */
	CONVERT_COEFF(2079471), /* Filter:3, Coefficient: 53 */
	CONVERT_COEFF(2895796), /* Filter:3, Coefficient: 54 */
	CONVERT_COEFF(3397039), /* Filter:3, Coefficient: 55 */
	CONVERT_COEFF(3471563), /* Filter:3, Coefficient: 56 */
	CONVERT_COEFF(-5414116), /* Filter:2, Coefficient: 53 */
	CONVERT_COEFF(-431194), /* Filter:2, Coefficient: 54 */
	CONVERT_COEFF(5865019), /* Filter:2, Coefficient: 55 */
	CONVERT_COEFF(12729600), /* Filter:2, Coefficient: 56 */
	CONVERT_COEFF(-27165458), /* Filter:1, Coefficient: 53 */
	CONVERT_COEFF(-30219478), /* Filter:1, Coefficient: 54 */
	CONVERT_COEFF(-27579070), /* Filter:1, Coefficient: 55 */
	CONVERT_COEFF(-18285419), /* Filter:1, Coefficient: 56 */

	/* Filter #15, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-292255), /* Filter:4, Coefficient: 57 */
	CONVERT_COEFF(-423276), /* Filter:4, Coefficient: 58 */
	CONVERT_COEFF(-505071), /* Filter:4, Coefficient: 59 */
	CONVERT_COEFF(-523436), /* Filter:4, Coefficient: 60 */
	CONVERT_COEFF(3066123), /* Filter:3, Coefficient: 57 */
	CONVERT_COEFF(2198356), /* Filter:3, Coefficient: 58 */
	CONVERT_COEFF(958050), /* Filter:3, Coefficient: 59 */
	CONVERT_COEFF(-503489), /* Filter:3, Coefficient: 60 */
	CONVERT_COEFF(19252509), /* Filter:2, Coefficient: 57 */
	CONVERT_COEFF(24488239), /* Filter:2, Coefficient: 58 */
	CONVERT_COEFF(27599287), /* Filter:2, Coefficient: 59 */
	CONVERT_COEFF(27990266), /* Filter:2, Coefficient: 60 */
	CONVERT_COEFF(-2217112), /* Filter:1, Coefficient: 57 */
	CONVERT_COEFF(19807628), /* Filter:1, Coefficient: 58 */
	CONVERT_COEFF(46067140), /* Filter:1, Coefficient: 59 */
	CONVERT_COEFF(74113281), /* Filter:1, Coefficient: 60 */

	/* Filter #16, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-473520), /* Filter:4, Coefficient: 61 */
	CONVERT_COEFF(-360852), /* Filter:4, Coefficient: 62 */
	CONVERT_COEFF(-200645), /* Filter:4, Coefficient: 63 */
	CONVERT_COEFF(-15496), /* Filter:4, Coefficient: 64 */
	CONVERT_COEFF(-1994632), /* Filter:3, Coefficient: 61 */
	CONVERT_COEFF(-3311919), /* Filter:3, Coefficient: 62 */
	CONVERT_COEFF(-4271266), /* Filter:3, Coefficient: 63 */
	CONVERT_COEFF(-4736706), /* Filter:3, Coefficient: 64 */
	CONVERT_COEFF(25410130), /* Filter:2, Coefficient: 61 */
	CONVERT_COEFF(20004573), /* Filter:2, Coefficient: 62 */
	CONVERT_COEFF(12308550), /* Filter:2, Coefficient: 63 */
	CONVERT_COEFF(3178633), /* Filter:2, Coefficient: 64 */
	CONVERT_COEFF(101068271), /* Filter:1, Coefficient: 61 */
	CONVERT_COEFF(124000002), /* Filter:1, Coefficient: 62 */
	CONVERT_COEFF(140320208), /* Filter:1, Coefficient: 63 */
	CONVERT_COEFF(148144605), /* Filter:1, Coefficient: 64 */

	/* Filter #17, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(168169), /* Filter:4, Coefficient: 65 */
	CONVERT_COEFF(324285), /* Filter:4, Coefficient: 66 */
	CONVERT_COEFF(431264), /* Filter:4, Coefficient: 67 */
	CONVERT_COEFF(475348), /* Filter:4, Coefficient: 68 */
	CONVERT_COEFF(-4641842), /* Filter:3, Coefficient: 65 */
	CONVERT_COEFF(-4000373), /* Filter:3, Coefficient: 66 */
	CONVERT_COEFF(-2903906), /* Filter:3, Coefficient: 67 */
	CONVERT_COEFF(-1507432), /* Filter:3, Coefficient: 68 */
	CONVERT_COEFF(-6325048), /* Filter:2, Coefficient: 65 */
	CONVERT_COEFF(-15089017), /* Filter:2, Coefficient: 66 */
	CONVERT_COEFF(-22105139), /* Filter:2, Coefficient: 67 */
	CONVERT_COEFF(-26612611), /* Filter:2, Coefficient: 68 */
	CONVERT_COEFF(146558923), /* Filter:1, Coefficient: 65 */
	CONVERT_COEFF(135748984), /* Filter:1, Coefficient: 66 */
	CONVERT_COEFF(116974214), /* Filter:1, Coefficient: 67 */
	CONVERT_COEFF(92388797), /* Filter:1, Coefficient: 68 */

	/* Filter #18, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(452628), /* Filter:4, Coefficient: 69 */
	CONVERT_COEFF(369409), /* Filter:4, Coefficient: 70 */
	CONVERT_COEFF(240897), /* Filter:4, Coefficient: 71 */
	CONVERT_COEFF(88460), /* Filter:4, Coefficient: 72 */
	CONVERT_COEFF(-4920), /* Filter:3, Coefficient: 69 */
	CONVERT_COEFF(1400822), /* Filter:3, Coefficient: 70 */
	CONVERT_COEFF(2528987), /* Filter:3, Coefficient: 71 */
	CONVERT_COEFF(3247243), /* Filter:3, Coefficient: 72 */
	CONVERT_COEFF(-28200972), /* Filter:2, Coefficient: 69 */
	CONVERT_COEFF(-26858406), /* Filter:2, Coefficient: 70 */
	CONVERT_COEFF(-22958846), /* Filter:2, Coefficient: 71 */
	CONVERT_COEFF(-17191493), /* Filter:2, Coefficient: 72 */
	CONVERT_COEFF(64738752), /* Filter:1, Coefficient: 69 */
	CONVERT_COEFF(36982432), /* Filter:1, Coefficient: 70 */
	CONVERT_COEFF(11893275), /* Filter:1, Coefficient: 71 */
	CONVERT_COEFF(-8295001), /* Filter:1, Coefficient: 72 */

	/* Filter #19, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-64023), /* Filter:4, Coefficient: 73 */
	CONVERT_COEFF(-194079), /* Filter:4, Coefficient: 74 */
	CONVERT_COEFF(-284155), /* Filter:4, Coefficient: 75 */
	CONVERT_COEFF(-324136), /* Filter:4, Coefficient: 76 */
	CONVERT_COEFF(3489610), /* Filter:3, Coefficient: 73 */
	CONVERT_COEFF(3263104), /* Filter:3, Coefficient: 74 */
	CONVERT_COEFF(2642488), /* Filter:3, Coefficient: 75 */
	CONVERT_COEFF(1754510), /* Filter:3, Coefficient: 76 */
	CONVERT_COEFF(-10445713), /* Filter:2, Coefficient: 73 */
	CONVERT_COEFF(-3671194), /* Filter:2, Coefficient: 74 */
	CONVERT_COEFF(2263465), /* Filter:2, Coefficient: 75 */
	CONVERT_COEFF(6691197), /* Filter:2, Coefficient: 76 */
	CONVERT_COEFF(-22148960), /* Filter:1, Coefficient: 73 */
	CONVERT_COEFF(-29166675), /* Filter:1, Coefficient: 74 */
	CONVERT_COEFF(-29766384), /* Filter:1, Coefficient: 75 */
	CONVERT_COEFF(-25142508), /* Filter:1, Coefficient: 76 */

	/* Filter #20, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-312505), /* Filter:4, Coefficient: 77 */
	CONVERT_COEFF(-255992), /* Filter:4, Coefficient: 78 */
	CONVERT_COEFF(-167880), /* Filter:4, Coefficient: 79 */
	CONVERT_COEFF(-65312), /* Filter:4, Coefficient: 80 */
	CONVERT_COEFF(754742), /* Filter:3, Coefficient: 77 */
	CONVERT_COEFF(-198757), /* Filter:3, Coefficient: 78 */
	CONVERT_COEFF(-970391), /* Filter:3, Coefficient: 79 */
	CONVERT_COEFF(-1466526), /* Filter:3, Coefficient: 80 */
	CONVERT_COEFF(9227960), /* Filter:2, Coefficient: 77 */
	CONVERT_COEFF(9804583), /* Filter:2, Coefficient: 78 */
	CONVERT_COEFF(8647121), /* Filter:2, Coefficient: 79 */
	CONVERT_COEFF(6212516), /* Filter:2, Coefficient: 80 */
	CONVERT_COEFF(-17019531), /* Filter:1, Coefficient: 77 */
	CONVERT_COEFF(-7348727), /* Filter:1, Coefficient: 78 */
	CONVERT_COEFF(2000942), /* Filter:1, Coefficient: 79 */
	CONVERT_COEFF(9509007), /* Filter:1, Coefficient: 80 */

	/* Filter #21, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(33847), /* Filter:4, Coefficient: 81 */
	CONVERT_COEFF(114046), /* Filter:4, Coefficient: 82 */
	CONVERT_COEFF(164441), /* Filter:4, Coefficient: 83 */
	CONVERT_COEFF(180290), /* Filter:4, Coefficient: 84 */
	CONVERT_COEFF(-1646620), /* Filter:3, Coefficient: 81 */
	CONVERT_COEFF(-1524717), /* Filter:3, Coefficient: 82 */
	CONVERT_COEFF(-1161761), /* Filter:3, Coefficient: 83 */
	CONVERT_COEFF(-650773), /* Filter:3, Coefficient: 84 */
	CONVERT_COEFF(3093389), /* Filter:2, Coefficient: 81 */
	CONVERT_COEFF(-89994), /* Filter:2, Coefficient: 82 */
	CONVERT_COEFF(-2791699), /* Filter:2, Coefficient: 83 */
	CONVERT_COEFF(-4619639), /* Filter:2, Coefficient: 84 */
	CONVERT_COEFF(14188512), /* Filter:1, Coefficient: 81 */
	CONVERT_COEFF(15667833), /* Filter:1, Coefficient: 82 */
	CONVERT_COEFF(14165998), /* Filter:1, Coefficient: 83 */
	CONVERT_COEFF(10376123), /* Filter:1, Coefficient: 84 */

	/* Filter #22, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(163170), /* Filter:4, Coefficient: 85 */
	CONVERT_COEFF(120081), /* Filter:4, Coefficient: 86 */
	CONVERT_COEFF(61686), /* Filter:4, Coefficient: 87 */
	CONVERT_COEFF(66), /* Filter:4, Coefficient: 88 */
	CONVERT_COEFF(-98000), /* Filter:3, Coefficient: 85 */
	CONVERT_COEFF(396363), /* Filter:3, Coefficient: 86 */
	CONVERT_COEFF(754499), /* Filter:3, Coefficient: 87 */
	CONVERT_COEFF(931769), /* Filter:3, Coefficient: 88 */
	CONVERT_COEFF(-5381371), /* Filter:2, Coefficient: 85 */
	CONVERT_COEFF(-5091650), /* Filter:2, Coefficient: 86 */
	CONVERT_COEFF(-3944213), /* Filter:2, Coefficient: 87 */
	CONVERT_COEFF(-2256257), /* Filter:2, Coefficient: 88 */
	CONVERT_COEFF(5285564), /* Filter:1, Coefficient: 85 */
	CONVERT_COEFF(-30634), /* Filter:1, Coefficient: 86 */
	CONVERT_COEFF(-4605459), /* Filter:1, Coefficient: 87 */
	CONVERT_COEFF(-7732849), /* Filter:1, Coefficient: 88 */

	/* Filter #23, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-53544), /* Filter:4, Coefficient: 89 */
	CONVERT_COEFF(-90654), /* Filter:4, Coefficient: 90 */
	CONVERT_COEFF(-106725), /* Filter:4, Coefficient: 91 */
	CONVERT_COEFF(-101537), /* Filter:4, Coefficient: 92 */
	CONVERT_COEFF(920613), /* Filter:3, Coefficient: 89 */
	CONVERT_COEFF(747526), /* Filter:3, Coefficient: 90 */
	CONVERT_COEFF(464354), /* Filter:3, Coefficient: 91 */
	CONVERT_COEFF(136043), /* Filter:3, Coefficient: 92 */
	CONVERT_COEFF(-398040), /* Filter:2, Coefficient: 89 */
	CONVERT_COEFF(1278522), /* Filter:2, Coefficient: 90 */
	CONVERT_COEFF(2499600), /* Filter:2, Coefficient: 91 */
	CONVERT_COEFF(3108250), /* Filter:2, Coefficient: 92 */
	CONVERT_COEFF(-9056522), /* Filter:1, Coefficient: 89 */
	CONVERT_COEFF(-8586783), /* Filter:1, Coefficient: 90 */
	CONVERT_COEFF(-6650839), /* Filter:1, Coefficient: 91 */
	CONVERT_COEFF(-3793297), /* Filter:1, Coefficient: 92 */

	/* Filter #24, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-78733), /* Filter:4, Coefficient: 93 */
	CONVERT_COEFF(-44705), /* Filter:4, Coefficient: 94 */
	CONVERT_COEFF(-7104), /* Filter:4, Coefficient: 95 */
	CONVERT_COEFF(26726), /* Filter:4, Coefficient: 96 */
	CONVERT_COEFF(-172582), /* Filter:3, Coefficient: 93 */
	CONVERT_COEFF(-408494), /* Filter:3, Coefficient: 94 */
	CONVERT_COEFF(-538640), /* Filter:3, Coefficient: 95 */
	CONVERT_COEFF(-553497), /* Filter:3, Coefficient: 96 */
	CONVERT_COEFF(3077680), /* Filter:2, Coefficient: 93 */
	CONVERT_COEFF(2499519), /* Filter:2, Coefficient: 94 */
	CONVERT_COEFF(1552119), /* Filter:2, Coefficient: 95 */
	CONVERT_COEFF(456990), /* Filter:2, Coefficient: 96 */
	CONVERT_COEFF(-650487), /* Filter:1, Coefficient: 93 */
	CONVERT_COEFF(2175698), /* Filter:1, Coefficient: 94 */
	CONVERT_COEFF(4221669), /* Filter:1, Coefficient: 95 */
	CONVERT_COEFF(5227614), /* Filter:1, Coefficient: 96 */

	/* Filter #25, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(51065), /* Filter:4, Coefficient: 97 */
	CONVERT_COEFF(62685), /* Filter:4, Coefficient: 98 */
	CONVERT_COEFF(61148), /* Filter:4, Coefficient: 99 */
	CONVERT_COEFF(48532), /* Filter:4, Coefficient: 100 */
	CONVERT_COEFF(-465874), /* Filter:3, Coefficient: 97 */
	CONVERT_COEFF(-305721), /* Filter:3, Coefficient: 98 */
	CONVERT_COEFF(-112385), /* Filter:3, Coefficient: 99 */
	CONVERT_COEFF(73933), /* Filter:3, Coefficient: 100 */
	CONVERT_COEFF(-567215), /* Filter:2, Coefficient: 97 */
	CONVERT_COEFF(-1344377), /* Filter:2, Coefficient: 98 */
	CONVERT_COEFF(-1767688), /* Filter:2, Coefficient: 99 */
	CONVERT_COEFF(-1810088), /* Filter:2, Coefficient: 100 */
	CONVERT_COEFF(5157406), /* Filter:1, Coefficient: 97 */
	CONVERT_COEFF(4175038), /* Filter:1, Coefficient: 98 */
	CONVERT_COEFF(2587411), /* Filter:1, Coefficient: 99 */
	CONVERT_COEFF(768423), /* Filter:1, Coefficient: 100 */

	/* Filter #26, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(28723), /* Filter:4, Coefficient: 101 */
	CONVERT_COEFF(6440), /* Filter:4, Coefficient: 102 */
	CONVERT_COEFF(-13774), /* Filter:4, Coefficient: 103 */
	CONVERT_COEFF(-28398), /* Filter:4, Coefficient: 104 */
	CONVERT_COEFF(219811), /* Filter:3, Coefficient: 101 */
	CONVERT_COEFF(303981), /* Filter:3, Coefficient: 102 */
	CONVERT_COEFF(319710), /* Filter:3, Coefficient: 103 */
	CONVERT_COEFF(274094), /* Filter:3, Coefficient: 104 */
	CONVERT_COEFF(-1518489), /* Filter:2, Coefficient: 101 */
	CONVERT_COEFF(-994890), /* Filter:2, Coefficient: 102 */
	CONVERT_COEFF(-369667), /* Filter:2, Coefficient: 103 */
	CONVERT_COEFF(226880), /* Filter:2, Coefficient: 104 */
	CONVERT_COEFF(-919124), /* Filter:1, Coefficient: 101 */
	CONVERT_COEFF(-2188898), /* Filter:1, Coefficient: 102 */
	CONVERT_COEFF(-2873130), /* Filter:1, Coefficient: 103 */
	CONVERT_COEFF(-2936618), /* Filter:1, Coefficient: 104 */

	/* Filter #27, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-35477), /* Filter:4, Coefficient: 105 */
	CONVERT_COEFF(-34783), /* Filter:4, Coefficient: 106 */
	CONVERT_COEFF(-27623), /* Filter:4, Coefficient: 107 */
	CONVERT_COEFF(-16364), /* Filter:4, Coefficient: 108 */
	CONVERT_COEFF(184790), /* Filter:3, Coefficient: 105 */
	CONVERT_COEFF(75160), /* Filter:3, Coefficient: 106 */
	CONVERT_COEFF(-31027), /* Filter:3, Coefficient: 107 */
	CONVERT_COEFF(-114243), /* Filter:3, Coefficient: 108 */
	CONVERT_COEFF(689048), /* Filter:2, Coefficient: 105 */
	CONVERT_COEFF(952149), /* Filter:2, Coefficient: 106 */
	CONVERT_COEFF(998741), /* Filter:2, Coefficient: 107 */
	CONVERT_COEFF(854886), /* Filter:2, Coefficient: 108 */
	CONVERT_COEFF(-2463839), /* Filter:1, Coefficient: 105 */
	CONVERT_COEFF(-1625344), /* Filter:1, Coefficient: 106 */
	CONVERT_COEFF(-632765), /* Filter:1, Coefficient: 107 */
	CONVERT_COEFF(307301), /* Filter:1, Coefficient: 108 */

	/* Filter #28, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-3819), /* Filter:4, Coefficient: 109 */
	CONVERT_COEFF(7377), /* Filter:4, Coefficient: 110 */
	CONVERT_COEFF(15261), /* Filter:4, Coefficient: 111 */
	CONVERT_COEFF(18835), /* Filter:4, Coefficient: 112 */
	CONVERT_COEFF(-162364), /* Filter:3, Coefficient: 109 */
	CONVERT_COEFF(-171920), /* Filter:3, Coefficient: 110 */
	CONVERT_COEFF(-147466), /* Filter:3, Coefficient: 111 */
	CONVERT_COEFF(-99442), /* Filter:3, Coefficient: 112 */
	CONVERT_COEFF(578544), /* Filter:2, Coefficient: 109 */
	CONVERT_COEFF(243498), /* Filter:2, Coefficient: 110 */
	CONVERT_COEFF(-77378), /* Filter:2, Coefficient: 111 */
	CONVERT_COEFF(-326115), /* Filter:2, Coefficient: 112 */
	CONVERT_COEFF(1031494), /* Filter:1, Coefficient: 109 */
	CONVERT_COEFF(1443736), /* Filter:1, Coefficient: 110 */
	CONVERT_COEFF(1522566), /* Filter:1, Coefficient: 111 */
	CONVERT_COEFF(1312875), /* Filter:1, Coefficient: 112 */

	/* Filter #29, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(18119), /* Filter:4, Coefficient: 113 */
	CONVERT_COEFF(13991), /* Filter:4, Coefficient: 114 */
	CONVERT_COEFF(7872), /* Filter:4, Coefficient: 115 */
	CONVERT_COEFF(1348), /* Filter:4, Coefficient: 116 */
	CONVERT_COEFF(-41184), /* Filter:3, Coefficient: 113 */
	CONVERT_COEFF(14196), /* Filter:3, Coefficient: 114 */
	CONVERT_COEFF(56400), /* Filter:3, Coefficient: 115 */
	CONVERT_COEFF(79557), /* Filter:3, Coefficient: 116 */
	CONVERT_COEFF(-468515), /* Filter:2, Coefficient: 113 */
	CONVERT_COEFF(-496901), /* Filter:2, Coefficient: 114 */
	CONVERT_COEFF(-427126), /* Filter:2, Coefficient: 115 */
	CONVERT_COEFF(-291358), /* Filter:2, Coefficient: 116 */
	CONVERT_COEFF(906078), /* Filter:1, Coefficient: 113 */
	CONVERT_COEFF(414464), /* Filter:1, Coefficient: 114 */
	CONVERT_COEFF(-54245), /* Filter:1, Coefficient: 115 */
	CONVERT_COEFF(-417064), /* Filter:1, Coefficient: 116 */

	/* Filter #30, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-4186), /* Filter:4, Coefficient: 117 */
	CONVERT_COEFF(-7781), /* Filter:4, Coefficient: 118 */
	CONVERT_COEFF(-9064), /* Filter:4, Coefficient: 119 */
	CONVERT_COEFF(-8214), /* Filter:4, Coefficient: 120 */
	CONVERT_COEFF(82669), /* Filter:3, Coefficient: 117 */
	CONVERT_COEFF(68978), /* Filter:3, Coefficient: 118 */
	CONVERT_COEFF(44558), /* Filter:3, Coefficient: 119 */
	CONVERT_COEFF(16546), /* Filter:3, Coefficient: 120 */
	CONVERT_COEFF(-128765), /* Filter:2, Coefficient: 117 */
	CONVERT_COEFF(23638), /* Filter:2, Coefficient: 118 */
	CONVERT_COEFF(138103), /* Filter:2, Coefficient: 119 */
	CONVERT_COEFF(200098), /* Filter:2, Coefficient: 120 */
	CONVERT_COEFF(-627465), /* Filter:1, Coefficient: 117 */
	CONVERT_COEFF(-677691), /* Filter:1, Coefficient: 118 */
	CONVERT_COEFF(-592807), /* Filter:1, Coefficient: 119 */
	CONVERT_COEFF(-419175), /* Filter:1, Coefficient: 120 */

	/* Filter #31, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(-5833), /* Filter:4, Coefficient: 121 */
	CONVERT_COEFF(-2739), /* Filter:4, Coefficient: 122 */
	CONVERT_COEFF(251), /* Filter:4, Coefficient: 123 */
	CONVERT_COEFF(2500), /* Filter:4, Coefficient: 124 */
	CONVERT_COEFF(-8556), /* Filter:3, Coefficient: 121 */
	CONVERT_COEFF(-26141), /* Filter:3, Coefficient: 122 */
	CONVERT_COEFF(-34137), /* Filter:3, Coefficient: 123 */
	CONVERT_COEFF(-32972), /* Filter:3, Coefficient: 124 */
	CONVERT_COEFF(208777), /* Filter:2, Coefficient: 121 */
	CONVERT_COEFF(174473), /* Filter:2, Coefficient: 122 */
	CONVERT_COEFF(114277), /* Filter:2, Coefficient: 123 */
	CONVERT_COEFF(46990), /* Filter:2, Coefficient: 124 */
	CONVERT_COEFF(-210728), /* Filter:1, Coefficient: 121 */
	CONVERT_COEFF(-16339), /* Filter:1, Coefficient: 122 */
	CONVERT_COEFF(129243), /* Filter:1, Coefficient: 123 */
	CONVERT_COEFF(209618), /* Filter:1, Coefficient: 124 */

	/* Filter #32, conversion from 48000 Hz to 8000 Hz */

	CONVERT_COEFF(3662), /* Filter:4, Coefficient: 125 */
	CONVERT_COEFF(3707), /* Filter:4, Coefficient: 126 */
	CONVERT_COEFF(2870), /* Filter:4, Coefficient: 127 */
	CONVERT_COEFF(335927), /* Filter:4, Coefficient: 128 */
	CONVERT_COEFF(-25001), /* Filter:3, Coefficient: 125 */
	CONVERT_COEFF(-13600), /* Filter:3, Coefficient: 126 */
	CONVERT_COEFF(-2195), /* Filter:3, Coefficient: 127 */
	CONVERT_COEFF(-458842), /* Filter:3, Coefficient: 128 */
	CONVERT_COEFF(-11331), /* Filter:2, Coefficient: 125 */
	CONVERT_COEFF(-50338), /* Filter:2, Coefficient: 126 */
	CONVERT_COEFF(-66501), /* Filter:2, Coefficient: 127 */
	CONVERT_COEFF(55557), /* Filter:2, Coefficient: 128 */
	CONVERT_COEFF(226117), /* Filter:1, Coefficient: 125 */
	CONVERT_COEFF(193432), /* Filter:1, Coefficient: 126 */
	CONVERT_COEFF(133189), /* Filter:1, Coefficient: 127 */
	CONVERT_COEFF(67358), /* Filter:1, Coefficient: 128 */
};
