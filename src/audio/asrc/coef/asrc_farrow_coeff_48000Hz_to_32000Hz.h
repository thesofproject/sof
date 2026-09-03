/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2019 Intel Corporation. All rights reserved.
 */

/* Conversion from 48000 Hz to 32000 Hz */
/* NUM_FILTERS=6, FILTER_LENGTH=80, alpha=8.000000, gamma=0.452000 */

__cold_rodata static const int32_t coeff48000to32000[] = {
/* Filter #6, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-11561), /* Filter:6, Coefficient: 1 */
CONVERT_COEFF(15747), /* Filter:6, Coefficient: 2 */
CONVERT_COEFF(9801), /* Filter:5, Coefficient: 1 */
CONVERT_COEFF(-57987), /* Filter:5, Coefficient: 2 */
CONVERT_COEFF(46237), /* Filter:4, Coefficient: 1 */
CONVERT_COEFF(-16129), /* Filter:4, Coefficient: 2 */
CONVERT_COEFF(9592), /* Filter:3, Coefficient: 1 */
CONVERT_COEFF(90049), /* Filter:3, Coefficient: 2 */
CONVERT_COEFF(-56068), /* Filter:2, Coefficient: 1 */
CONVERT_COEFF(83510), /* Filter:2, Coefficient: 2 */
CONVERT_COEFF(-22379), /* Filter:1, Coefficient: 1 */
CONVERT_COEFF(-24379), /* Filter:1, Coefficient: 2 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(5105), /* Filter:6, Coefficient: 3 */
CONVERT_COEFF(-38743), /* Filter:6, Coefficient: 4 */
CONVERT_COEFF(55855), /* Filter:5, Coefficient: 3 */
CONVERT_COEFF(57347), /* Filter:5, Coefficient: 4 */
CONVERT_COEFF(-97341), /* Filter:4, Coefficient: 3 */
CONVERT_COEFF(155756), /* Filter:4, Coefficient: 4 */
CONVERT_COEFF(-143614), /* Filter:3, Coefficient: 3 */
CONVERT_COEFF(-53049), /* Filter:3, Coefficient: 4 */
CONVERT_COEFF(61872), /* Filter:2, Coefficient: 3 */
CONVERT_COEFF(-268865), /* Filter:2, Coefficient: 4 */
CONVERT_COEFF(90814), /* Filter:1, Coefficient: 3 */
CONVERT_COEFF(-27309), /* Filter:1, Coefficient: 4 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(32278), /* Filter:6, Coefficient: 5 */
CONVERT_COEFF(39046), /* Filter:6, Coefficient: 6 */
CONVERT_COEFF(-175841), /* Filter:5, Coefficient: 5 */
CONVERT_COEFF(72860), /* Filter:5, Coefficient: 6 */
CONVERT_COEFF(35097), /* Filter:4, Coefficient: 5 */
CONVERT_COEFF(-344433), /* Filter:4, Coefficient: 6 */
CONVERT_COEFF(365130), /* Filter:3, Coefficient: 5 */
CONVERT_COEFF(-248936), /* Filter:3, Coefficient: 6 */
CONVERT_COEFF(128732), /* Filter:2, Coefficient: 5 */
CONVERT_COEFF(422344), /* Filter:2, Coefficient: 6 */
CONVERT_COEFF(-174868), /* Filter:1, Coefficient: 5 */
CONVERT_COEFF(210533), /* Filter:1, Coefficient: 6 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-95870), /* Filter:6, Coefficient: 7 */
CONVERT_COEFF(23105), /* Filter:6, Coefficient: 8 */
CONVERT_COEFF(247915), /* Filter:5, Coefficient: 7 */
CONVERT_COEFF(-361409), /* Filter:5, Coefficient: 8 */
CONVERT_COEFF(268897), /* Filter:4, Coefficient: 7 */
CONVERT_COEFF(365707), /* Filter:4, Coefficient: 8 */
CONVERT_COEFF(-457784), /* Filter:3, Coefficient: 7 */
CONVERT_COEFF(858658), /* Filter:3, Coefficient: 8 */
CONVERT_COEFF(-623551), /* Filter:2, Coefficient: 7 */
CONVERT_COEFF(-218850), /* Filter:2, Coefficient: 8 */
CONVERT_COEFF(151417), /* Filter:1, Coefficient: 7 */
CONVERT_COEFF(-508987), /* Filter:1, Coefficient: 8 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(138684), /* Filter:6, Coefficient: 9 */
CONVERT_COEFF(-160549), /* Filter:6, Coefficient: 10 */
CONVERT_COEFF(-88230), /* Filter:5, Coefficient: 9 */
CONVERT_COEFF(670200), /* Filter:5, Coefficient: 10 */
CONVERT_COEFF(-792638), /* Filter:4, Coefficient: 9 */
CONVERT_COEFF(86409), /* Filter:4, Coefficient: 10 */
CONVERT_COEFF(41857), /* Filter:3, Coefficient: 9 */
CONVERT_COEFF(-1469440), /* Filter:3, Coefficient: 10 */
CONVERT_COEFF(1266645), /* Filter:2, Coefficient: 9 */
CONVERT_COEFF(-690167), /* Filter:2, Coefficient: 10 */
CONVERT_COEFF(158227), /* Filter:1, Coefficient: 9 */
CONVERT_COEFF(724561), /* Filter:1, Coefficient: 10 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-79806), /* Filter:6, Coefficient: 11 */
CONVERT_COEFF(315837), /* Filter:6, Coefficient: 12 */
CONVERT_COEFF(-440690), /* Filter:5, Coefficient: 11 */
CONVERT_COEFF(-669306), /* Filter:5, Coefficient: 12 */
CONVERT_COEFF(1208069), /* Filter:4, Coefficient: 11 */
CONVERT_COEFF(-1151663), /* Filter:4, Coefficient: 12 */
CONVERT_COEFF(1160033), /* Filter:3, Coefficient: 11 */
CONVERT_COEFF(1367901), /* Filter:3, Coefficient: 12 */
CONVERT_COEFF(-1490906), /* Filter:2, Coefficient: 11 */
CONVERT_COEFF(2295682), /* Filter:2, Coefficient: 12 */
CONVERT_COEFF(-839003), /* Filter:1, Coefficient: 11 */
CONVERT_COEFF(-482312), /* Filter:1, Coefficient: 12 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-143037), /* Filter:6, Coefficient: 13 */
CONVERT_COEFF(-351812), /* Filter:6, Coefficient: 14 */
CONVERT_COEFF(1230829), /* Filter:5, Coefficient: 13 */
CONVERT_COEFF(-10053), /* Filter:5, Coefficient: 14 */
CONVERT_COEFF(-910652), /* Filter:4, Coefficient: 13 */
CONVERT_COEFF(2480789), /* Filter:4, Coefficient: 14 */
CONVERT_COEFF(-2897785), /* Filter:3, Coefficient: 13 */
CONVERT_COEFF(246665), /* Filter:3, Coefficient: 14 */
CONVERT_COEFF(473707), /* Filter:2, Coefficient: 13 */
CONVERT_COEFF(-3847887), /* Filter:2, Coefficient: 14 */
CONVERT_COEFF(1676174), /* Filter:1, Coefficient: 13 */
CONVERT_COEFF(-570776), /* Filter:1, Coefficient: 14 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(494431), /* Filter:6, Coefficient: 15 */
CONVERT_COEFF(110738), /* Filter:6, Coefficient: 16 */
CONVERT_COEFF(-1802066), /* Filter:5, Coefficient: 15 */
CONVERT_COEFF(1456443), /* Filter:5, Coefficient: 16 */
CONVERT_COEFF(-641633), /* Filter:4, Coefficient: 15 */
CONVERT_COEFF(-3110197), /* Filter:4, Coefficient: 16 */
CONVERT_COEFF(4107125), /* Filter:3, Coefficient: 15 */
CONVERT_COEFF(-3569996), /* Filter:3, Coefficient: 16 */
CONVERT_COEFF(2297303), /* Filter:2, Coefficient: 15 */
CONVERT_COEFF(3846674), /* Filter:2, Coefficient: 16 */
CONVERT_COEFF(-2053116), /* Filter:1, Coefficient: 15 */
CONVERT_COEFF(2402095), /* Filter:1, Coefficient: 16 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-790086), /* Filter:6, Coefficient: 17 */
CONVERT_COEFF(463549), /* Filter:6, Coefficient: 18 */
CONVERT_COEFF(1420579), /* Filter:5, Coefficient: 17 */
CONVERT_COEFF(-3164667), /* Filter:5, Coefficient: 18 */
CONVERT_COEFF(3363870), /* Filter:4, Coefficient: 17 */
CONVERT_COEFF(1795377), /* Filter:4, Coefficient: 18 */
CONVERT_COEFF(-3143459), /* Filter:3, Coefficient: 17 */
CONVERT_COEFF(7478803), /* Filter:3, Coefficient: 18 */
CONVERT_COEFF(-6253867), /* Filter:2, Coefficient: 17 */
CONVERT_COEFF(-704435), /* Filter:2, Coefficient: 18 */
CONVERT_COEFF(1135780), /* Filter:1, Coefficient: 17 */
CONVERT_COEFF(-4267272), /* Filter:1, Coefficient: 18 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(736606), /* Filter:6, Coefficient: 19 */
CONVERT_COEFF(-1203048), /* Filter:6, Coefficient: 20 */
CONVERT_COEFF(474626), /* Filter:5, Coefficient: 19 */
CONVERT_COEFF(3997758), /* Filter:5, Coefficient: 20 */
CONVERT_COEFF(-6095509), /* Filter:4, Coefficient: 19 */
CONVERT_COEFF(2170913), /* Filter:4, Coefficient: 20 */
CONVERT_COEFF(-1291163), /* Filter:3, Coefficient: 19 */
CONVERT_COEFF(-9390934), /* Filter:3, Coefficient: 20 */
CONVERT_COEFF(9301477), /* Filter:2, Coefficient: 19 */
CONVERT_COEFF(-6005932), /* Filter:2, Coefficient: 20 */
CONVERT_COEFF(1601388), /* Filter:1, Coefficient: 19 */
CONVERT_COEFF(4727524), /* Filter:1, Coefficient: 20 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-85369), /* Filter:6, Coefficient: 21 */
CONVERT_COEFF(1689090), /* Filter:6, Coefficient: 22 */
CONVERT_COEFF(-3695078), /* Filter:5, Coefficient: 21 */
CONVERT_COEFF(-2619412), /* Filter:5, Coefficient: 22 */
CONVERT_COEFF(6715445), /* Filter:4, Coefficient: 21 */
CONVERT_COEFF(-8010806), /* Filter:4, Coefficient: 22 */
CONVERT_COEFF(8830976), /* Filter:3, Coefficient: 21 */
CONVERT_COEFF(6179129), /* Filter:3, Coefficient: 22 */
CONVERT_COEFF(-8288130), /* Filter:2, Coefficient: 21 */
CONVERT_COEFF(14331732), /* Filter:2, Coefficient: 22 */
CONVERT_COEFF(-5703839), /* Filter:1, Coefficient: 21 */
CONVERT_COEFF(-2226042), /* Filter:1, Coefficient: 22 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-1153692), /* Filter:6, Coefficient: 23 */
CONVERT_COEFF(-1392092), /* Filter:6, Coefficient: 24 */
CONVERT_COEFF(6934895), /* Filter:5, Coefficient: 23 */
CONVERT_COEFF(-1671143), /* Filter:5, Coefficient: 24 */
CONVERT_COEFF(-3066771), /* Filter:4, Coefficient: 23 */
CONVERT_COEFF(13022550), /* Filter:4, Coefficient: 24 */
CONVERT_COEFF(-16519290), /* Filter:3, Coefficient: 23 */
CONVERT_COEFF(3931355), /* Filter:3, Coefficient: 24 */
CONVERT_COEFF(596302), /* Filter:2, Coefficient: 23 */
CONVERT_COEFF(-19674765), /* Filter:2, Coefficient: 24 */
CONVERT_COEFF(9343886), /* Filter:1, Coefficient: 23 */
CONVERT_COEFF(-3864751), /* Filter:1, Coefficient: 24 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(2561676), /* Filter:6, Coefficient: 25 */
CONVERT_COEFF(-41718), /* Filter:6, Coefficient: 26 */
CONVERT_COEFF(-7983534), /* Filter:5, Coefficient: 25 */
CONVERT_COEFF(8090613), /* Filter:5, Coefficient: 26 */
CONVERT_COEFF(-5546048), /* Filter:4, Coefficient: 25 */
CONVERT_COEFF(-13215426), /* Filter:4, Coefficient: 26 */
CONVERT_COEFF(19154324), /* Filter:3, Coefficient: 25 */
CONVERT_COEFF(-19288118), /* Filter:3, Coefficient: 26 */
CONVERT_COEFF(13651643), /* Filter:2, Coefficient: 25 */
CONVERT_COEFF(16170479), /* Filter:2, Coefficient: 26 */
CONVERT_COEFF(-9649047), /* Filter:1, Coefficient: 25 */
CONVERT_COEFF(12189269), /* Filter:1, Coefficient: 26 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-3351923), /* Filter:6, Coefficient: 27 */
CONVERT_COEFF(2484217), /* Filter:6, Coefficient: 28 */
CONVERT_COEFF(4635953), /* Filter:5, Coefficient: 27 */
CONVERT_COEFF(-14057177), /* Filter:5, Coefficient: 28 */
CONVERT_COEFF(17029298), /* Filter:4, Coefficient: 27 */
CONVERT_COEFF(5073548), /* Filter:4, Coefficient: 28 */
CONVERT_COEFF(-11295504), /* Filter:3, Coefficient: 27 */
CONVERT_COEFF(33812123), /* Filter:3, Coefficient: 28 */
CONVERT_COEFF(-29933479), /* Filter:2, Coefficient: 27 */
CONVERT_COEFF(407469), /* Filter:2, Coefficient: 28 */
CONVERT_COEFF(3905181), /* Filter:1, Coefficient: 27 */
CONVERT_COEFF(-19010870), /* Filter:1, Coefficient: 28 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(2646463), /* Filter:6, Coefficient: 29 */
CONVERT_COEFF(-5183436), /* Filter:6, Coefficient: 30 */
CONVERT_COEFF(4012768), /* Filter:5, Coefficient: 29 */
CONVERT_COEFF(15785061), /* Filter:5, Coefficient: 30 */
CONVERT_COEFF(-26242846), /* Filter:4, Coefficient: 29 */
CONVERT_COEFF(12222374), /* Filter:4, Coefficient: 30 */
CONVERT_COEFF(-9629387), /* Filter:3, Coefficient: 29 */
CONVERT_COEFF(-38047097), /* Filter:3, Coefficient: 30 */
CONVERT_COEFF(39448271), /* Filter:2, Coefficient: 29 */
CONVERT_COEFF(-29337586), /* Filter:2, Coefficient: 30 */
CONVERT_COEFF(8709491), /* Filter:1, Coefficient: 29 */
CONVERT_COEFF(18945155), /* Filter:1, Coefficient: 30 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(113142), /* Filter:6, Coefficient: 31 */
CONVERT_COEFF(6922098), /* Filter:6, Coefficient: 32 */
CONVERT_COEFF(-16561721), /* Filter:5, Coefficient: 31 */
CONVERT_COEFF(-9670889), /* Filter:5, Coefficient: 32 */
CONVERT_COEFF(26293489), /* Filter:4, Coefficient: 31 */
CONVERT_COEFF(-35302032), /* Filter:4, Coefficient: 32 */
CONVERT_COEFF(40551122), /* Filter:3, Coefficient: 31 */
CONVERT_COEFF(22142931), /* Filter:3, Coefficient: 32 */
CONVERT_COEFF(-31488442), /* Filter:2, Coefficient: 31 */
CONVERT_COEFF(62886474), /* Filter:2, Coefficient: 32 */
CONVERT_COEFF(-25616062), /* Filter:1, Coefficient: 31 */
CONVERT_COEFF(-6708613), /* Filter:1, Coefficient: 32 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-4854058), /* Filter:6, Coefficient: 33 */
CONVERT_COEFF(-6493349), /* Filter:6, Coefficient: 34 */
CONVERT_COEFF(29320297), /* Filter:5, Coefficient: 33 */
CONVERT_COEFF(-6238872), /* Filter:5, Coefficient: 34 */
CONVERT_COEFF(-10922848), /* Filter:4, Coefficient: 33 */
CONVERT_COEFF(57004450), /* Filter:4, Coefficient: 34 */
CONVERT_COEFF(-71937950), /* Filter:3, Coefficient: 33 */
CONVERT_COEFF(20998625), /* Filter:3, Coefficient: 34 */
CONVERT_COEFF(-2929444), /* Filter:2, Coefficient: 33 */
CONVERT_COEFF(-86584128), /* Filter:2, Coefficient: 34 */
CONVERT_COEFF(40270809), /* Filter:1, Coefficient: 33 */
CONVERT_COEFF(-21053631), /* Filter:1, Coefficient: 34 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(10955382), /* Filter:6, Coefficient: 35 */
CONVERT_COEFF(3639331), /* Filter:6, Coefficient: 36 */
CONVERT_COEFF(-37929537), /* Filter:5, Coefficient: 35 */
CONVERT_COEFF(31607052), /* Filter:5, Coefficient: 36 */
CONVERT_COEFF(-23682378), /* Filter:4, Coefficient: 35 */
CONVERT_COEFF(-69775212), /* Filter:4, Coefficient: 36 */
CONVERT_COEFF(89787906), /* Filter:3, Coefficient: 35 */
CONVERT_COEFF(-97112393), /* Filter:3, Coefficient: 36 */
CONVERT_COEFF(69191980), /* Filter:2, Coefficient: 35 */
CONVERT_COEFF(80706313), /* Filter:2, Coefficient: 36 */
CONVERT_COEFF(-42367788), /* Filter:1, Coefficient: 35 */
CONVERT_COEFF(65956939), /* Filter:1, Coefficient: 36 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-17266601), /* Filter:6, Coefficient: 37 */
CONVERT_COEFF(231253), /* Filter:6, Coefficient: 38 */
CONVERT_COEFF(41975839), /* Filter:5, Coefficient: 37 */
CONVERT_COEFF(-59844606), /* Filter:5, Coefficient: 38 */
CONVERT_COEFF(81790468), /* Filter:4, Coefficient: 37 */
CONVERT_COEFF(86455952), /* Filter:4, Coefficient: 38 */
CONVERT_COEFF(-81580682), /* Filter:3, Coefficient: 37 */
CONVERT_COEFF(240735052), /* Filter:3, Coefficient: 38 */
CONVERT_COEFF(-178457029), /* Filter:2, Coefficient: 37 */
CONVERT_COEFF(-14491810), /* Filter:2, Coefficient: 38 */
CONVERT_COEFF(15022342), /* Filter:1, Coefficient: 37 */
CONVERT_COEFF(-138518549), /* Filter:1, Coefficient: 38 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(21704927), /* Filter:6, Coefficient: 39 */
CONVERT_COEFF(-2600806), /* Filter:6, Coefficient: 40 */
CONVERT_COEFF(-44992655), /* Filter:5, Coefficient: 39 */
CONVERT_COEFF(73711853), /* Filter:5, Coefficient: 40 */
CONVERT_COEFF(-138195163), /* Filter:4, Coefficient: 39 */
CONVERT_COEFF(-113308335), /* Filter:4, Coefficient: 40 */
CONVERT_COEFF(145279809), /* Filter:3, Coefficient: 39 */
CONVERT_COEFF(-321141069), /* Filter:3, Coefficient: 40 */
CONVERT_COEFF(488414096), /* Filter:2, Coefficient: 39 */
CONVERT_COEFF(292747412), /* Filter:2, Coefficient: 40 */
CONVERT_COEFF(114569679), /* Filter:1, Coefficient: 39 */
CONVERT_COEFF(586792923), /* Filter:1, Coefficient: 40 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-20066125), /* Filter:6, Coefficient: 41 */
CONVERT_COEFF(5767713), /* Filter:6, Coefficient: 42 */
CONVERT_COEFF(38637120), /* Filter:5, Coefficient: 41 */
CONVERT_COEFF(-70881813), /* Filter:5, Coefficient: 42 */
CONVERT_COEFF(145358829), /* Filter:4, Coefficient: 41 */
CONVERT_COEFF(111916983), /* Filter:4, Coefficient: 42 */
CONVERT_COEFF(-248537116), /* Filter:3, Coefficient: 41 */
CONVERT_COEFF(217233738), /* Filter:3, Coefficient: 42 */
CONVERT_COEFF(-407818384), /* Filter:2, Coefficient: 41 */
CONVERT_COEFF(-414365665), /* Filter:2, Coefficient: 42 */
CONVERT_COEFF(516212737), /* Filter:1, Coefficient: 41 */
CONVERT_COEFF(23787556), /* Filter:1, Coefficient: 42 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(15089402), /* Filter:6, Coefficient: 43 */
CONVERT_COEFF(-7757981), /* Filter:6, Coefficient: 44 */
CONVERT_COEFF(-22766795), /* Filter:5, Coefficient: 43 */
CONVERT_COEFF(54397391), /* Filter:5, Coefficient: 44 */
CONVERT_COEFF(-108596377), /* Filter:4, Coefficient: 43 */
CONVERT_COEFF(-60427595), /* Filter:4, Coefficient: 44 */
CONVERT_COEFF(188328936), /* Filter:3, Coefficient: 43 */
CONVERT_COEFF(-122913915), /* Filter:3, Coefficient: 44 */
CONVERT_COEFF(101272401), /* Filter:2, Coefficient: 43 */
CONVERT_COEFF(136295795), /* Filter:2, Coefficient: 44 */
CONVERT_COEFF(-126544125), /* Filter:1, Coefficient: 43 */
CONVERT_COEFF(46784418), /* Filter:1, Coefficient: 44 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-8119365), /* Filter:6, Coefficient: 45 */
CONVERT_COEFF(8416921), /* Filter:6, Coefficient: 46 */
CONVERT_COEFF(239202), /* Filter:5, Coefficient: 45 */
CONVERT_COEFF(-36066145), /* Filter:5, Coefficient: 46 */
CONVERT_COEFF(78881593), /* Filter:4, Coefficient: 45 */
CONVERT_COEFF(7227261), /* Filter:4, Coefficient: 46 */
CONVERT_COEFF(-57723218), /* Filter:3, Coefficient: 45 */
CONVERT_COEFF(99822925), /* Filter:3, Coefficient: 46 */
CONVERT_COEFF(-112035461), /* Filter:2, Coefficient: 45 */
CONVERT_COEFF(-30308183), /* Filter:2, Coefficient: 46 */
CONVERT_COEFF(46379080), /* Filter:1, Coefficient: 45 */
CONVERT_COEFF(-52379260), /* Filter:1, Coefficient: 46 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(2011547), /* Filter:6, Coefficient: 47 */
CONVERT_COEFF(-7011741), /* Filter:6, Coefficient: 48 */
CONVERT_COEFF(15579621), /* Filter:5, Coefficient: 47 */
CONVERT_COEFF(18206458), /* Filter:5, Coefficient: 48 */
CONVERT_COEFF(-55803497), /* Filter:4, Coefficient: 47 */
CONVERT_COEFF(21803835), /* Filter:4, Coefficient: 48 */
CONVERT_COEFF(-9284004), /* Filter:3, Coefficient: 47 */
CONVERT_COEFF(-64228804), /* Filter:3, Coefficient: 48 */
CONVERT_COEFF(88786923), /* Filter:2, Coefficient: 47 */
CONVERT_COEFF(-24912829), /* Filter:2, Coefficient: 48 */
CONVERT_COEFF(-3286550), /* Filter:1, Coefficient: 47 */
CONVERT_COEFF(38004833), /* Filter:1, Coefficient: 48 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(2029399), /* Filter:6, Coefficient: 49 */
CONVERT_COEFF(4158325), /* Filter:6, Coefficient: 50 */
CONVERT_COEFF(-20392589), /* Filter:5, Coefficient: 49 */
CONVERT_COEFF(-2906434), /* Filter:5, Coefficient: 50 */
CONVERT_COEFF(28980439), /* Filter:4, Coefficient: 49 */
CONVERT_COEFF(-30983025), /* Filter:4, Coefficient: 50 */
CONVERT_COEFF(39777989), /* Filter:3, Coefficient: 49 */
CONVERT_COEFF(25764575), /* Filter:3, Coefficient: 50 */
CONVERT_COEFF(-50106572), /* Filter:2, Coefficient: 49 */
CONVERT_COEFF(44996435), /* Filter:2, Coefficient: 50 */
CONVERT_COEFF(-18138626), /* Filter:1, Coefficient: 49 */
CONVERT_COEFF(-17850332), /* Filter:1, Coefficient: 50 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-3721541), /* Filter:6, Coefficient: 51 */
CONVERT_COEFF(-1133029), /* Filter:6, Coefficient: 52 */
CONVERT_COEFF(16751891), /* Filter:5, Coefficient: 51 */
CONVERT_COEFF(-6818425), /* Filter:5, Coefficient: 52 */
CONVERT_COEFF(-5048118), /* Filter:4, Coefficient: 51 */
CONVERT_COEFF(25896772), /* Filter:4, Coefficient: 52 */
CONVERT_COEFF(-43217126), /* Filter:3, Coefficient: 51 */
CONVERT_COEFF(4190043), /* Filter:3, Coefficient: 52 */
CONVERT_COEFF(12663590), /* Filter:2, Coefficient: 51 */
CONVERT_COEFF(-40495107), /* Filter:2, Coefficient: 52 */
CONVERT_COEFF(23180027), /* Filter:1, Coefficient: 51 */
CONVERT_COEFF(608735), /* Filter:1, Coefficient: 52 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(3433587), /* Filter:6, Coefficient: 53 */
CONVERT_COEFF(-1029758), /* Filter:6, Coefficient: 54 */
CONVERT_COEFF(-8939750), /* Filter:5, Coefficient: 53 */
CONVERT_COEFF(10066060), /* Filter:5, Coefficient: 54 */
CONVERT_COEFF(-10289050), /* Filter:4, Coefficient: 53 */
CONVERT_COEFF(-13908366), /* Filter:4, Coefficient: 54 */
CONVERT_COEFF(30176511), /* Filter:3, Coefficient: 53 */
CONVERT_COEFF(-19720665), /* Filter:3, Coefficient: 54 */
CONVERT_COEFF(12685188), /* Filter:2, Coefficient: 53 */
CONVERT_COEFF(23538156), /* Filter:2, Coefficient: 54 */
CONVERT_COEFF(-17751380), /* Filter:1, Coefficient: 53 */
CONVERT_COEFF(9315300), /* Filter:1, Coefficient: 54 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-2029714), /* Filter:6, Coefficient: 55 */
CONVERT_COEFF(1907010), /* Filter:6, Coefficient: 56 */
CONVERT_COEFF(1203126), /* Filter:5, Coefficient: 55 */
CONVERT_COEFF(-8271006), /* Filter:5, Coefficient: 56 */
CONVERT_COEFF(15437261), /* Filter:4, Coefficient: 55 */
CONVERT_COEFF(1959317), /* Filter:4, Coefficient: 56 */
CONVERT_COEFF(-11906392), /* Filter:3, Coefficient: 55 */
CONVERT_COEFF(21431694), /* Filter:3, Coefficient: 56 */
CONVERT_COEFF(-22526671), /* Filter:2, Coefficient: 55 */
CONVERT_COEFF(-5324400), /* Filter:2, Coefficient: 56 */
CONVERT_COEFF(8260899), /* Filter:1, Coefficient: 55 */
CONVERT_COEFF(-11561731), /* Filter:1, Coefficient: 56 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(463533), /* Filter:6, Coefficient: 57 */
CONVERT_COEFF(-1687787), /* Filter:6, Coefficient: 58 */
CONVERT_COEFF(3698444), /* Filter:5, Coefficient: 57 */
CONVERT_COEFF(4136678), /* Filter:5, Coefficient: 58 */
CONVERT_COEFF(-12705057), /* Filter:4, Coefficient: 57 */
CONVERT_COEFF(5580298), /* Filter:4, Coefficient: 58 */
CONVERT_COEFF(-2881357), /* Filter:3, Coefficient: 57 */
CONVERT_COEFF(-14454865), /* Filter:3, Coefficient: 58 */
CONVERT_COEFF(19856482), /* Filter:2, Coefficient: 57 */
CONVERT_COEFF(-6933075), /* Filter:2, Coefficient: 58 */
CONVERT_COEFF(140888), /* Filter:1, Coefficient: 57 */
CONVERT_COEFF(8573112), /* Filter:1, Coefficient: 58 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(600074), /* Filter:6, Coefficient: 59 */
CONVERT_COEFF(911501), /* Filter:6, Coefficient: 60 */
CONVERT_COEFF(-5065760), /* Filter:5, Coefficient: 59 */
CONVERT_COEFF(-233174), /* Filter:5, Coefficient: 60 */
CONVERT_COEFF(6376293), /* Filter:4, Coefficient: 59 */
CONVERT_COEFF(-7669533), /* Filter:4, Coefficient: 60 */
CONVERT_COEFF(10115126), /* Filter:3, Coefficient: 59 */
CONVERT_COEFF(5126005), /* Filter:3, Coefficient: 60 */
CONVERT_COEFF(-10972601), /* Filter:2, Coefficient: 59 */
CONVERT_COEFF(11128930), /* Filter:2, Coefficient: 60 */
CONVERT_COEFF(-4785738), /* Filter:1, Coefficient: 59 */
CONVERT_COEFF(-3732684), /* Filter:1, Coefficient: 60 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-950577), /* Filter:6, Coefficient: 61 */
CONVERT_COEFF(-130034), /* Filter:6, Coefficient: 62 */
CONVERT_COEFF(3849839), /* Filter:5, Coefficient: 61 */
CONVERT_COEFF(-1976633), /* Filter:5, Coefficient: 62 */
CONVERT_COEFF(-439082), /* Filter:4, Coefficient: 61 */
CONVERT_COEFF(5830744), /* Filter:4, Coefficient: 62 */
CONVERT_COEFF(-10239291), /* Filter:3, Coefficient: 61 */
CONVERT_COEFF(1875733), /* Filter:3, Coefficient: 62 */
CONVERT_COEFF(1978448), /* Filter:2, Coefficient: 61 */
CONVERT_COEFF(-9164115), /* Filter:2, Coefficient: 62 */
CONVERT_COEFF(5531160), /* Filter:1, Coefficient: 61 */
CONVERT_COEFF(-269508), /* Filter:1, Coefficient: 62 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(756094), /* Filter:6, Coefficient: 63 */
CONVERT_COEFF(-329642), /* Filter:6, Coefficient: 64 */
CONVERT_COEFF(-1672857), /* Filter:5, Coefficient: 63 */
CONVERT_COEFF(2337162), /* Filter:5, Coefficient: 64 */
CONVERT_COEFF(-2898810), /* Filter:4, Coefficient: 63 */
CONVERT_COEFF(-2565926), /* Filter:4, Coefficient: 64 */
CONVERT_COEFF(6351912), /* Filter:3, Coefficient: 63 */
CONVERT_COEFF(-4786768), /* Filter:3, Coefficient: 64 */
CONVERT_COEFF(3532591), /* Filter:2, Coefficient: 63 */
CONVERT_COEFF(4618733), /* Filter:2, Coefficient: 64 */
CONVERT_COEFF(-3833892), /* Filter:1, Coefficient: 63 */
CONVERT_COEFF(2235084), /* Filter:1, Coefficient: 64 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-346938), /* Filter:6, Coefficient: 65 */
CONVERT_COEFF(420998), /* Filter:6, Coefficient: 66 */
CONVERT_COEFF(-118556), /* Filter:5, Coefficient: 65 */
CONVERT_COEFF(-1556054), /* Filter:5, Coefficient: 66 */
CONVERT_COEFF(3449180), /* Filter:4, Coefficient: 65 */
CONVERT_COEFF(-99308), /* Filter:4, Coefficient: 66 */
CONVERT_COEFF(-1879552), /* Filter:3, Coefficient: 65 */
CONVERT_COEFF(4332349), /* Filter:3, Coefficient: 66 */
CONVERT_COEFF(-4953240), /* Filter:2, Coefficient: 65 */
CONVERT_COEFF(-565926), /* Filter:2, Coefficient: 66 */
CONVERT_COEFF(1508675), /* Filter:1, Coefficient: 65 */
CONVERT_COEFF(-2340479), /* Filter:1, Coefficient: 66 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(2948), /* Filter:6, Coefficient: 67 */
CONVERT_COEFF(-283734), /* Filter:6, Coefficient: 68 */
CONVERT_COEFF(938534), /* Filter:5, Coefficient: 67 */
CONVERT_COEFF(530259), /* Filter:5, Coefficient: 68 */
CONVERT_COEFF(-2308321), /* Filter:4, Coefficient: 67 */
CONVERT_COEFF(1316267), /* Filter:4, Coefficient: 68 */
CONVERT_COEFF(-1033406), /* Filter:3, Coefficient: 67 */
CONVERT_COEFF(-2361476), /* Filter:3, Coefficient: 68 */
CONVERT_COEFF(3678031), /* Filter:2, Coefficient: 67 */
CONVERT_COEFF(-1548139), /* Filter:2, Coefficient: 68 */
CONVERT_COEFF(191583), /* Filter:1, Coefficient: 67 */
CONVERT_COEFF(1469400), /* Filter:1, Coefficient: 68 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(154632), /* Filter:6, Coefficient: 69 */
CONVERT_COEFF(97677), /* Filter:6, Coefficient: 70 */
CONVERT_COEFF(-909572), /* Filter:5, Coefficient: 69 */
CONVERT_COEFF(153482), /* Filter:5, Coefficient: 70 */
CONVERT_COEFF(817805), /* Filter:4, Coefficient: 69 */
CONVERT_COEFF(-1291146), /* Filter:4, Coefficient: 70 */
CONVERT_COEFF(1928437), /* Filter:3, Coefficient: 69 */
CONVERT_COEFF(514414), /* Filter:3, Coefficient: 70 */
CONVERT_COEFF(-1615722), /* Filter:2, Coefficient: 69 */
CONVERT_COEFF(1829243), /* Filter:2, Coefficient: 70 */
CONVERT_COEFF(-877441), /* Filter:1, Coefficient: 69 */
CONVERT_COEFF(-501872), /* Filter:1, Coefficient: 70 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-151006), /* Filter:6, Coefficient: 71 */
CONVERT_COEFF(22864), /* Filter:6, Coefficient: 72 */
CONVERT_COEFF(491289), /* Filter:5, Coefficient: 71 */
CONVERT_COEFF(-360835), /* Filter:5, Coefficient: 72 */
CONVERT_COEFF(169841), /* Filter:4, Coefficient: 71 */
CONVERT_COEFF(707188), /* Filter:4, Coefficient: 72 */
CONVERT_COEFF(-1484055), /* Filter:3, Coefficient: 71 */
CONVERT_COEFF(448482), /* Filter:3, Coefficient: 72 */
CONVERT_COEFF(84371), /* Filter:2, Coefficient: 71 */
CONVERT_COEFF(-1162547), /* Filter:2, Coefficient: 72 */
CONVERT_COEFF(801814), /* Filter:1, Coefficient: 71 */
CONVERT_COEFF(-87747), /* Filter:1, Coefficient: 72 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(77476), /* Filter:6, Coefficient: 73 */
CONVERT_COEFF(-55647), /* Filter:6, Coefficient: 74 */
CONVERT_COEFF(-98904), /* Filter:5, Coefficient: 73 */
CONVERT_COEFF(262255), /* Filter:5, Coefficient: 74 */
CONVERT_COEFF(-474181), /* Filter:4, Coefficient: 73 */
CONVERT_COEFF(-161908), /* Filter:4, Coefficient: 74 */
CONVERT_COEFF(655915), /* Filter:3, Coefficient: 73 */
CONVERT_COEFF(-589182), /* Filter:3, Coefficient: 74 */
CONVERT_COEFF(527672), /* Filter:2, Coefficient: 73 */
CONVERT_COEFF(407424), /* Filter:2, Coefficient: 74 */
CONVERT_COEFF(-432603), /* Filter:1, Coefficient: 73 */
CONVERT_COEFF(255380), /* Filter:1, Coefficient: 74 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-12425), /* Filter:6, Coefficient: 75 */
CONVERT_COEFF(36432), /* Filter:6, Coefficient: 76 */
CONVERT_COEFF(-86370), /* Filter:5, Coefficient: 75 */
CONVERT_COEFF(-93036), /* Filter:5, Coefficient: 76 */
CONVERT_COEFF(348709), /* Filter:4, Coefficient: 75 */
CONVERT_COEFF(-94475), /* Filter:4, Coefficient: 76 */
CONVERT_COEFF(-68486), /* Filter:3, Coefficient: 75 */
CONVERT_COEFF(343484), /* Filter:3, Coefficient: 76 */
CONVERT_COEFF(-485562), /* Filter:2, Coefficient: 75 */
CONVERT_COEFF(16521), /* Filter:2, Coefficient: 76 */
CONVERT_COEFF(118325), /* Filter:1, Coefficient: 75 */
CONVERT_COEFF(-185814), /* Filter:1, Coefficient: 76 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(-14137), /* Filter:6, Coefficient: 77 */
CONVERT_COEFF(-9206), /* Filter:6, Coefficient: 78 */
CONVERT_COEFF(94324), /* Filter:5, Coefficient: 77 */
CONVERT_COEFF(-8059), /* Filter:5, Coefficient: 78 */
CONVERT_COEFF(-126700), /* Filter:4, Coefficient: 77 */
CONVERT_COEFF(110024), /* Filter:4, Coefficient: 78 */
CONVERT_COEFF(-132999), /* Filter:3, Coefficient: 77 */
CONVERT_COEFF(-93296), /* Filter:3, Coefficient: 78 */
CONVERT_COEFF(229610), /* Filter:2, Coefficient: 77 */
CONVERT_COEFF(-109871), /* Filter:2, Coefficient: 78 */
CONVERT_COEFF(23112), /* Filter:1, Coefficient: 77 */
CONVERT_COEFF(73212), /* Filter:1, Coefficient: 78 */

/* Filter #1, conversion from 48000 Hz to 32000 Hz */

CONVERT_COEFF(11763), /* Filter:6, Coefficient: 79 */
CONVERT_COEFF(-1554661), /* Filter:6, Coefficient: 80 */
CONVERT_COEFF(-36850), /* Filter:5, Coefficient: 79 */
CONVERT_COEFF(3952809), /* Filter:5, Coefficient: 80 */
CONVERT_COEFF(-3318), /* Filter:4, Coefficient: 79 */
CONVERT_COEFF(-3431485), /* Filter:4, Coefficient: 80 */
CONVERT_COEFF(98915), /* Filter:3, Coefficient: 79 */
CONVERT_COEFF(1107272), /* Filter:3, Coefficient: 80 */
CONVERT_COEFF(-44438), /* Filter:2, Coefficient: 79 */
CONVERT_COEFF(-62812), /* Filter:2, Coefficient: 80 */
CONVERT_COEFF(-37195), /* Filter:1, Coefficient: 79 */
CONVERT_COEFF(-11124)

};
