/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2019 Intel Corporation. All rights reserved.
 */

/* Conversion from 48000 Hz to 44100 Hz */
/* NUM_FILTERS=7, FILTER_LENGTH=64, alpha=8.150000, gamma=0.456000 */

__cold_rodata static const int32_t coeff48000to44100[] = {
/* Filter #7, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-34608), /* Filter:7, Coefficient: 1 */
CONVERT_COEFF(58942), /* Filter:7, Coefficient: 2 */
CONVERT_COEFF(69866), /* Filter:6, Coefficient: 1 */
CONVERT_COEFF(-157606), /* Filter:6, Coefficient: 2 */
CONVERT_COEFF(61623), /* Filter:5, Coefficient: 1 */
CONVERT_COEFF(-49681), /* Filter:5, Coefficient: 2 */
CONVERT_COEFF(-66312), /* Filter:4, Coefficient: 1 */
CONVERT_COEFF(181693), /* Filter:4, Coefficient: 2 */
CONVERT_COEFF(-133695), /* Filter:3, Coefficient: 1 */
CONVERT_COEFF(219785), /* Filter:3, Coefficient: 2 */
CONVERT_COEFF(-1792), /* Filter:2, Coefficient: 1 */
CONVERT_COEFF(-79997), /* Filter:2, Coefficient: 2 */
CONVERT_COEFF(33030), /* Filter:1, Coefficient: 1 */
CONVERT_COEFF(-71885), /* Filter:1, Coefficient: 2 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-85115), /* Filter:7, Coefficient: 3 */
CONVERT_COEFF(96633), /* Filter:7, Coefficient: 4 */
CONVERT_COEFF(299637), /* Filter:6, Coefficient: 3 */
CONVERT_COEFF(-469979), /* Filter:6, Coefficient: 4 */
CONVERT_COEFF(-50870), /* Filter:5, Coefficient: 3 */
CONVERT_COEFF(302189), /* Filter:5, Coefficient: 4 */
CONVERT_COEFF(-384955), /* Filter:4, Coefficient: 3 */
CONVERT_COEFF(630700), /* Filter:4, Coefficient: 4 */
CONVERT_COEFF(-230581), /* Filter:3, Coefficient: 3 */
CONVERT_COEFF(36535), /* Filter:3, Coefficient: 4 */
CONVERT_COEFF(271909), /* Filter:2, Coefficient: 3 */
CONVERT_COEFF(-561039), /* Filter:2, Coefficient: 4 */
CONVERT_COEFF(101248), /* Filter:1, Coefficient: 3 */
CONVERT_COEFF(-78725), /* Filter:1, Coefficient: 4 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-67867), /* Filter:7, Coefficient: 5 */
CONVERT_COEFF(-28521), /* Filter:7, Coefficient: 6 */
CONVERT_COEFF(592244), /* Filter:6, Coefficient: 5 */
CONVERT_COEFF(-540223), /* Filter:6, Coefficient: 6 */
CONVERT_COEFF(-730836), /* Filter:5, Coefficient: 5 */
CONVERT_COEFF(1274818), /* Filter:5, Coefficient: 6 */
CONVERT_COEFF(-786525), /* Filter:4, Coefficient: 5 */
CONVERT_COEFF(641498), /* Filter:4, Coefficient: 6 */
CONVERT_COEFF(483560), /* Filter:3, Coefficient: 5 */
CONVERT_COEFF(-1352248), /* Filter:3, Coefficient: 6 */
CONVERT_COEFF(844568), /* Filter:2, Coefficient: 5 */
CONVERT_COEFF(-919927), /* Filter:2, Coefficient: 6 */
CONVERT_COEFF(-43685), /* Filter:1, Coefficient: 5 */
CONVERT_COEFF(291450), /* Filter:1, Coefficient: 6 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(207221), /* Filter:7, Coefficient: 7 */
CONVERT_COEFF(-452333), /* Filter:7, Coefficient: 8 */
CONVERT_COEFF(168968), /* Filter:6, Coefficient: 7 */
CONVERT_COEFF(620325), /* Filter:6, Coefficient: 8 */
CONVERT_COEFF(-1742595), /* Filter:5, Coefficient: 7 */
CONVERT_COEFF(1816487), /* Filter:5, Coefficient: 8 */
CONVERT_COEFF(29205), /* Filter:4, Coefficient: 7 */
CONVERT_COEFF(-1344095), /* Filter:4, Coefficient: 8 */
CONVERT_COEFF(2396129), /* Filter:3, Coefficient: 7 */
CONVERT_COEFF(-3194402), /* Filter:3, Coefficient: 8 */
CONVERT_COEFF(530444), /* Filter:2, Coefficient: 7 */
CONVERT_COEFF(525026), /* Filter:2, Coefficient: 8 */
CONVERT_COEFF(-633133), /* Filter:1, Coefficient: 7 */
CONVERT_COEFF(956210), /* Filter:1, Coefficient: 8 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(703127), /* Filter:7, Coefficient: 9 */
CONVERT_COEFF(-853001), /* Filter:7, Coefficient: 10 */
CONVERT_COEFF(-1794792), /* Filter:6, Coefficient: 9 */
CONVERT_COEFF(3115259), /* Filter:6, Coefficient: 10 */
CONVERT_COEFF(-1127711), /* Filter:5, Coefficient: 9 */
CONVERT_COEFF(-592357), /* Filter:5, Coefficient: 10 */
CONVERT_COEFF(3171756), /* Filter:4, Coefficient: 9 */
CONVERT_COEFF(-5026421), /* Filter:4, Coefficient: 10 */
CONVERT_COEFF(3130331), /* Filter:3, Coefficient: 9 */
CONVERT_COEFF(-1578560), /* Filter:3, Coefficient: 10 */
CONVERT_COEFF(-2241194), /* Filter:2, Coefficient: 9 */
CONVERT_COEFF(4271071), /* Filter:2, Coefficient: 10 */
CONVERT_COEFF(-1072749), /* Filter:1, Coefficient: 9 */
CONVERT_COEFF(768744), /* Filter:1, Coefficient: 10 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(769748), /* Filter:7, Coefficient: 11 */
CONVERT_COEFF(-339193), /* Filter:7, Coefficient: 12 */
CONVERT_COEFF(-4115635), /* Filter:6, Coefficient: 11 */
CONVERT_COEFF(4175773), /* Filter:6, Coefficient: 12 */
CONVERT_COEFF(3316547), /* Filter:5, Coefficient: 11 */
CONVERT_COEFF(-6552082), /* Filter:5, Coefficient: 12 */
CONVERT_COEFF(6078263), /* Filter:4, Coefficient: 11 */
CONVERT_COEFF(-5335178), /* Filter:4, Coefficient: 12 */
CONVERT_COEFF(-1791676), /* Filter:3, Coefficient: 11 */
CONVERT_COEFF(6684486), /* Filter:3, Coefficient: 12 */
CONVERT_COEFF(-5884188), /* Filter:2, Coefficient: 11 */
CONVERT_COEFF(6087444), /* Filter:2, Coefficient: 12 */
CONVERT_COEFF(104733), /* Filter:1, Coefficient: 11 */
CONVERT_COEFF(-1522163), /* Filter:1, Coefficient: 12 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-475432), /* Filter:7, Coefficient: 13 */
CONVERT_COEFF(1576583), /* Filter:7, Coefficient: 14 */
CONVERT_COEFF(-2703131), /* Filter:6, Coefficient: 13 */
CONVERT_COEFF(-602943), /* Filter:6, Coefficient: 14 */
CONVERT_COEFF(9289206), /* Filter:5, Coefficient: 13 */
CONVERT_COEFF(-10150369), /* Filter:5, Coefficient: 14 */
CONVERT_COEFF(1998375), /* Filter:4, Coefficient: 13 */
CONVERT_COEFF(4084791), /* Filter:4, Coefficient: 14 */
CONVERT_COEFF(-11958163), /* Filter:3, Coefficient: 13 */
CONVERT_COEFF(15672295), /* Filter:3, Coefficient: 14 */
CONVERT_COEFF(-3932030), /* Filter:2, Coefficient: 13 */
CONVERT_COEFF(-1045712), /* Filter:2, Coefficient: 14 */
CONVERT_COEFF(3198988), /* Filter:1, Coefficient: 13 */
CONVERT_COEFF(-4582047), /* Filter:1, Coefficient: 14 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-2703138), /* Filter:7, Coefficient: 15 */
CONVERT_COEFF(3453146), /* Filter:7, Coefficient: 16 */
CONVERT_COEFF(5477993), /* Filter:6, Coefficient: 15 */
CONVERT_COEFF(-10918971), /* Filter:6, Coefficient: 16 */
CONVERT_COEFF(7770551), /* Filter:5, Coefficient: 15 */
CONVERT_COEFF(-1353879), /* Filter:5, Coefficient: 16 */
CONVERT_COEFF(-12023857), /* Filter:4, Coefficient: 15 */
CONVERT_COEFF(19736881), /* Filter:4, Coefficient: 16 */
CONVERT_COEFF(-15501757), /* Filter:3, Coefficient: 15 */
CONVERT_COEFF(9501580), /* Filter:3, Coefficient: 16 */
CONVERT_COEFF(8383285), /* Filter:2, Coefficient: 15 */
CONVERT_COEFF(-16438405), /* Filter:2, Coefficient: 16 */
CONVERT_COEFF(4952446), /* Filter:1, Coefficient: 15 */
CONVERT_COEFF(-3644365), /* Filter:1, Coefficient: 16 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-3371842), /* Filter:7, Coefficient: 17 */
CONVERT_COEFF(2097989), /* Filter:7, Coefficient: 18 */
CONVERT_COEFF(15222797), /* Filter:6, Coefficient: 17 */
CONVERT_COEFF(-16309530), /* Filter:6, Coefficient: 18 */
CONVERT_COEFF(-8743201), /* Filter:5, Coefficient: 17 */
CONVERT_COEFF(20620478), /* Filter:5, Coefficient: 18 */
CONVERT_COEFF(-24216328), /* Filter:4, Coefficient: 17 */
CONVERT_COEFF(22270762), /* Filter:4, Coefficient: 18 */
CONVERT_COEFF(2945332), /* Filter:3, Coefficient: 17 */
CONVERT_COEFF(-20300949), /* Filter:3, Coefficient: 18 */
CONVERT_COEFF(22503598), /* Filter:2, Coefficient: 17 */
CONVERT_COEFF(-23386679), /* Filter:2, Coefficient: 18 */
CONVERT_COEFF(335976), /* Filter:1, Coefficient: 17 */
CONVERT_COEFF(4676188), /* Filter:1, Coefficient: 18 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(462769), /* Filter:7, Coefficient: 19 */
CONVERT_COEFF(-3989397), /* Filter:7, Coefficient: 20 */
CONVERT_COEFF(12321848), /* Filter:6, Coefficient: 19 */
CONVERT_COEFF(-2389147), /* Filter:6, Coefficient: 20 */
CONVERT_COEFF(-30859164), /* Filter:5, Coefficient: 19 */
CONVERT_COEFF(35112554), /* Filter:5, Coefficient: 20 */
CONVERT_COEFF(-11641126), /* Filter:4, Coefficient: 19 */
CONVERT_COEFF(-7783920), /* Filter:4, Coefficient: 20 */
CONVERT_COEFF(38527510), /* Filter:3, Coefficient: 19 */
CONVERT_COEFF(-51525628), /* Filter:3, Coefficient: 20 */
CONVERT_COEFF(16406368), /* Filter:2, Coefficient: 19 */
CONVERT_COEFF(-579401), /* Filter:2, Coefficient: 20 */
CONVERT_COEFF(-10331423), /* Filter:1, Coefficient: 19 */
CONVERT_COEFF(14886324), /* Filter:1, Coefficient: 20 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(7681853), /* Filter:7, Coefficient: 21 */
CONVERT_COEFF(-10332684), /* Filter:7, Coefficient: 22 */
CONVERT_COEFF(-12649004), /* Filter:6, Coefficient: 21 */
CONVERT_COEFF(29813575), /* Filter:6, Coefficient: 22 */
CONVERT_COEFF(-29258606), /* Filter:5, Coefficient: 21 */
CONVERT_COEFF(10919084), /* Filter:5, Coefficient: 22 */
CONVERT_COEFF(33051579), /* Filter:4, Coefficient: 21 */
CONVERT_COEFF(-57937612), /* Filter:4, Coefficient: 22 */
CONVERT_COEFF(52464541), /* Filter:3, Coefficient: 21 */
CONVERT_COEFF(-35835631), /* Filter:3, Coefficient: 22 */
CONVERT_COEFF(-22361334), /* Filter:2, Coefficient: 21 */
CONVERT_COEFF(47518499), /* Filter:2, Coefficient: 22 */
CONVERT_COEFF(-16268114), /* Filter:1, Coefficient: 21 */
CONVERT_COEFF(12660525), /* Filter:1, Coefficient: 22 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(10556529), /* Filter:7, Coefficient: 23 */
CONVERT_COEFF(-7162197), /* Filter:7, Coefficient: 24 */
CONVERT_COEFF(-44081375), /* Filter:6, Coefficient: 23 */
CONVERT_COEFF(49138162), /* Filter:6, Coefficient: 24 */
CONVERT_COEFF(19020907), /* Filter:5, Coefficient: 23 */
CONVERT_COEFF(-55343811), /* Filter:5, Coefficient: 24 */
CONVERT_COEFF(73731473), /* Filter:4, Coefficient: 23 */
CONVERT_COEFF(-71049212), /* Filter:4, Coefficient: 24 */
CONVERT_COEFF(-221654), /* Filter:3, Coefficient: 23 */
CONVERT_COEFF(51953451), /* Filter:3, Coefficient: 24 */
CONVERT_COEFF(-67260276), /* Filter:2, Coefficient: 23 */
CONVERT_COEFF(72617965), /* Filter:2, Coefficient: 24 */
CONVERT_COEFF(-3194146), /* Filter:1, Coefficient: 23 */
CONVERT_COEFF(-11448189), /* Filter:1, Coefficient: 24 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-380917), /* Filter:7, Coefficient: 25 */
CONVERT_COEFF(11434339), /* Filter:7, Coefficient: 26 */
CONVERT_COEFF(-38782554), /* Filter:6, Coefficient: 25 */
CONVERT_COEFF(8866692), /* Filter:6, Coefficient: 26 */
CONVERT_COEFF(88284037), /* Filter:5, Coefficient: 25 */
CONVERT_COEFF(-104388358), /* Filter:5, Coefficient: 26 */
CONVERT_COEFF(42410154), /* Filter:4, Coefficient: 25 */
CONVERT_COEFF(14925059), /* Filter:4, Coefficient: 26 */
CONVERT_COEFF(-109003806), /* Filter:3, Coefficient: 25 */
CONVERT_COEFF(154763074), /* Filter:3, Coefficient: 26 */
CONVERT_COEFF(-55449878), /* Filter:2, Coefficient: 25 */
CONVERT_COEFF(10901411), /* Filter:2, Coefficient: 26 */
CONVERT_COEFF(28705285), /* Filter:1, Coefficient: 25 */
CONVERT_COEFF(-44216319), /* Filter:1, Coefficient: 26 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-23628219), /* Filter:7, Coefficient: 27 */
CONVERT_COEFF(32030513), /* Filter:7, Coefficient: 28 */
CONVERT_COEFF(40253511), /* Filter:6, Coefficient: 27 */
CONVERT_COEFF(-100091024), /* Filter:6, Coefficient: 28 */
CONVERT_COEFF(88441513), /* Filter:5, Coefficient: 27 */
CONVERT_COEFF(-26445221), /* Filter:5, Coefficient: 28 */
CONVERT_COEFF(-96386403), /* Filter:4, Coefficient: 27 */
CONVERT_COEFF(187893880), /* Filter:4, Coefficient: 28 */
CONVERT_COEFF(-167878153), /* Filter:3, Coefficient: 27 */
CONVERT_COEFF(123866455), /* Filter:3, Coefficient: 28 */
CONVERT_COEFF(60443544), /* Filter:2, Coefficient: 27 */
CONVERT_COEFF(-151195441), /* Filter:2, Coefficient: 28 */
CONVERT_COEFF(52284288), /* Filter:1, Coefficient: 27 */
CONVERT_COEFF(-46468489), /* Filter:1, Coefficient: 28 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-27579775), /* Filter:7, Coefficient: 29 */
CONVERT_COEFF(944226), /* Filter:7, Coefficient: 30 */
CONVERT_COEFF(146139326), /* Filter:6, Coefficient: 29 */
CONVERT_COEFF(-125193602), /* Filter:6, Coefficient: 30 */
CONVERT_COEFF(-89102768), /* Filter:5, Coefficient: 29 */
CONVERT_COEFF(248694534), /* Filter:5, Coefficient: 30 */
CONVERT_COEFF(-262248494), /* Filter:4, Coefficient: 29 */
CONVERT_COEFF(259694722), /* Filter:4, Coefficient: 30 */
CONVERT_COEFF(6010978), /* Filter:3, Coefficient: 29 */
CONVERT_COEFF(-266538025), /* Filter:3, Coefficient: 30 */
CONVERT_COEFF(246363387), /* Filter:2, Coefficient: 29 */
CONVERT_COEFF(-319948913), /* Filter:2, Coefficient: 30 */
CONVERT_COEFF(19590070), /* Filter:1, Coefficient: 29 */
CONVERT_COEFF(39171518), /* Filter:1, Coefficient: 30 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(39504750), /* Filter:7, Coefficient: 31 */
CONVERT_COEFF(-55450763), /* Filter:7, Coefficient: 32 */
CONVERT_COEFF(-8119725), /* Filter:6, Coefficient: 31 */
CONVERT_COEFF(167052942), /* Filter:6, Coefficient: 32 */
CONVERT_COEFF(-331843959), /* Filter:5, Coefficient: 31 */
CONVERT_COEFF(152869295), /* Filter:5, Coefficient: 32 */
CONVERT_COEFF(54567300), /* Filter:4, Coefficient: 31 */
CONVERT_COEFF(-585088494), /* Filter:4, Coefficient: 32 */
CONVERT_COEFF(769027860), /* Filter:3, Coefficient: 31 */
CONVERT_COEFF(-550799263), /* Filter:3, Coefficient: 32 */
CONVERT_COEFF(300954467), /* Filter:2, Coefficient: 31 */
CONVERT_COEFF(871436613), /* Filter:2, Coefficient: 32 */
CONVERT_COEFF(-163170521), /* Filter:1, Coefficient: 31 */
CONVERT_COEFF(660899842), /* Filter:1, Coefficient: 32 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(38104285), /* Filter:7, Coefficient: 33 */
CONVERT_COEFF(1432784), /* Filter:7, Coefficient: 34 */
CONVERT_COEFF(-224528201), /* Filter:6, Coefficient: 33 */
CONVERT_COEFF(117817853), /* Filter:6, Coefficient: 34 */
CONVERT_COEFF(215108034), /* Filter:5, Coefficient: 33 */
CONVERT_COEFF(-360908796), /* Filter:5, Coefficient: 34 */
CONVERT_COEFF(566423159), /* Filter:4, Coefficient: 33 */
CONVERT_COEFF(-22644330), /* Filter:4, Coefficient: 34 */
CONVERT_COEFF(-547493323), /* Filter:3, Coefficient: 33 */
CONVERT_COEFF(767195769), /* Filter:3, Coefficient: 34 */
CONVERT_COEFF(-871689336), /* Filter:2, Coefficient: 33 */
CONVERT_COEFF(-300550035), /* Filter:2, Coefficient: 34 */
CONVERT_COEFF(660899842), /* Filter:1, Coefficient: 33 */
CONVERT_COEFF(-163170521), /* Filter:1, Coefficient: 34 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-27672195), /* Filter:7, Coefficient: 35 */
CONVERT_COEFF(31921116), /* Filter:7, Coefficient: 36 */
CONVERT_COEFF(19821501), /* Filter:6, Coefficient: 35 */
CONVERT_COEFF(-91903193), /* Filter:6, Coefficient: 36 */
CONVERT_COEFF(227085039), /* Filter:5, Coefficient: 35 */
CONVERT_COEFF(-46451389), /* Filter:5, Coefficient: 36 */
CONVERT_COEFF(-290592428), /* Filter:4, Coefficient: 35 */
CONVERT_COEFF(278076526), /* Filter:4, Coefficient: 36 */
CONVERT_COEFF(-267778001), /* Filter:3, Coefficient: 35 */
CONVERT_COEFF(8458989), /* Filter:3, Coefficient: 36 */
CONVERT_COEFF(319555239), /* Filter:2, Coefficient: 35 */
CONVERT_COEFF(-246162036), /* Filter:2, Coefficient: 36 */
CONVERT_COEFF(39171518), /* Filter:1, Coefficient: 35 */
CONVERT_COEFF(19590070), /* Filter:1, Coefficient: 36 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-23433614), /* Filter:7, Coefficient: 37 */
CONVERT_COEFF(11233560), /* Filter:7, Coefficient: 38 */
CONVERT_COEFF(101002885), /* Filter:6, Coefficient: 37 */
CONVERT_COEFF(-76882874), /* Filter:6, Coefficient: 38 */
CONVERT_COEFF(-64257112), /* Filter:5, Coefficient: 37 */
CONVERT_COEFF(110837148), /* Filter:5, Coefficient: 38 */
CONVERT_COEFF(-187504747), /* Filter:4, Coefficient: 37 */
CONVERT_COEFF(85554391), /* Filter:4, Coefficient: 38 */
CONVERT_COEFF(121745909), /* Filter:3, Coefficient: 37 */
CONVERT_COEFF(-166662052), /* Filter:3, Coefficient: 38 */
CONVERT_COEFF(151201064), /* Filter:2, Coefficient: 37 */
CONVERT_COEFF(-60582140), /* Filter:2, Coefficient: 38 */
CONVERT_COEFF(-46468489), /* Filter:1, Coefficient: 37 */
CONVERT_COEFF(52284288), /* Filter:1, Coefficient: 38 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-223441), /* Filter:7, Coefficient: 39 */
CONVERT_COEFF(-7253420), /* Filter:7, Coefficient: 40 */
CONVERT_COEFF(40567202), /* Filter:6, Coefficient: 39 */
CONVERT_COEFF(-5841992), /* Filter:6, Coefficient: 40 */
CONVERT_COEFF(-110758392), /* Filter:5, Coefficient: 39 */
CONVERT_COEFF(82493642), /* Filter:5, Coefficient: 40 */
CONVERT_COEFF(-397767), /* Filter:4, Coefficient: 39 */
CONVERT_COEFF(-55476805), /* Filter:4, Coefficient: 40 */
CONVERT_COEFF(154450910), /* Filter:3, Coefficient: 39 */
CONVERT_COEFF(-109358640), /* Filter:3, Coefficient: 40 */
CONVERT_COEFF(-10716025), /* Filter:2, Coefficient: 39 */
CONVERT_COEFF(55283389), /* Filter:2, Coefficient: 40 */
CONVERT_COEFF(-44216319), /* Filter:1, Coefficient: 39 */
CONVERT_COEFF(28705285), /* Filter:1, Coefficient: 40 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(10580704), /* Filter:7, Coefficient: 41 */
CONVERT_COEFF(-10304561), /* Filter:7, Coefficient: 42 */
CONVERT_COEFF(-19382377), /* Filter:6, Coefficient: 41 */
CONVERT_COEFF(32139774), /* Filter:6, Coefficient: 42 */
CONVERT_COEFF(-42829268), /* Filter:5, Coefficient: 41 */
CONVERT_COEFF(4984408), /* Filter:5, Coefficient: 42 */
CONVERT_COEFF(79727899), /* Filter:4, Coefficient: 41 */
CONVERT_COEFF(-77180699), /* Filter:4, Coefficient: 42 */
CONVERT_COEFF(52664560), /* Filter:3, Coefficient: 41 */
CONVERT_COEFF(-1000453), /* Filter:3, Coefficient: 42 */
CONVERT_COEFF(-72507573), /* Filter:2, Coefficient: 41 */
CONVERT_COEFF(67216590), /* Filter:2, Coefficient: 42 */
CONVERT_COEFF(-11448189), /* Filter:1, Coefficient: 41 */
CONVERT_COEFF(-3194146), /* Filter:1, Coefficient: 42 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(7623908), /* Filter:7, Coefficient: 43 */
CONVERT_COEFF(-3924457), /* Filter:7, Coefficient: 44 */
CONVERT_COEFF(-33292678), /* Filter:6, Coefficient: 43 */
CONVERT_COEFF(26137219), /* Filter:6, Coefficient: 44 */
CONVERT_COEFF(22596297), /* Filter:5, Coefficient: 43 */
CONVERT_COEFF(-36478796), /* Filter:5, Coefficient: 44 */
CONVERT_COEFF(56875188), /* Filter:4, Coefficient: 43 */
CONVERT_COEFF(-29072432), /* Filter:4, Coefficient: 44 */
CONVERT_COEFF(-35199577), /* Filter:3, Coefficient: 43 */
CONVERT_COEFF(52081162), /* Filter:3, Coefficient: 44 */
CONVERT_COEFF(-47532278), /* Filter:2, Coefficient: 43 */
CONVERT_COEFF(22412200), /* Filter:2, Coefficient: 44 */
CONVERT_COEFF(12660525), /* Filter:1, Coefficient: 43 */
CONVERT_COEFF(-16268114), /* Filter:1, Coefficient: 44 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(408527), /* Filter:7, Coefficient: 45 */
CONVERT_COEFF(2131760), /* Filter:7, Coefficient: 46 */
CONVERT_COEFF(-14928192), /* Filter:6, Coefficient: 45 */
CONVERT_COEFF(3604808), /* Filter:6, Coefficient: 46 */
CONVERT_COEFF(37496027), /* Filter:5, Coefficient: 45 */
CONVERT_COEFF(-29308651), /* Filter:5, Coefficient: 46 */
CONVERT_COEFF(2701198), /* Filter:4, Coefficient: 45 */
CONVERT_COEFF(16300851), /* Filter:4, Coefficient: 46 */
CONVERT_COEFF(-51410197), /* Filter:3, Coefficient: 45 */
CONVERT_COEFF(38626003), /* Filter:3, Coefficient: 46 */
CONVERT_COEFF(514571), /* Filter:2, Coefficient: 45 */
CONVERT_COEFF(-16347016), /* Filter:2, Coefficient: 46 */
CONVERT_COEFF(14886324), /* Filter:1, Coefficient: 45 */
CONVERT_COEFF(-10331423), /* Filter:1, Coefficient: 46 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-3383382), /* Filter:7, Coefficient: 47 */
CONVERT_COEFF(3446766), /* Filter:7, Coefficient: 48 */
CONVERT_COEFF(5060056), /* Filter:6, Coefficient: 47 */
CONVERT_COEFF(-9794928), /* Filter:6, Coefficient: 48 */
CONVERT_COEFF(16712649), /* Filter:5, Coefficient: 47 */
CONVERT_COEFF(-4136967), /* Filter:5, Coefficient: 48 */
CONVERT_COEFF(-25550339), /* Filter:4, Coefficient: 47 */
CONVERT_COEFF(25786949), /* Filter:4, Coefficient: 48 */
CONVERT_COEFF(-20524157), /* Filter:3, Coefficient: 47 */
CONVERT_COEFF(3201416), /* Filter:3, Coefficient: 48 */
CONVERT_COEFF(23344971), /* Filter:2, Coefficient: 47 */
CONVERT_COEFF(-22483689), /* Filter:2, Coefficient: 48 */
CONVERT_COEFF(4676188), /* Filter:1, Coefficient: 47 */
CONVERT_COEFF(335976), /* Filter:1, Coefficient: 48 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-2686276), /* Filter:7, Coefficient: 49 */
CONVERT_COEFF(1556907), /* Filter:7, Coefficient: 50 */
CONVERT_COEFF(10698922), /* Filter:6, Coefficient: 49 */
CONVERT_COEFF(-8800439), /* Filter:6, Coefficient: 50 */
CONVERT_COEFF(-5353271), /* Filter:5, Coefficient: 49 */
CONVERT_COEFF(10426821), /* Filter:5, Coefficient: 50 */
CONVERT_COEFF(-19783847), /* Filter:4, Coefficient: 49 */
CONVERT_COEFF(11038509), /* Filter:4, Coefficient: 50 */
CONVERT_COEFF(9283532), /* Filter:3, Coefficient: 49 */
CONVERT_COEFF(-15360523), /* Filter:3, Coefficient: 50 */
CONVERT_COEFF(16437903), /* Filter:2, Coefficient: 49 */
CONVERT_COEFF(-8395909), /* Filter:2, Coefficient: 50 */
CONVERT_COEFF(-3644365), /* Filter:1, Coefficient: 49 */
CONVERT_COEFF(4952446), /* Filter:1, Coefficient: 50 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-458759), /* Filter:7, Coefficient: 51 */
CONVERT_COEFF(-349838), /* Filter:7, Coefficient: 52 */
CONVERT_COEFF(5504157), /* Filter:6, Coefficient: 51 */
CONVERT_COEFF(-2104680), /* Filter:6, Coefficient: 52 */
CONVERT_COEFF(-11299744), /* Filter:5, Coefficient: 51 */
CONVERT_COEFF(9194215), /* Filter:5, Coefficient: 52 */
CONVERT_COEFF(-2643603), /* Filter:4, Coefficient: 51 */
CONVERT_COEFF(-3406265), /* Filter:4, Coefficient: 52 */
CONVERT_COEFF(15614973), /* Filter:3, Coefficient: 51 */
CONVERT_COEFF(-11968720), /* Filter:3, Coefficient: 52 */
CONVERT_COEFF(1064109), /* Filter:2, Coefficient: 51 */
CONVERT_COEFF(3914091), /* Filter:2, Coefficient: 52 */
CONVERT_COEFF(-4582047), /* Filter:1, Coefficient: 51 */
CONVERT_COEFF(3198988), /* Filter:1, Coefficient: 52 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(773994), /* Filter:7, Coefficient: 53 */
CONVERT_COEFF(-852306), /* Filter:7, Coefficient: 54 */
CONVERT_COEFF(-520111), /* Filter:6, Coefficient: 53 */
CONVERT_COEFF(2004349), /* Filter:6, Coefficient: 54 */
CONVERT_COEFF(-5690292), /* Filter:5, Coefficient: 53 */
CONVERT_COEFF(2181982), /* Filter:5, Coefficient: 54 */
CONVERT_COEFF(6401526), /* Filter:4, Coefficient: 53 */
CONVERT_COEFF(-6690447), /* Filter:4, Coefficient: 54 */
CONVERT_COEFF(6735661), /* Filter:3, Coefficient: 53 */
CONVERT_COEFF(-1855964), /* Filter:3, Coefficient: 54 */
CONVERT_COEFF(-6073880), /* Filter:2, Coefficient: 53 */
CONVERT_COEFF(5876421), /* Filter:2, Coefficient: 54 */
CONVERT_COEFF(-1522163), /* Filter:1, Coefficient: 53 */
CONVERT_COEFF(104733), /* Filter:1, Coefficient: 54 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(699704), /* Filter:7, Coefficient: 55 */
CONVERT_COEFF(-448245), /* Filter:7, Coefficient: 56 */
CONVERT_COEFF(-2415951), /* Filter:6, Coefficient: 55 */
CONVERT_COEFF(2082258), /* Filter:6, Coefficient: 56 */
CONVERT_COEFF(439695), /* Filter:5, Coefficient: 55 */
CONVERT_COEFF(-1855684), /* Filter:5, Coefficient: 56 */
CONVERT_COEFF(5225196), /* Filter:4, Coefficient: 55 */
CONVERT_COEFF(-3082950), /* Filter:4, Coefficient: 56 */
CONVERT_COEFF(-1521598), /* Filter:3, Coefficient: 55 */
CONVERT_COEFF(3091258), /* Filter:3, Coefficient: 56 */
CONVERT_COEFF(-4268572), /* Filter:2, Coefficient: 55 */
CONVERT_COEFF(2242351), /* Filter:2, Coefficient: 56 */
CONVERT_COEFF(768744), /* Filter:1, Coefficient: 55 */
CONVERT_COEFF(-1072749), /* Filter:1, Coefficient: 56 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(203843), /* Filter:7, Coefficient: 57 */
CONVERT_COEFF(-26420), /* Filter:7, Coefficient: 58 */
CONVERT_COEFF(-1402019), /* Filter:6, Coefficient: 57 */
CONVERT_COEFF(704429), /* Filter:6, Coefficient: 58 */
CONVERT_COEFF(2199202), /* Filter:5, Coefficient: 57 */
CONVERT_COEFF(-1845730), /* Filter:5, Coefficient: 58 */
CONVERT_COEFF(1112474), /* Filter:4, Coefficient: 57 */
CONVERT_COEFF(227477), /* Filter:4, Coefficient: 58 */
CONVERT_COEFF(-3174871), /* Filter:3, Coefficient: 57 */
CONVERT_COEFF(2392005), /* Filter:3, Coefficient: 58 */
CONVERT_COEFF(-527993), /* Filter:2, Coefficient: 57 */
CONVERT_COEFF(-527169), /* Filter:2, Coefficient: 58 */
CONVERT_COEFF(956210), /* Filter:1, Coefficient: 57 */
CONVERT_COEFF(-633133), /* Filter:1, Coefficient: 58 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-68761), /* Filter:7, Coefficient: 59 */
CONVERT_COEFF(96718), /* Filter:7, Coefficient: 60 */
CONVERT_COEFF(-181684), /* Filter:6, Coefficient: 59 */
CONVERT_COEFF(-110580), /* Filter:6, Coefficient: 60 */
CONVERT_COEFF(1207778), /* Filter:5, Coefficient: 59 */
CONVERT_COEFF(-596672), /* Filter:5, Coefficient: 60 */
CONVERT_COEFF(-852533), /* Filter:4, Coefficient: 59 */
CONVERT_COEFF(926541), /* Filter:4, Coefficient: 60 */
CONVERT_COEFF(-1357177), /* Filter:3, Coefficient: 59 */
CONVERT_COEFF(491739), /* Filter:3, Coefficient: 60 */
CONVERT_COEFF(917240), /* Filter:2, Coefficient: 59 */
CONVERT_COEFF(-842790), /* Filter:2, Coefficient: 60 */
CONVERT_COEFF(291450), /* Filter:1, Coefficient: 59 */
CONVERT_COEFF(-43685), /* Filter:1, Coefficient: 60 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-84837), /* Filter:7, Coefficient: 61 */
CONVERT_COEFF(58623), /* Filter:7, Coefficient: 62 */
CONVERT_COEFF(210495), /* Filter:6, Coefficient: 61 */
CONVERT_COEFF(-195186), /* Filter:6, Coefficient: 62 */
CONVERT_COEFF(170808), /* Filter:5, Coefficient: 61 */
CONVERT_COEFF(45623), /* Filter:5, Coefficient: 62 */
CONVERT_COEFF(-705509), /* Filter:4, Coefficient: 61 */
CONVERT_COEFF(414544), /* Filter:4, Coefficient: 62 */
CONVERT_COEFF(28928), /* Filter:3, Coefficient: 61 */
CONVERT_COEFF(-225201), /* Filter:3, Coefficient: 62 */
CONVERT_COEFF(560092), /* Filter:2, Coefficient: 61 */
CONVERT_COEFF(-271537), /* Filter:2, Coefficient: 62 */
CONVERT_COEFF(-78725), /* Filter:1, Coefficient: 61 */
CONVERT_COEFF(101248), /* Filter:1, Coefficient: 62 */

/* Filter #1, conversion from 48000 Hz to 44100 Hz */

CONVERT_COEFF(-34394), /* Filter:7, Coefficient: 63 */
CONVERT_COEFF(-3711660), /* Filter:7, Coefficient: 64 */
CONVERT_COEFF(137148), /* Filter:6, Coefficient: 63 */
CONVERT_COEFF(10382357), /* Filter:6, Coefficient: 64 */
CONVERT_COEFF(-107490), /* Filter:5, Coefficient: 63 */
CONVERT_COEFF(-10759470), /* Filter:5, Coefficient: 64 */
CONVERT_COEFF(-186990), /* Filter:4, Coefficient: 63 */
CONVERT_COEFF(5200106), /* Filter:4, Coefficient: 64 */
CONVERT_COEFF(216708), /* Filter:3, Coefficient: 63 */
CONVERT_COEFF(-1228780), /* Filter:3, Coefficient: 64 */
CONVERT_COEFF(79933), /* Filter:2, Coefficient: 63 */
CONVERT_COEFF(84418), /* Filter:2, Coefficient: 64 */
CONVERT_COEFF(-71885), /* Filter:1, Coefficient: 63 */
CONVERT_COEFF(33030)

};
