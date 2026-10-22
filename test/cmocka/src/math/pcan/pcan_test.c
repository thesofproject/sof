// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Antigravity AI & SOF Team

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <math.h>

#include <sof/math/pcan.h>

#include "ref_pcan_lut.h"
#include "ref_pcan_func.h"
#include "ref_pcan_stream.h"
#include "ref_pcan_corners.h"

/**
 * \brief Phase 1: Test LUT Generation against Octave reference
 */
static void test_pcan_lut_generation(void **state)
{
	(void)state;
	int16_t lut[PCAN_LUT_SIZE];
	int ret;
	int i;

	/* Test Configuration 1 (Default TFLM settings) */
	ret = pcan_compute_lut(0.95f, 80.0f, 21, 10, lut);
	assert_int_equal(ret, 0);

	for (i = 0; i < PCAN_TEST_LUT1_SIZE; i++) {
		assert_int_equal(lut[i], ref_pcan_lut1[i]);
	}

	/* Test Configuration 2 (Custom settings) */
	ret = pcan_compute_lut(0.8f, 50.0f, 18, 12, lut);
	assert_int_equal(ret, 0);

	for (i = 0; i < PCAN_TEST_LUT2_SIZE; i++) {
		assert_int_equal(lut[i], ref_pcan_lut2[i]);
	}
}

/**
 * \brief Phase 2: Test WideDynamicFunction quadratic interpolation against Octave reference
 */
static void test_pcan_wide_dynamic_function(void **state)
{
	(void)state;
	int16_t lut[PCAN_LUT_SIZE];
	int16_t out;
	int ret;
	int i;

	ret = pcan_compute_lut(0.95f, 80.0f, 21, 10, lut);
	assert_int_equal(ret, 0);

	for (i = 0; i < PCAN_TEST_WDF_NUM_POINTS; i++) {
		out = pcan_wide_dynamic_function(ref_pcan_wdf_inputs[i], lut);
		assert_int_equal(out, ref_pcan_wdf_outputs[i]);
	}
}

/**
 * \brief Phase 3: Test PcanShrink piecewise compression against Octave reference
 */
static void test_pcan_shrink(void **state)
{
	(void)state;
	uint32_t out;
	int i;

	for (i = 0; i < PCAN_TEST_SHRINK_NUM_POINTS; i++) {
		out = pcan_shrink(ref_pcan_shrink_inputs[i]);
		assert_int_equal(out, ref_pcan_shrink_outputs[i]);
	}
}

/**
 * \brief Phase 4: Test Full Streaming Multi-Channel Processing & IIR Noise Estimation
 */
static void test_pcan_streaming(void **state)
{
	(void)state;
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
	assert_int_equal(ret, 0);

	/* Stream frame-by-frame and check outputs */
	for (f = 0; f < PCAN_STREAM_NUM_FRAMES; f++) {
		for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++) {
			channel_data[c] = ref_pcan_stream_inputs[f * PCAN_STREAM_NUM_CHANNELS + c];
		}

		pcan_update_noise_estimate(&pstate, channel_data);
		pcan_apply(&pstate, channel_data);

		for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++) {
			uint32_t expected = ref_pcan_stream_outputs[f * PCAN_STREAM_NUM_CHANNELS + c];
			assert_int_equal(channel_data[c], expected);
		}
	}

	/* Verify final noise estimate vector */
	for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++) {
		assert_int_equal(pstate.noise_estimate[c], ref_pcan_stream_final_noise[c]);
	}

	/* Test reset functionality */
	pcan_reset(&pstate);
	for (c = 0; c < PCAN_STREAM_NUM_CHANNELS; c++) {
		assert_int_equal(pstate.noise_estimate[c], 0);
	}

	pcan_free_state(&pstate);
}

/**
 * \brief Phase 5: Test Corner and Boundary Cases
 */
static void test_pcan_corner_cases(void **state)
{
	(void)state;
	int16_t lut[PCAN_LUT_SIZE];
	struct pcan_config bad_cfg;
	struct pcan_state bad_state;
	int16_t wdf_out;
	uint32_t shrink_out;
	int ret;
	int i;

	ret = pcan_compute_lut(0.95f, 80.0f, 21, 10, lut);
	assert_int_equal(ret, 0);

	/* Test numerical corner cases */
	for (i = 0; i < PCAN_CORNERS_NUM_POINTS; i++) {
		wdf_out = pcan_wide_dynamic_function(ref_pcan_corner_inputs[i], lut);
		assert_int_equal(wdf_out, ref_pcan_corner_wdf_outputs[i]);

		shrink_out = pcan_shrink(ref_pcan_corner_inputs[i]);
		assert_int_equal(shrink_out, ref_pcan_corner_shrink_outputs[i]);
	}

	/* Test invalid configuration parameters */
	bad_cfg.strength = 0.95f;
	bad_cfg.offset = 80.0f;
	bad_cfg.gain_bits = 5; /* Too small -> negative snr_shift */
	bad_cfg.smoothing_coef = 819;
	bad_cfg.smoothing_bits = 10;
	bad_cfg.input_correction_bits = 0;
	bad_cfg.enable_pcan = true;

	ret = pcan_populate_state(&bad_cfg, &bad_state, NULL, NULL, 16, 10, 0);
	assert_true(ret < 0);

	ret = pcan_populate_state(NULL, &bad_state, NULL, NULL, 16, 10, 0);
	assert_true(ret < 0);

	ret = pcan_populate_state(&bad_cfg, &bad_state, NULL, NULL, 0, 10, 0);
	assert_true(ret < 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_pcan_lut_generation),
		cmocka_unit_test(test_pcan_wide_dynamic_function),
		cmocka_unit_test(test_pcan_shrink),
		cmocka_unit_test(test_pcan_streaming),
		cmocka_unit_test(test_pcan_corner_cases),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
