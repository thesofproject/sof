/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Shriram Shastry <malladi.sastry@linux.intel.com>
 */

/* Reference table generated by Matlab log2() */
static const double log2_lookup_table[] = {
	0.0000000000000000, 24.3706434451000007, 25.3706434117999997, 25.9556059015000002,
	26.3706433951999983, 26.6925714867999986, 26.9556058904000011, 27.1779983101000013,
	27.3706433868999994, 27.5405683874000005, 27.6925714800999998, 27.8300750032999993,
	27.9556058848000006, 28.0710831017999993, 28.1779983053999992, 28.2775339786000011,
	28.3706433826999991, 28.4581062236999998, 28.5405683836999984, 28.6185708954999996,
	28.6925714768000013, 28.7629608045000005, 28.8300750002000008, 28.8942053375000008,
	28.9556058820999986, 29.0144995723000001, 29.0710831005999992, 29.1255308843999998,
	29.1779983041999991, 29.2286243771000009, 29.2775339774999992, 29.3248396922000012,
	29.3706433816999990, 29.4150375010000005, 29.4581062228000015, 29.4999263983999995,
	29.5405683828000001, 29.5800967469000007, 29.6185708946000013, 29.6560455999999988,
	29.6925714760000012, 29.7281953855999994, 29.7629608037000004, 29.7969081355999990,
	29.8300749994999990, 29.8624964770999988, 29.8942053367999989, 29.9252322323999991,
	29.9556058814000004, 29.9853532246999990, 30.0144995702999999, 30.0430687224999993,
	30.0710830985999991, 30.0985638350000002, 30.1255308825999997, 30.1520030938999994,
	30.1779983023999989, 30.2035333944999991, 30.2286243753999990, 30.2532864295999993,
	30.2775339758000008, 30.3013807178000008, 30.3248396906000011, 30.3479233037000000,
	30.3706433800999989, 30.3930111931000013, 30.4150374994000003, 30.4367325704999985,
	30.4581062212999996, 30.4791678367999985, 30.4999263969000012, 30.5203904994999995,
	30.5405683814000000, 30.5604679387999987, 30.5800967455999988, 30.5994620708000014,
	30.6185708938000012, 30.6374299209999990, 30.6560455991000005, 30.6744241283999983,
	30.6925714750999994, 30.7104933831000011, 30.7281953847999993, 30.7456828115000000,
	30.7629608029000003, 30.7800343163000001, 30.7969081347999989, 30.8135868759999987,
	30.8300749986999989, 30.8463768109999990, 30.8624964764000005, 30.8784380202000008,
	30.8942053361000006, 30.9098021910999989, 30.9252322317000008, 30.9404989882999999,
	30.9556058806999985, 30.9705562221000008, 30.9853532240000007, 30.9999999997000018};
/* testvector in Q31.1 */
static const uint32_t vector_table[100] = {
	2U, 43383510U, 86767018U, 130150526U, 173534034U, 216917542U,
	260301050U, 303684558U, 347068066U, 390451574U, 433835082U, 477218590U,
	520602098U, 563985606U, 607369114U, 650752622U, 694136130U, 737519638U,
	780903146U, 824286654U, 867670162U, 911053670U, 954437178U, 997820686U,
	1041204194U, 1084587703U, 1127971211U, 1171354719U, 1214738227U, 1258121735U,
	1301505243U, 1344888751U, 1388272259U, 1431655767U, 1475039275U, 1518422783U,
	1561806291U, 1605189799U, 1648573307U, 1691956815U, 1735340323U, 1778723831U,
	1822107339U, 1865490847U, 1908874355U, 1952257863U, 1995641371U, 2039024879U,
	2082408387U, 2125791895U, 2169175403U, 2212558911U, 2255942419U, 2299325927U,
	2342709435U, 2386092943U, 2429476451U, 2472859959U, 2516243467U, 2559626975U,
	2603010483U, 2646393991U, 2689777499U, 2733161007U, 2776544515U, 2819928023U,
	2863311531U, 2906695039U, 2950078547U, 2993462055U, 3036845563U, 3080229071U,
	3123612579U, 3166996087U, 3210379595U, 3253763104U, 3297146612U, 3340530120U,
	3383913628U, 3427297136U, 3470680644U, 3514064152U, 3557447660U, 3600831168U,
	3644214676U, 3687598184U, 3730981692U, 3774365200U, 3817748708U, 3861132216U,
	3904515724U, 3947899232U, 3991282740U, 4034666248U, 4078049756U, 4121433264U,
	4164816772U, 4208200280U, 4251583788U, 4294967295U};
