// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
//
// Original test from sof/test/cmocka/src/math/arithmetic/base2_logarithm.c
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>

#include <zephyr/ztest.h>
#include <sof/audio/format.h>
#include <sof/math/log.h>
#include <math.h>
#include <rtos/string.h>

/* Test data tables from MATLAB-generated reference */
#include "log2_tables.h"

/* 'Error[max] = 0.0000236785999981,THD(-dBc) = -92.5128795787487235' */
#define CMP_TOLERANCE 0.0000236785691029f

/* testvector in Q32.0 */
static const uint32_t uv[100] = {
	1ULL,	 43383509ULL,   86767017ULL,   130150525ULL, 173534033ULL, 216917541ULL,
	260301049ULL, 303684557ULL,  347068065ULL,  390451573ULL, 433835081ULL, 477218589ULL,
	520602097ULL, 563985605ULL,  607369113ULL,  650752621ULL, 694136129ULL, 737519638ULL,
	780903146ULL, 824286654ULL,  867670162ULL,  911053670ULL, 954437178ULL, 997820686ULL,
	1041204194ULL, 1084587702ULL, 1127971210ULL, 1171354718ULL, 1214738226ULL, 1258121734ULL,
	1301505242ULL, 1344888750ULL, 1388272258ULL, 1431655766ULL, 1475039274ULL, 1518422782ULL,
	1561806290ULL, 1605189798ULL, 1648573306ULL, 1691956814ULL, 1735340322ULL, 1778723830ULL,
	1822107338ULL, 1865490846ULL, 1908874354ULL, 1952257862ULL, 1995641370ULL, 2039024878ULL,
	2082408386ULL, 2125791894ULL, 2169175403ULL, 2212558911ULL, 2255942419ULL, 2299325927ULL,
	2342709435ULL, 2386092943ULL, 2429476451ULL, 2472859959ULL, 2516243467ULL, 2559626975ULL,
	2603010483ULL, 2646393991ULL, 2689777499ULL, 2733161007ULL, 2776544515ULL, 2819928023ULL,
	2863311531ULL, 2906695039ULL, 2950078547ULL, 2993462055ULL, 3036845563ULL, 3080229071ULL,
	3123612579ULL, 3166996087ULL, 3210379595ULL, 3253763103ULL, 3297146611ULL, 3340530119ULL,
	3383913627ULL, 3427297135ULL, 3470680643ULL, 3514064151ULL, 3557447659ULL, 3600831168ULL,
	3644214676ULL, 3687598184ULL, 3730981692ULL, 3774365200ULL, 3817748708ULL, 3861132216ULL,
	3904515724ULL, 3947899232ULL, 3991282740ULL, 4034666248ULL, 4078049756ULL, 4121433264ULL,
	4164816772ULL, 4208200280ULL, 4251583788ULL, 4294967295ULL};

/**
 * @brief Test base-2 logarithm function with fixed-point arithmetic
 *
 * This test validates the base2_logarithm() function against MATLAB-generated
 * reference values. It tests 100 uniformly distributed input values across
 * the full uint32_t range, checking that the fixed-point logarithm calculation
 * stays within acceptable tolerance.
 *
 * Input values: Q32.0 format (unsigned 32-bit integers)
 * Result: Q16.16 fixed-point format
 * Reference: MATLAB log2() function results
 */
ZTEST(math_advanced_functions_suite, test_math_arithmetic_base2log_fixed)
{
	uint32_t u[100];
	int i, ret;

	BUILD_ASSERT(ARRAY_SIZE(uv) == ARRAY_SIZE(log2_lookup_table),
		     "Test vector size must match reference table size");

	ret = memcpy_s((void *)&u[0], sizeof(u), (void *)&uv[0], 100U * sizeof(uint32_t));
	zassert_equal(ret, 0, "memcpy_s failed with error code %d", ret);

	for (i = 0; i < ARRAY_SIZE(u); i++) {
		float y = Q_CONVERT_QTOF(base2_logarithm(u[i]), 0);
		float delta = fabsf((float)(log2_lookup_table[i] - (double)y / (1 << 16)));

		zassert_true(delta <= CMP_TOLERANCE,
			     "base2_logarithm(%u): delta %.16f > tolerance (expected %.16f, got %.16f)",
			     u[i], (double)delta, log2_lookup_table[i], (double)y / (1 << 16));
	}
}
