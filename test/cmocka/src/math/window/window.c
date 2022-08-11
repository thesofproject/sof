// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

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
#include <sof/math/window.h>

#include "ref_window_rectangular.h"
#include "ref_window_blackman.h"
#include "ref_window_hamming.h"
#include "ref_window_povey.h"

#define MAX_ERR_RMS		1.0
#define MAX_STR_SIZE		256
#define MAX_WIN_LENGTH		4096
#undef DEBUG_FILES		/* Change to #define to get file output */

static float window_delta_rms(const int16_t *ref_win, int16_t *win, int win_length)
{
	float err_squared = 0.0;
	int i;

	for (i = 0; i < win_length; i++) {
		err_squared += ((float)ref_win[i] - (float)win[i]) *
			((float)ref_win[i] - (float)win[i]);
	}

	return sqrt(err_squared / (float)win_length);
}

static float test_window(char *window_name, const int16_t *ref_win, int window_length)
{
	float err_rms;
	int16_t *win;

	win = malloc(window_length * sizeof(int16_t));
	if (!win) {
		fprintf(stderr, "Failed to allocate reference window.\n");
		exit(EXIT_FAILURE);
	}

	/* calculate window and get RMS error vs. reference */
	if (!strcmp(window_name, "rectangular")) {
		win_rectangular_16b(win, window_length);
	} else if (!strcmp(window_name, "blackman")) {
		win_blackman_16b(win, window_length, WIN_BLACKMAN_A0);
	} else if (!strcmp(window_name, "hamming")) {
		win_hamming_16b(win, window_length);
	} else if (!strcmp(window_name, "povey")) {
		win_povey_16b(win, window_length);
	} else {
		fprintf(stderr, "Illegal window %s.\n", window_name);
		free(win);
		exit(EXIT_FAILURE);
	}

#ifdef DEBUG_FILES
	FILE *fh;
	int i;

	fh = fopen("window.txt", "w");
	for (i = 0; i < window_length; i++)
		fprintf(fh, "%d %d\n", win[i], ref_win[i]);

	fclose(fh);
#endif

	err_rms = window_delta_rms(ref_win, win, window_length);
	printf("Window %s RMS error = %5.2f LSB (max %5.2f)\n", window_name, err_rms, MAX_ERR_RMS);
	free(win);
	return err_rms;
}

static void test_math_window_rectangular(void **state)
{
	float err_rms;

	(void)state;

	err_rms = test_window("rectangular", ref_rectangular, LENGTH_RECTANGULAR);
	assert_true(err_rms < MAX_ERR_RMS);
}

static void test_math_window_blackman(void **state)
{
	float err_rms;

	(void)state;

	err_rms = test_window("blackman", ref_blackman, LENGTH_BLACKMAN);
	assert_true(err_rms < MAX_ERR_RMS);
}

static void test_math_window_hamming(void **state)
{
	float err_rms;

	(void)state;

	err_rms = test_window("hamming", ref_hamming, LENGTH_HAMMING);
	assert_true(err_rms < MAX_ERR_RMS);
}

static void test_math_window_povey(void **state)
{
	float err_rms;

	(void)state;

	err_rms = test_window("povey", ref_povey, LENGTH_POVEY);
	assert_true(err_rms < MAX_ERR_RMS);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_math_window_rectangular),
		cmocka_unit_test(test_math_window_blackman),
		cmocka_unit_test(test_math_window_hamming),
		cmocka_unit_test(test_math_window_povey),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
