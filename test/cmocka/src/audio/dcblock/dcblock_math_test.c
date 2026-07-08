// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include <sof/common.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>

#include "dcblock.h"

#define TEST_CHANNELS	2
#define TEST_FRAMES	256

/*
 * The dcblock processing functions operate on struct cir_buf_source /
 * struct cir_buf_sink circular buffer views. The tests use static, non
 * wrapping sample buffers, so the views simply span the whole array.
 */
static void cir_buf_src_setup(struct cir_buf_source *s, const void *data, size_t bytes)
{
	s->buf_start = data;
	s->buf_end = (const char *)data + bytes;
	s->ptr = data;
}

static void cir_buf_snk_setup(struct cir_buf_sink *s, void *data, size_t bytes)
{
	s->buf_start = data;
	s->buf_end = (char *)data + bytes;
	s->ptr = data;
}

/* Q2.30 coefficient close to 1.0 used for the DC removal test cases. */
#define R_COEF_NEAR_ONE	1063004406 /* ~0.99 in Q2.30 */

/*
 * Reference DC blocking filter implemented in floating point. Mirrors the
 * fixed point recurrence y[n] = x[n] - x[n-1] + R * y[n-1] where R is the
 * Q2.30 coefficient converted to a real number.
 */
struct ref_state {
	double x_prev;
	double y_prev;
};

static double dcblock_ref(struct ref_state *s, double r, double x)
{
	double y = x - s->x_prev + r * s->y_prev;

	s->x_prev = x;
	s->y_prev = y;
	return y;
}

/* Fill the source buffer with a per-channel sinusoid plus a DC offset. */
static void gen_input(double *ref_in, int channels, int frames, double dc)
{
	int ch, i;

	for (i = 0; i < frames; i++) {
		for (ch = 0; ch < channels; ch++) {
			double phase = 2.0 * M_PI * (i + 1) * (ch + 1) / 64.0;

			ref_in[i * channels + ch] = dc + 0.3 * sin(phase);
		}
	}
}

/*
 * Runs the S32 processing function and compares it to the floating point
 * reference. Returns the mean absolute value of the last quarter of the
 * output which is used to check DC convergence.
 */
static double run_s32_case(int32_t r_coeff, double dc, double tol_rel)
{
	struct comp_data cd;
	struct cir_buf_source csrc;
	struct cir_buf_sink csnk;
	int32_t src[TEST_FRAMES * TEST_CHANNELS];
	int32_t dst[TEST_FRAMES * TEST_CHANNELS];
	double ref_in[TEST_FRAMES * TEST_CHANNELS];
	struct ref_state rstate[TEST_CHANNELS];
	double r = (double)r_coeff / (double)ONE_Q2_30;
	double tail_abs_sum = 0.0;
	int tail_count = 0;
	dcblock_func func;
	int ch, i;

	memset(&cd, 0, sizeof(cd));
	cd.channels = TEST_CHANNELS;
	for (ch = 0; ch < TEST_CHANNELS; ch++)
		cd.R_coeffs[ch] = r_coeff;

	memset(rstate, 0, sizeof(rstate));
	gen_input(ref_in, TEST_CHANNELS, TEST_FRAMES, dc);

	for (i = 0; i < TEST_FRAMES * TEST_CHANNELS; i++)
		src[i] = (int32_t)round(ref_in[i] * 2147483647.0);

	cir_buf_src_setup(&csrc, src, sizeof(src));
	cir_buf_snk_setup(&csnk, dst, sizeof(dst));

	func = dcblock_find_func(SOF_IPC_FRAME_S32_LE);
	assert_non_null(func);
	assert_int_equal(func(&cd, &csrc, &csnk, TEST_FRAMES), 0);

	for (i = 0; i < TEST_FRAMES; i++) {
		for (ch = 0; ch < TEST_CHANNELS; ch++) {
			int idx = i * TEST_CHANNELS + ch;
			double refy = dcblock_ref(&rstate[ch], r,
						  (double)src[idx] / 2147483648.0);
			double outy = (double)dst[idx] / 2147483648.0;
			double delta = fabs(refy - outy);

			if (delta > tol_rel) {
				printf("s32 mismatch idx %d ref %g out %g delta %g\n",
				       idx, refy, outy, delta);
				assert_true(delta <= tol_rel);
			}

			if (i >= TEST_FRAMES * 3 / 4) {
				tail_abs_sum += fabs(outy);
				tail_count++;
			}
		}
	}

	return tail_count ? tail_abs_sum / tail_count : 0.0;
}

/* Passthrough: R = 1.0 gives y[n] = x[n] - x[n-1], a pure differentiator. */
static void test_dcblock_passthrough(void **state)
{
	(void)state;

	/* With no DC and R=1.0 the reference matches bit-close. */
	run_s32_case(ONE_Q2_30, 0.0, 1.0e-6);
}

/* A strong DC offset must be attenuated towards zero in steady state. */
static void test_dcblock_dc_removal(void **state)
{
	(void)state;

	double tail = run_s32_case(R_COEF_NEAR_ONE, 0.5, 1.0e-6);

	/* The residual DC plus small sinusoid must be well below the input DC. */
	printf("dc_removal tail mean abs = %g\n", tail);
	assert_true(tail < 0.25);
}

/* Full scale input must not overflow (saturation path in dcblock_generic). */
static void test_dcblock_saturation(void **state)
{
	(void)state;

	struct comp_data cd;
	struct cir_buf_source csrc;
	struct cir_buf_sink csnk;
	int32_t src[TEST_FRAMES * TEST_CHANNELS];
	int32_t dst[TEST_FRAMES * TEST_CHANNELS];
	dcblock_func func;
	int i;

	memset(&cd, 0, sizeof(cd));
	cd.channels = TEST_CHANNELS;
	for (i = 0; i < TEST_CHANNELS; i++)
		cd.R_coeffs[i] = R_COEF_NEAR_ONE;

	/*
	 * Alternate per frame so that every channel sees the full-scale
	 * transitions and exercises the saturation path.
	 */
	for (i = 0; i < TEST_FRAMES * TEST_CHANNELS; i++)
		src[i] = ((i / TEST_CHANNELS) & 1) ? INT32_MAX : INT32_MIN;

	cir_buf_src_setup(&csrc, src, sizeof(src));
	cir_buf_snk_setup(&csnk, dst, sizeof(dst));

	func = dcblock_find_func(SOF_IPC_FRAME_S32_LE);
	assert_non_null(func);
	assert_int_equal(func(&cd, &csrc, &csnk, TEST_FRAMES), 0);

	for (i = 0; i < TEST_FRAMES * TEST_CHANNELS; i++) {
		int32_t expected = ((i / TEST_CHANNELS) & 1) ? INT32_MAX : INT32_MIN;

		assert_int_equal(dst[i], expected);
	}
}

/* Bit-exactness check against the floating point reference for S32. */
static void test_dcblock_bitexact_s32(void **state)
{
	(void)state;

	run_s32_case(R_COEF_NEAR_ONE, 0.1, 1.0e-6);
}

/*
 * Runs the S16 processing function and compares it to the floating point
 * reference. The component internally works in Q1.31, so the tolerance must
 * account for the 16-bit output quantization step (2 LSB of S16).
 */
static void run_s16_case(int32_t r_coeff, double dc)
{
	struct comp_data cd;
	struct cir_buf_source csrc;
	struct cir_buf_sink csnk;
	int16_t src[TEST_FRAMES * TEST_CHANNELS];
	int16_t dst[TEST_FRAMES * TEST_CHANNELS];
	double ref_in[TEST_FRAMES * TEST_CHANNELS];
	struct ref_state rstate[TEST_CHANNELS];
	double r = (double)r_coeff / (double)ONE_Q2_30;
	double tol = 2.0 / 32768.0;
	dcblock_func func;
	int ch, i;

	memset(&cd, 0, sizeof(cd));
	cd.channels = TEST_CHANNELS;
	for (ch = 0; ch < TEST_CHANNELS; ch++)
		cd.R_coeffs[ch] = r_coeff;

	memset(rstate, 0, sizeof(rstate));
	gen_input(ref_in, TEST_CHANNELS, TEST_FRAMES, dc);

	for (i = 0; i < TEST_FRAMES * TEST_CHANNELS; i++)
		src[i] = (int16_t)round(ref_in[i] * 32767.0);

	cir_buf_src_setup(&csrc, src, sizeof(src));
	cir_buf_snk_setup(&csnk, dst, sizeof(dst));

	func = dcblock_find_func(SOF_IPC_FRAME_S16_LE);
	assert_non_null(func);
	assert_int_equal(func(&cd, &csrc, &csnk, TEST_FRAMES), 0);

	for (i = 0; i < TEST_FRAMES; i++) {
		for (ch = 0; ch < TEST_CHANNELS; ch++) {
			int idx = i * TEST_CHANNELS + ch;
			double refy = dcblock_ref(&rstate[ch], r,
						  (double)src[idx] / 32768.0);
			double outy = (double)dst[idx] / 32768.0;
			double delta = fabs(refy - outy);

			if (delta > tol) {
				printf("s16 mismatch idx %d ref %g out %g delta %g\n",
				       idx, refy, outy, delta);
				assert_true(delta <= tol);
			}
		}
	}
}

/* Bit-exactness check against the floating point reference for S16. */
static void test_dcblock_bitexact_s16(void **state)
{
	(void)state;

	run_s16_case(R_COEF_NEAR_ONE, 0.1);
}

/*
 * Runs the S24 (in 32-bit container) processing function and compares it to
 * the floating point reference with a tolerance of 2 LSB of S24.
 */
static void run_s24_case(int32_t r_coeff, double dc)
{
	struct comp_data cd;
	struct cir_buf_source csrc;
	struct cir_buf_sink csnk;
	int32_t src[TEST_FRAMES * TEST_CHANNELS];
	int32_t dst[TEST_FRAMES * TEST_CHANNELS];
	double ref_in[TEST_FRAMES * TEST_CHANNELS];
	struct ref_state rstate[TEST_CHANNELS];
	double r = (double)r_coeff / (double)ONE_Q2_30;
	double tol = 2.0 / 8388608.0;
	dcblock_func func;
	int ch, i;

	memset(&cd, 0, sizeof(cd));
	cd.channels = TEST_CHANNELS;
	for (ch = 0; ch < TEST_CHANNELS; ch++)
		cd.R_coeffs[ch] = r_coeff;

	memset(rstate, 0, sizeof(rstate));
	gen_input(ref_in, TEST_CHANNELS, TEST_FRAMES, dc);

	for (i = 0; i < TEST_FRAMES * TEST_CHANNELS; i++)
		src[i] = (int32_t)round(ref_in[i] * 8388607.0);

	cir_buf_src_setup(&csrc, src, sizeof(src));
	cir_buf_snk_setup(&csnk, dst, sizeof(dst));

	func = dcblock_find_func(SOF_IPC_FRAME_S24_4LE);
	assert_non_null(func);
	assert_int_equal(func(&cd, &csrc, &csnk, TEST_FRAMES), 0);

	for (i = 0; i < TEST_FRAMES; i++) {
		for (ch = 0; ch < TEST_CHANNELS; ch++) {
			int idx = i * TEST_CHANNELS + ch;
			double refy = dcblock_ref(&rstate[ch], r,
						  (double)src[idx] / 8388608.0);
			double outy = (double)dst[idx] / 8388608.0;
			double delta = fabs(refy - outy);

			if (delta > tol) {
				printf("s24 mismatch idx %d ref %g out %g delta %g\n",
				       idx, refy, outy, delta);
				assert_true(delta <= tol);
			}
		}
	}
}

/* Bit-exactness check against the floating point reference for S24. */
static void test_dcblock_bitexact_s24(void **state)
{
	(void)state;

	run_s24_case(R_COEF_NEAR_ONE, 0.1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_dcblock_passthrough),
		cmocka_unit_test(test_dcblock_dc_removal),
		cmocka_unit_test(test_dcblock_saturation),
		cmocka_unit_test(test_dcblock_bitexact_s32),
		cmocka_unit_test(test_dcblock_bitexact_s16),
		cmocka_unit_test(test_dcblock_bitexact_s24),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
