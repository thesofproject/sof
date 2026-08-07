// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Piotr Hoppe <piotr.hoppe@intel.com>

/**
 * @file crossover_lr4_test.c
 * @brief Unit tests for the LR4 (Linkwitz-Riley 4th order) biquad filter
 *        used by the Crossover component.
 *
 * The Crossover module decomposes each audio channel into frequency bands
 * using LR4 filters — each implemented as two identical 2nd-order biquads
 * cascaded in series, processed via iir_df1_4th().
 *
 * These tests verify the fundamental DSP properties of the LR4 filter
 * without depending on the full SOF component stack:
 *
 *  1. Identity biquad passes signal unchanged.
 *  2. LP LR4 passes DC and blocks near-Nyquist signals.
 *  3. HP LR4 blocks DC and passes near-Nyquist signals.
 *  4. LP and HP split input power equally at the crossover frequency
 *     (defining property of Linkwitz-Riley crossovers: each band is −3 dB).
 *  5. LP LR4 strongly attenuates high-frequency signals (selectivity).
 *  6. HP LR4 strongly attenuates low-frequency signals (selectivity).
 *  7. LP and HP biquads share the same denominator (poles are identical).
 *
 * Coefficient computation follows the Web Audio resonance filter design used
 * in src/audio/crossover/tune/sof_crossover_gen_coefs.m.
 */

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>

#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
#include <user/eq.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

/** Sample rate used for all tests */
#define FS 48000.0

/** Crossover frequency (Hz) */
#define FC_HZ 1000.0

/** Total signal length (samples) */
#define SIGNAL_LEN 512

/**
 * Number of leading samples to discard when measuring steady-state
 * response.  At 1 kHz / 48 kHz the LR4 time constant is ~5 periods
 * (~240 samples); 288 gives comfortable headroom.
 */
#define WARMUP_LEN 288

/** Q2.30: 1.0 in Q2.30 fixed-point (coefficient scale) */
#define Q30 ((int64_t)1 << 30)

/** Q2.14: 1.0 in Q2.14 fixed-point (iir_df1 output gain = unity) */
#define UNITY_GAIN_Q14 ((int32_t)(1 << 14))

/**
 * Number of int32_t words in one biquad coefficient set.
 * Order: {a2, a1, b2, b1, b0, output_shift, output_gain}.
 */
#define BIQUAD_NCOEF  SOF_EQ_IIR_NBIQUAD  /* 7 */

/** LR4 = two identical biquads in series */
#define LR4_NCOEF   (2 * BIQUAD_NCOEF)

/** 4 delay slots per biquad, 2 biquads */
#define LR4_NDELAY  (2 * IIR_DF1_NUM_STATE)

/* =========================================================================
 * Helper: biquad coefficient computation
 * =========================================================================
 *
 * Computes Butterworth 2nd-order LP/HP biquad coefficients using the same
 * Web Audio resonance filter design as sof_crossover_gen_coefs.m.
 *
 * The SOF iir_df1 engine stores a-coefficients NEGATED with respect to the
 * standard transfer function convention.  Standard IIR DF-I recurrence:
 *
 *   y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] − a1_std·y[n-1] − a2_std·y[n-2]
 *
 * SOF iir_df1 accumulates:
 *
 *   acc += coef[a1] · y[n-1] + coef[a2] · y[n-2]  (addition, not subtraction)
 *
 * Therefore:  coef[a1] = −a1_std,  coef[a2] = −a2_std.
 *
 * All b and negated-a coefficients are quantised to Q2.30.
 * Output gain is Q2.14 unity (16384).  Output shift is 0.
 */

/**
 * @brief Compute SOF-format biquad coefficients for a 2nd-order LP filter.
 *
 * @param fs  Sample rate (Hz).
 * @param fc  Cutoff frequency (Hz).
 * @param coef  Output array of BIQUAD_NCOEF int32_t values.
 */
static void compute_biquad_lp(double fs, double fc, int32_t coef[BIQUAD_NCOEF])
{
	double cutoff = fc / (fs / 2.0);
	double d = M_SQRT2;		/* resonance = 0 */
	double theta = M_PI * cutoff;
	double sn = 0.5 * d * sin(theta);
	double beta = 0.5 * (1.0 - sn) / (1.0 + sn);
	double gamma_v = (0.5 + beta) * cos(theta);
	double alpha = 0.25 * (0.5 + beta - gamma_v);

	double b0 =  2.0 * alpha;
	double b1 =  4.0 * alpha;	/* LP: positive b1 */
	double b2 =  2.0 * alpha;
	double a1_std = -2.0 * gamma_v;
	double a2_std =  2.0 * beta;

	coef[0] = (int32_t)llround(-a2_std * Q30);	/* negated a2 */
	coef[1] = (int32_t)llround(-a1_std * Q30);	/* negated a1 */
	coef[2] = (int32_t)llround(b2 * Q30);
	coef[3] = (int32_t)llround(b1 * Q30);
	coef[4] = (int32_t)llround(b0 * Q30);
	coef[5] = 0;				/* output_shift = 0 */
	coef[6] = UNITY_GAIN_Q14;		/* output_gain = 1.0 */
}

/**
 * @brief Compute SOF-format biquad coefficients for a 2nd-order HP filter.
 *
 * LP and HP share the same denominator (a-coefficients); only the
 * b-coefficients differ.
 *
 * @param fs  Sample rate (Hz).
 * @param fc  Cutoff frequency (Hz).
 * @param coef  Output array of BIQUAD_NCOEF int32_t values.
 */
static void compute_biquad_hp(double fs, double fc, int32_t coef[BIQUAD_NCOEF])
{
	double cutoff = fc / (fs / 2.0);
	double d = M_SQRT2;
	double theta = M_PI * cutoff;
	double sn = 0.5 * d * sin(theta);
	double beta = 0.5 * (1.0 - sn) / (1.0 + sn);
	double gamma_v = (0.5 + beta) * cos(theta);
	double alpha = 0.25 * (0.5 + beta + gamma_v);	/* HP: different alpha */

	double b0 =  2.0 * alpha;
	double b1 = -4.0 * alpha;	/* HP: negative b1 */
	double b2 =  2.0 * alpha;
	double a1_std = -2.0 * gamma_v;	/* same denominator as LP */
	double a2_std =  2.0 * beta;

	coef[0] = (int32_t)llround(-a2_std * Q30);
	coef[1] = (int32_t)llround(-a1_std * Q30);
	coef[2] = (int32_t)llround(b2 * Q30);
	coef[3] = (int32_t)llround(b1 * Q30);
	coef[4] = (int32_t)llround(b0 * Q30);
	coef[5] = 0;
	coef[6] = UNITY_GAIN_Q14;
}

/* =========================================================================
 * Helper: LR4 state initialisation
 * ========================================================================= */

/**
 * @brief Initialise an LR4 iir_state_df1 from a single biquad coefficient set.
 *
 * An LR4 filter is two identical biquads in series.  The coefficient buffer
 * must hold LR4_NCOEF words; the delay buffer must hold LR4_NDELAY words.
 *
 * @param lr4         IIR state to initialise.
 * @param coef        Buffer of LR4_NCOEF int32_t (storage for two biquads).
 * @param delay       Buffer of LR4_NDELAY int32_t (zeroed delay lines).
 * @param biquad_coef Single biquad coefficient set (BIQUAD_NCOEF words).
 */
static void init_lr4(struct iir_state_df1 *lr4,
		     int32_t coef[LR4_NCOEF],
		     int32_t delay[LR4_NDELAY],
		     const int32_t biquad_coef[BIQUAD_NCOEF])
{
	memcpy(coef,              biquad_coef, BIQUAD_NCOEF * sizeof(int32_t));
	memcpy(coef + BIQUAD_NCOEF, biquad_coef, BIQUAD_NCOEF * sizeof(int32_t));
	memset(delay, 0, LR4_NDELAY * sizeof(int32_t));
	lr4->biquads = 2;
	lr4->biquads_in_series = 2;
	lr4->coef = coef;
	lr4->delay = delay;
}

/* =========================================================================
 * Helper: signal metrics
 * ========================================================================= */

/**
 * @brief Compute the RMS of an array of Q1.31 samples.
 *
 * Converts each sample to double in [−1, 1] before squaring to avoid
 * integer overflow.
 *
 * @param buf  Array of int32_t Q1.31 samples.
 * @param n    Number of samples.
 * @return     RMS value in [0, 1].
 */
static double rms_q31(const int32_t *buf, int n)
{
	double sum = 0.0;
	int i;

	for (i = 0; i < n; i++) {
		double s = (double)buf[i] / (double)(1u << 31);

		sum += s * s;
	}
	return sqrt(sum / n);
}

/* =========================================================================
 * Tests
 * ========================================================================= */

/**
 * @brief Test 1 – identity biquad passes every sample unchanged.
 *
 * A biquad with b0 = 1.0 and all other coefficients zero implements y[n] = x[n].
 * Two such biquads in series (LR4) must also be y[n] = x[n].
 */
static void test_lr4_identity_passthrough(void **state)
{
	(void)state;

	struct iir_state_df1 lr4;
	int32_t coef[LR4_NCOEF];
	int32_t delay[LR4_NDELAY];

	/*
	 * Identity biquad: b0 = 1.0 (Q2.30), gain = 1.0 (Q2.14),
	 * a2 = a1 = b2 = b1 = 0, output_shift = 0.
	 */
	const int32_t identity_bq[BIQUAD_NCOEF] = {
		0, 0, 0, 0, (int32_t)Q30, 0, UNITY_GAIN_Q14
	};

	init_lr4(&lr4, coef, delay, identity_bq);

	/* Feed a 100 Hz sine and verify output equals input sample-by-sample */
	for (int i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(0.5 * INT32_MAX *
				      sin(2.0 * M_PI * 100.0 * i / FS));
		int32_t y = iir_df1_4th(&lr4, x);

		assert_int_equal(x, y);
	}
}

/**
 * @brief Test 2 – LP LR4 passes DC and blocks near-Nyquist signals.
 *
 * Steady-state DC output must be within 1 % of input.
 * Near-Nyquist (0.45·fs) output must be less than 1 % of input RMS.
 */
static void test_lr4_lp_dc_pass_hf_block(void **state)
{
	(void)state;

	struct iir_state_df1 lr4;
	int32_t coef[LR4_NCOEF];
	int32_t delay[LR4_NDELAY];
	int32_t bq_lp[BIQUAD_NCOEF];
	int32_t out[SIGNAL_LEN];
	double rms_in, rms_out_dc, rms_out_hf;
	int i;

	compute_biquad_lp(FS, FC_HZ, bq_lp);

	/* --- DC passthrough test --- */
	init_lr4(&lr4, coef, delay, bq_lp);
	int32_t x_dc = (int32_t)(0.5 * INT32_MAX);

	for (i = 0; i < SIGNAL_LEN; i++)
		out[i] = iir_df1_4th(&lr4, x_dc);

	rms_in = (double)x_dc / (1u << 31);
	rms_out_dc = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	printf("LP DC:  input rms=%.4f  output rms=%.4f\n", rms_in, rms_out_dc);
	assert_true(fabs(rms_out_dc - rms_in) / rms_in < 0.01);

	/* --- High-frequency blocking test --- */
	init_lr4(&lr4, coef, delay, bq_lp);
	double f_hf = 0.45 * FS;
	double amp = 0.5 * INT32_MAX;

	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * f_hf * i / FS));

		out[i] = iir_df1_4th(&lr4, x);
	}
	rms_out_hf = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	printf("LP HF:  f=%.0fHz  output rms=%.6f  (expect < %.4f)\n",
	       f_hf, rms_out_hf, 0.01 * amp / (1u << 31));
	assert_true(rms_out_hf < 0.01 * amp / (1u << 31));
}

/**
 * @brief Test 3 – HP LR4 blocks DC and passes near-Nyquist signals.
 *
 * Steady-state DC output must be less than 1 % of input.
 * Near-Nyquist (0.45·fs) output must be within 5 % of input RMS.
 */
static void test_lr4_hp_dc_block_hf_pass(void **state)
{
	(void)state;

	struct iir_state_df1 lr4;
	int32_t coef[LR4_NCOEF];
	int32_t delay[LR4_NDELAY];
	int32_t bq_hp[BIQUAD_NCOEF];
	int32_t out[SIGNAL_LEN];
	double rms_in, rms_out_dc, rms_out_hf;
	int i;

	compute_biquad_hp(FS, FC_HZ, bq_hp);

	/* --- DC blocking test --- */
	init_lr4(&lr4, coef, delay, bq_hp);
	int32_t x_dc = (int32_t)(0.5 * INT32_MAX);

	for (i = 0; i < SIGNAL_LEN; i++)
		out[i] = iir_df1_4th(&lr4, x_dc);

	rms_out_dc = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	printf("HP DC:  input rms=%.4f  output rms=%.6f  (expect < %.4f)\n",
	       (double)x_dc / (1u << 31), rms_out_dc,
	       0.01 * (double)x_dc / (1u << 31));
	assert_true(rms_out_dc < 0.01 * (double)x_dc / (1u << 31));

	/* --- High-frequency passthrough test --- */
	init_lr4(&lr4, coef, delay, bq_hp);
	double f_hf = 0.45 * FS;
	double amp = 0.5 * INT32_MAX;

	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * f_hf * i / FS));

		out[i] = iir_df1_4th(&lr4, x);
	}
	/* RMS of a sine = peak / sqrt(2) */
	rms_in = (amp / (1u << 31)) * M_SQRT1_2;
	rms_out_hf = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	printf("HP HF:  f=%.0fHz  input rms=%.4f  output rms=%.4f\n",
	       f_hf, rms_in, rms_out_hf);
	assert_true(fabs(rms_out_hf - rms_in) / rms_in < 0.05);
}

/**
 * @brief Test 4 – LP and HP power-split at the crossover frequency.
 *
 * At the crossover frequency, an LR4 Linkwitz-Riley filter produces:
 *   |LP| = |HP| = 0.5  (each output is −6 dB relative to input)
 *
 * This follows from the LR4 being the square of a Butterworth 2nd order:
 * each Butterworth biquad has gain 1/sqrt(2) at fc, so the cascade of two
 * gives (1/sqrt(2))^2 = 0.5.  LP and HP gain being identical at fc is the
 * defining symmetry property of Linkwitz-Riley crossovers.
 *
 * Feeding a sine at fc Hz and measuring steady-state RMS must satisfy:
 *   |rms_lp − rms_hp| / rms_in < 5 %  (symmetric split)
 *   |rms_lp − 0.5·rms_in| / rms_in < 5 %  (correct −6 dB level)
 */
static void test_lr4_crossover_equal_power_split(void **state)
{
	(void)state;

	struct iir_state_df1 lr4_lp, lr4_hp;
	int32_t coef_lp[LR4_NCOEF], coef_hp[LR4_NCOEF];
	int32_t delay_lp[LR4_NDELAY], delay_hp[LR4_NDELAY];
	int32_t bq_lp[BIQUAD_NCOEF], bq_hp[BIQUAD_NCOEF];
	int32_t out_lp[SIGNAL_LEN], out_hp[SIGNAL_LEN];
	double amp = 0.5 * INT32_MAX;
	int i;

	compute_biquad_lp(FS, FC_HZ, bq_lp);
	compute_biquad_hp(FS, FC_HZ, bq_hp);
	init_lr4(&lr4_lp, coef_lp, delay_lp, bq_lp);
	init_lr4(&lr4_hp, coef_hp, delay_hp, bq_hp);

	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * FC_HZ * i / FS));

		out_lp[i] = iir_df1_4th(&lr4_lp, x);
		out_hp[i] = iir_df1_4th(&lr4_hp, x);
	}

	/* Discard transient and compute steady-state RMS */
	double rms_lp = rms_q31(out_lp + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double rms_hp = rms_q31(out_hp + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double rms_in = (amp / (1u << 31)) * M_SQRT1_2;
	/*
	 * LR4 is two 2nd-order Butterworth sections in series.  At the
	 * cutoff frequency each Butterworth section has gain = 1/sqrt(2),
	 * so the LR4 gain at fc is (1/sqrt(2))^2 = 0.5  (−6 dB).
	 */
	double expected = rms_in * 0.5;

	printf("Crossover split at %.0fHz:  rms_lp=%.4f  rms_hp=%.4f  "
	       "expected=%.4f\n", FC_HZ, rms_lp, rms_hp, expected);

	/* LP and HP must have equal level at the crossover frequency */
	assert_true(fabs(rms_lp - rms_hp) / rms_in < 0.05);

	/* Each output must be at −6 dB (amplitude × 0.5) relative to input */
	assert_true(fabs(rms_lp - expected) / rms_in < 0.05);
	assert_true(fabs(rms_hp - expected) / rms_in < 0.05);
}

/**
 * @brief Test 5 – LP LR4 frequency selectivity.
 *
 * At 100 Hz (one decade below crossover) the LP gain ≈ 1.
 * At 10 kHz (one decade above crossover) the LP gain ≈ 0.
 * The ratio of the two output RMS values must be at least 100 (40 dB).
 */
static void test_lr4_lp_frequency_selectivity(void **state)
{
	(void)state;

	struct iir_state_df1 lr4;
	int32_t coef[LR4_NCOEF];
	int32_t delay[LR4_NDELAY];
	int32_t bq_lp[BIQUAD_NCOEF];
	int32_t out[SIGNAL_LEN];
	double amp = 0.5 * INT32_MAX;
	double rms_lf, rms_hf;
	int i;

	compute_biquad_lp(FS, FC_HZ, bq_lp);

	/* Low frequency (100 Hz) — should pass */
	init_lr4(&lr4, coef, delay, bq_lp);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 100.0 * i / FS));

		out[i] = iir_df1_4th(&lr4, x);
	}
	rms_lf = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	/* High frequency (10 kHz) — should be blocked */
	init_lr4(&lr4, coef, delay, bq_lp);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 10000.0 * i / FS));

		out[i] = iir_df1_4th(&lr4, x);
	}
	rms_hf = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	printf("LP selectivity:  rms@100Hz=%.4f  rms@10kHz=%.6f  "
	       "ratio=%.1f  (expect > 100)\n",
	       rms_lf, rms_hf, rms_lf / (rms_hf + 1e-12));

	/* LP should be at least 40 dB stronger at 100 Hz than at 10 kHz */
	assert_true(rms_lf > 100.0 * rms_hf);
}

/**
 * @brief Test 6 – HP LR4 frequency selectivity.
 *
 * At 10 kHz (one decade above crossover) the HP gain ≈ 1.
 * At 100 Hz (one decade below crossover) the HP gain ≈ 0.
 * The ratio of the two output RMS values must be at least 100 (40 dB).
 */
static void test_lr4_hp_frequency_selectivity(void **state)
{
	(void)state;

	struct iir_state_df1 lr4;
	int32_t coef[LR4_NCOEF];
	int32_t delay[LR4_NDELAY];
	int32_t bq_hp[BIQUAD_NCOEF];
	int32_t out[SIGNAL_LEN];
	double amp = 0.5 * INT32_MAX;
	double rms_lf, rms_hf;
	int i;

	compute_biquad_hp(FS, FC_HZ, bq_hp);

	/* Low frequency (100 Hz) — should be blocked */
	init_lr4(&lr4, coef, delay, bq_hp);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 100.0 * i / FS));

		out[i] = iir_df1_4th(&lr4, x);
	}
	rms_lf = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	/* High frequency (10 kHz) — should pass */
	init_lr4(&lr4, coef, delay, bq_hp);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 10000.0 * i / FS));

		out[i] = iir_df1_4th(&lr4, x);
	}
	rms_hf = rms_q31(out + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	printf("HP selectivity:  rms@100Hz=%.6f  rms@10kHz=%.4f  "
	       "ratio=%.1f  (expect > 100)\n",
	       rms_lf, rms_hf, rms_hf / (rms_lf + 1e-12));

	/* HP should be at least 40 dB stronger at 10 kHz than at 100 Hz */
	assert_true(rms_hf > 100.0 * rms_lf);
}

/**
 * @brief Test 7 – LP and HP biquads share the same denominator.
 *
 * A fundamental property of Linkwitz-Riley crossovers: LP and HP filters
 * have identical poles (same a-coefficients).  Verifies that the coefficient
 * computation is consistent.
 */
static void test_lr4_lp_hp_same_denominator(void **state)
{
	(void)state;

	int32_t bq_lp[BIQUAD_NCOEF];
	int32_t bq_hp[BIQUAD_NCOEF];

	compute_biquad_lp(FS, FC_HZ, bq_lp);
	compute_biquad_hp(FS, FC_HZ, bq_hp);

	/* coef[0] = a2 (negated), coef[1] = a1 (negated) — must match */
	assert_int_equal(bq_lp[0], bq_hp[0]);	/* a2 */
	assert_int_equal(bq_lp[1], bq_hp[1]);	/* a1 */

	/* b-coefficients must differ */
	assert_int_not_equal(bq_lp[3], bq_hp[3]);	/* b1 has opposite sign */
}

/* =========================================================================
 * Test entry point
 * ========================================================================= */

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lr4_identity_passthrough),
		cmocka_unit_test(test_lr4_lp_dc_pass_hf_block),
		cmocka_unit_test(test_lr4_hp_dc_block_hf_pass),
		cmocka_unit_test(test_lr4_crossover_equal_power_split),
		cmocka_unit_test(test_lr4_lp_frequency_selectivity),
		cmocka_unit_test(test_lr4_hp_frequency_selectivity),
		cmocka_unit_test(test_lr4_lp_hp_same_denominator),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
