// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/math/window/window.c:
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/math/window.h>

#include <stdint.h>
#include <math.h>

/* Reference windows shared with the legacy CMocka test. They are #included
 * in place from the CMocka tree (added to the include path in CMakeLists.txt)
 * so the exact same fixed-point data is exercised.
 */
#include "ref_window_rectangular.h"
#include "ref_window_blackman.h"
#include "ref_window_hamming.h"
#include "ref_window_povey.h"

/* Maximum tolerated RMS error against the reference, in LSB */
#define MAX_ERR_RMS 1.0f

/* Scratch window, large enough for the longest reference vector */
#define MAX_WIN_LENGTH LENGTH_HAMMING

BUILD_ASSERT(LENGTH_RECTANGULAR <= MAX_WIN_LENGTH);
BUILD_ASSERT(LENGTH_BLACKMAN <= MAX_WIN_LENGTH);
BUILD_ASSERT(LENGTH_HAMMING <= MAX_WIN_LENGTH);
BUILD_ASSERT(LENGTH_POVEY <= MAX_WIN_LENGTH);

static int16_t win[MAX_WIN_LENGTH];

/**
 * @brief Compute the RMS error of a calculated window against its reference.
 * @param[in] ref_win Reference Q1.15 coefficients.
 * @param[in] win Calculated Q1.15 coefficients.
 * @param[in] win_length Number of coefficients to compare.
 * @return RMS of the per-coefficient error, in LSB.
 */
static float window_delta_rms(const int16_t *ref_win, const int16_t *win, int win_length)
{
	float err_squared = 0.0;
	int i;

	for (i = 0; i < win_length; i++) {
		err_squared +=
			((float)ref_win[i] - (float)win[i]) * ((float)ref_win[i] - (float)win[i]);
	}

	return sqrtf(err_squared / (float)win_length);
}

/**
 * @brief Check the window left in win[] against a reference vector.
 * @param[in] window_name Window name, for the test log.
 * @param[in] ref_win Reference Q1.15 coefficients.
 * @param[in] window_length Number of coefficients to compare.
 */
static void check_window(const char *window_name, const int16_t *ref_win, int window_length)
{
	float err_rms = window_delta_rms(ref_win, win, window_length);

	/* printk() is varargs, so the float arguments are promoted to double
	 * anyway; cast explicitly to keep -Wdouble-promotion quiet.
	 */
	TC_PRINT("Window %s RMS error = %5.2f LSB (max %5.2f)\n", window_name, (double)err_rms,
		 (double)MAX_ERR_RMS);

	zassert_true(err_rms < MAX_ERR_RMS, "Window %s RMS error %5.2f exceeds limit %5.2f",
		     window_name, (double)err_rms, (double)MAX_ERR_RMS);
}

/**
 * @brief Verify the rectangular window against its reference vector
 */
ZTEST(math_window_suite, test_math_window_rectangular)
{
	win_rectangular_16b(win, LENGTH_RECTANGULAR);
	check_window("rectangular", ref_rectangular, LENGTH_RECTANGULAR);
}

/**
 * @brief Verify the Blackman window against its reference vector
 */
ZTEST(math_window_suite, test_math_window_blackman)
{
	win_blackman_16b(win, LENGTH_BLACKMAN, WIN_BLACKMAN_A0_Q15);
	check_window("blackman", ref_blackman, LENGTH_BLACKMAN);
}

/**
 * @brief Verify the Hamming window against its reference vector
 */
ZTEST(math_window_suite, test_math_window_hamming)
{
	win_hamming_16b(win, LENGTH_HAMMING);
	check_window("hamming", ref_hamming, LENGTH_HAMMING);
}

/**
 * @brief Verify the Povey window against its reference vector
 */
ZTEST(math_window_suite, test_math_window_povey)
{
	win_povey_16b(win, LENGTH_POVEY);
	check_window("povey", ref_povey, LENGTH_POVEY);
}

/**
 * @brief Define and initialize the window function test suite
 */
ZTEST_SUITE(math_window_suite, NULL, NULL, NULL, NULL, NULL);
