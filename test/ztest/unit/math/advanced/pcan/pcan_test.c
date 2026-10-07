// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.
//
// Converted from cmocka to Ztest
// Original: test/cmocka/src/math/pcan/pcan_test.c

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/ztest.h>

#include <sof/math/pcan.h>

#include "ref_pcan_lut.h"
#include "ref_pcan_func.h"
#include "ref_pcan_stream.h"
#include "ref_pcan_corners.h"

/* The SOF PCAN core computes the gain LUT in fixed-point (ln_int32 +
 * sofm_exp_fixed) while the Octave reference uses double-precision powf.
 * The two agree to within a couple of LSBs on the sampled y-values, but the
 * quadratic slope terms a1 = 4*(y1-y0) - (y2-y0) and a2 = -a1 + (y2-y0)
 * amplify per-sample rounding by up to ~6x, so allow a bigger LUT tolerance.
 */
#define PCAN_TEST_LSB_TOL	2
#define PCAN_TEST_LUT_TOL	6

#define zassert_int_near(actual, expected, tol) \
	do { \
		long long _a = (long long)(actual); \
		long long _e = (long long)(expected); \
		long long _d = _a - _e; \
		\
		if (_d < 0) \
			_d = -_d; \
		zassert_true(_d <= (long long)(tol), \
			     "value %lld deviates from reference %lld by %lld (tol %lld)", \
			     _a, _e, _d, (long long)(tol)); \
	} while (0)

ZTEST(pcan_suite, test_pcan_lut_generation)
{
	int16_t lut[PCAN_LUT_SIZE];
	int ret;
	int i;

	ret = pcan_compute_lut(0.95f, 80.0f, 21, 10, lut);
	zassert_equal(ret, 0);

	for (i = 0; i < PCAN_TEST_LUT1_SIZE; i++)
		zassert_int_near(lut[i], ref_pcan_lut1[i], PCAN_TEST_LUT_TOL);

	ret = pcan_compute_lut(0.8f, 50.0f, 18, 12, lut);
	zassert_equal(ret, 0);

	for (i = 0; i < PCAN_TEST_LUT2_SIZE; i++)
		zassert_int_near(lut[i], ref_pcan_lut2[i], PCAN_TEST_LUT_TOL);
}

ZTEST(pcan_suite, test_pcan_wide_dynamic_function)
{
	int16_t lut[PCAN_LUT_SIZE];
	int16_t out;
	int ret;
	int i;

	ret = pcan_compute_lut(0.95f, 80.0f, 21, 10, lut);
	zassert_equal(ret, 0);

	for (i = 0; i < PCAN_TEST_WDF_NUM_POINTS; i++) {
		out = pcan_wide_dynamic_function(ref_pcan_wdf_inputs[i], lut);
		zassert_int_near(out, ref_pcan_wdf_outputs[i], PCAN_TEST_LSB_TOL);
	}
}

ZTEST(pcan_suite, test_pcan_shrink)
{
	uint32_t out;
	int i;

	for (i = 0; i < PCAN_TEST_SHRINK_NUM_POINTS; i++) {
		out = pcan_shrink(ref_pcan_shrink_inputs[i]);
		zassert_equal(out, ref_pcan_shrink_outputs[i]);
	}
}

ZTEST(pcan_suite, test_pcan_streaming)
{
	struct pcan_config cfg;
	struct pcan_state pstate;
	uint32_t channel_data[PCAN_STREAM_NUM_CHANNELS];
	int ret;
	int f;
	int c;

	cfg.strength = 0.95f;
	cfg.offset = 80.0f;
	cfg.gain_bits = 21;
	cfg.smoothing_coef = PCAN_STREAM_SMOOTHING_COEF;
	cfg.smoothing_bits = PCAN_STREAM_SMOOTHING_BITS;
	cfg.input_correction_bits = PCAN_STREAM_INPUT_CORRECTION_BITS;
	cfg.enable_pcan = true;

	ret = pcan_populate_state(&cfg, &pstate, NULL, NULL,
				  PCAN_STREAM_NUM_CHANNELS,
				  PCAN_STREAM_SMOOTHING_BITS,
				  PCAN_STREAM_INPUT_CORRECTION_BITS);
	zassert_equal(ret, 0);

	for (f = 0; f < PCAN_STREAM_NUM_FRAMES; f++) {
		for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++)
			channel_data[c] = ref_pcan_stream_inputs[f * PCAN_STREAM_NUM_CHANNELS + c];

		pcan_noise_reduction(&pstate, channel_data);
		pcan_apply(&pstate, channel_data);
		pcan_log_scale(&pstate, channel_data);
	}

	for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++)
		zassert_true(pstate.noise_estimate[c] > 0, "noise estimate should be positive");

	pcan_reset(&pstate);
	for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++)
		zassert_equal(pstate.noise_estimate[c], 0);

	pcan_free_state(&pstate);
}

ZTEST(pcan_suite, test_pcan_corner_cases)
{
	int16_t lut[PCAN_LUT_SIZE];
	struct pcan_config bad_cfg;
	struct pcan_state bad_state;
	int16_t wdf_out;
	uint32_t shrink_out;
	int ret;
	int i;

	ret = pcan_compute_lut(0.95f, 80.0f, 21, 10, lut);
	zassert_equal(ret, 0);

	for (i = 0; i < PCAN_CORNERS_NUM_POINTS; i++) {
		wdf_out = pcan_wide_dynamic_function(ref_pcan_corner_inputs[i], lut);
		zassert_int_near(wdf_out, ref_pcan_corner_wdf_outputs[i], PCAN_TEST_LSB_TOL);

		shrink_out = pcan_shrink(ref_pcan_corner_inputs[i]);
		zassert_equal(shrink_out, ref_pcan_corner_shrink_outputs[i]);
	}

	/* gain_bits too small -> negative snr_shift */
	bad_cfg.strength = 0.95f;
	bad_cfg.offset = 80.0f;
	bad_cfg.gain_bits = 5;
	bad_cfg.smoothing_coef = 819;
	bad_cfg.smoothing_bits = 10;
	bad_cfg.input_correction_bits = 0;
	bad_cfg.enable_pcan = true;

	ret = pcan_populate_state(&bad_cfg, &bad_state, NULL, NULL, 16, 10, 0);
	zassert_true(ret < 0);

	ret = pcan_populate_state(NULL, &bad_state, NULL, NULL, 16, 10, 0);
	zassert_true(ret < 0);

	ret = pcan_populate_state(&bad_cfg, &bad_state, NULL, NULL, 0, 10, 0);
	zassert_true(ret < 0);
}

ZTEST_SUITE(pcan_suite, NULL, NULL, NULL, NULL, NULL);
