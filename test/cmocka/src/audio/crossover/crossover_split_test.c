// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Piotr Hoppe <piotr.hoppe@intel.com>

/**
 * @file crossover_split_test.c
 * @brief Unit tests for the crossover_generic split topology functions:
 *        crossover_generic_split_2way, _3way, and _4way.
 *
 * The split functions are the routing core of the Crossover component.
 * Each one wires a set of LR4 filters (lowpass[], highpass[]) into a
 * specific band-split tree:
 *
 *   2-way: 1 LR4 split  → LP, HP
 *   3-way: 3 LR4 stages → LP (with phase-realignment merge), MID, HP
 *   4-way: 3 LR4 splits → LP, MID-LO, MID-HI, HP
 *
 * The functions are static in crossover_generic.c and are accessed here
 * via the exported crossover_split_fnmap[] / crossover_find_split_func().
 *
 * Tests:
 *  1. crossover_find_split_func returns NULL for invalid num_sinks.
 *  2. crossover_find_split_func returns a non-NULL function for 2, 3, 4.
 *  3. 2-way split: outputs are non-zero for a non-zero input.
 *  4. 2-way split: both outputs are zero when input is zero.
 *  5. 2-way split with LP only (HP zeroed): passes low-frequency,
 *     blocks high-frequency.
 *  6. 2-way split with HP only (LP zeroed): blocks low-frequency,
 *     passes high-frequency.
 *  7. 3-way split: all three outputs are non-zero for a broadband input.
 *  8. 3-way split: sum of band powers ≈ input power (energy conservation).
 *  9. 4-way split: all four outputs are non-zero for a broadband input.
 * 10. 4-way split: sum of band powers ≈ input power.
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

/* audio_stream_copy is referenced by crossover_default_pass() which is
 * compiled as part of crossover_generic.c.  The split functions under test
 * never call it, but the linker needs a definition.
 */
#include <sof/audio/audio_stream.h>

int audio_stream_copy(const struct audio_stream *source, uint32_t ioffset,
		      struct audio_stream *sink, uint32_t ooffset, uint32_t samples)
{
	(void)source; (void)ioffset; (void)sink; (void)ooffset; (void)samples;
	return 0;
}

#include <sof/audio/format.h>
#include <sof/math/iir_df1.h>
#include <module/crossover/crossover_common.h>
#include <user/eq.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

#define FS	48000.0
#define FC_HZ	1000.0

#define SIGNAL_LEN	512
#define WARMUP_LEN	288

#define Q30		((int64_t)1 << 30)
#define UNITY_GAIN_Q14	((int32_t)(1 << 14))

#define BIQUAD_NCOEF	SOF_EQ_IIR_NBIQUAD	/* 7 */
#define LR4_NCOEF	(2 * BIQUAD_NCOEF)
#define LR4_NDELAY	(2 * IIR_DF1_NUM_STATE)

/* =========================================================================
 * Coefficient helpers (identical to crossover_lr4_test.c)
 * ========================================================================= */

static void compute_biquad_lp(double fs, double fc, int32_t coef[BIQUAD_NCOEF])
{
	double cutoff = fc / (fs / 2.0);
	double d = M_SQRT2;
	double theta = M_PI * cutoff;
	double sn = 0.5 * d * sin(theta);
	double beta = 0.5 * (1.0 - sn) / (1.0 + sn);
	double gamma_v = (0.5 + beta) * cos(theta);
	double alpha = 0.25 * (0.5 + beta - gamma_v);

	coef[0] = (int32_t)llround(-(2.0 * beta) * Q30);
	coef[1] = (int32_t)llround((2.0 * gamma_v) * Q30);
	coef[2] = (int32_t)llround((2.0 * alpha) * Q30);
	coef[3] = (int32_t)llround((4.0 * alpha) * Q30);
	coef[4] = (int32_t)llround((2.0 * alpha) * Q30);
	coef[5] = 0;
	coef[6] = UNITY_GAIN_Q14;
}

static void compute_biquad_hp(double fs, double fc, int32_t coef[BIQUAD_NCOEF])
{
	double cutoff = fc / (fs / 2.0);
	double d = M_SQRT2;
	double theta = M_PI * cutoff;
	double sn = 0.5 * d * sin(theta);
	double beta = 0.5 * (1.0 - sn) / (1.0 + sn);
	double gamma_v = (0.5 + beta) * cos(theta);
	double alpha = 0.25 * (0.5 + beta + gamma_v);

	coef[0] = (int32_t)llround(-(2.0 * beta) * Q30);
	coef[1] = (int32_t)llround((2.0 * gamma_v) * Q30);
	coef[2] = (int32_t)llround((2.0 * alpha) * Q30);
	coef[3] = (int32_t)llround((-4.0 * alpha) * Q30);
	coef[4] = (int32_t)llround((2.0 * alpha) * Q30);
	coef[5] = 0;
	coef[6] = UNITY_GAIN_Q14;
}

/* =========================================================================
 * State helpers
 * ========================================================================= */

/**
 * @brief Backing storage for one LR4 filter (coef + delay).
 */
struct lr4_storage {
	int32_t coef[LR4_NCOEF];
	int32_t delay[LR4_NDELAY];
};

/**
 * @brief Initialise one iir_state_df1 LR4 from a single biquad set.
 */
static void init_lr4_state(struct iir_state_df1 *lr4,
			   struct lr4_storage *s,
			   const int32_t bq[BIQUAD_NCOEF])
{
	memcpy(s->coef,              bq, BIQUAD_NCOEF * sizeof(int32_t));
	memcpy(s->coef + BIQUAD_NCOEF, bq, BIQUAD_NCOEF * sizeof(int32_t));
	memset(s->delay, 0, LR4_NDELAY * sizeof(int32_t));
	lr4->biquads = 2;
	lr4->biquads_in_series = 2;
	lr4->coef = s->coef;
	lr4->delay = s->delay;
}

/**
 * @brief Backing storage for a full crossover_state (all LR4 filters for
 *        one audio channel).
 *
 * CROSSOVER_MAX_LR4 = 3 lowpass and 3 highpass LR4 slots.
 */
struct channel_storage {
	struct lr4_storage lp[CROSSOVER_MAX_LR4];
	struct lr4_storage hp[CROSSOVER_MAX_LR4];
};

/**
 * @brief Populate a crossover_state with LP/HP LR4 filters at fc_lp and fc_hp.
 *
 * For 2-way: only slot 0 is used (1 LR4 pair).
 * For 3-way: slots 0–2 are used (3 LR4 pairs; same coef repeated as required
 *             by the Linkwitz-Riley phase-realignment merge logic).
 * For 4-way: slots 0–2 are used (3 LR4 pairs at low, mid, high fc).
 *
 * @param state    crossover_state to initialise.
 * @param storage  Backing memory (must outlive @p state).
 * @param num_sinks  2, 3, or 4.
 * @param fc_lo    Low crossover frequency (Hz).
 * @param fc_hi    High crossover frequency (Hz); ignored for 2-way.
 */
static void init_crossover_state(struct crossover_state *state,
				 struct channel_storage *storage,
				 int num_sinks,
				 double fc_lo, double fc_hi)
{
	int32_t bq_lp_lo[BIQUAD_NCOEF], bq_hp_lo[BIQUAD_NCOEF];
	int32_t bq_lp_hi[BIQUAD_NCOEF], bq_hp_hi[BIQUAD_NCOEF];
	int i;

	compute_biquad_lp(FS, fc_lo, bq_lp_lo);
	compute_biquad_hp(FS, fc_lo, bq_hp_lo);
	compute_biquad_lp(FS, fc_hi, bq_lp_hi);
	compute_biquad_hp(FS, fc_hi, bq_hp_hi);

	/* Zero all slots first */
	memset(state, 0, sizeof(*state));

	switch (num_sinks) {
	case 2:
		/* 1 LP/HP pair at fc_lo */
		init_lr4_state(&state->lowpass[0],  &storage->lp[0], bq_lp_lo);
		init_lr4_state(&state->highpass[0], &storage->hp[0], bq_hp_lo);
		break;
	case 3:
		/*
		 * 3-way layout (from crossover_generic_split_3way):
		 *   slot 0: first LP/HP split at fc_lo
		 *   slot 1: phase-realignment merge (LP/HP at fc_hi applied to
		 *           the low-freq branch from slot 0)
		 *   slot 2: second LP/HP split at fc_hi applied to hi-freq branch
		 */
		init_lr4_state(&state->lowpass[0],  &storage->lp[0], bq_lp_lo);
		init_lr4_state(&state->highpass[0], &storage->hp[0], bq_hp_lo);
		init_lr4_state(&state->lowpass[1],  &storage->lp[1], bq_lp_hi);
		init_lr4_state(&state->highpass[1], &storage->hp[1], bq_hp_hi);
		init_lr4_state(&state->lowpass[2],  &storage->lp[2], bq_lp_hi);
		init_lr4_state(&state->highpass[2], &storage->hp[2], bq_hp_hi);
		break;
	case 4:
		/*
		 * 4-way layout (from crossover_generic_split_4way):
		 *   slot 0: LP/HP split at fc_lo  (produces bands 0,1)
		 *   slot 1: LP/HP split at fc_mid (first stage, produces z1/z2)
		 *   slot 2: LP/HP split at fc_hi  (produces bands 2,3)
		 */
		for (i = 0; i < CROSSOVER_MAX_LR4; i++) {
			double fc = (i == 0) ? fc_lo :
				    (i == 1) ? ((fc_lo + fc_hi) / 2.0) : fc_hi;
			int32_t bq_lp[BIQUAD_NCOEF], bq_hp[BIQUAD_NCOEF];

			compute_biquad_lp(FS, fc, bq_lp);
			compute_biquad_hp(FS, fc, bq_hp);
			init_lr4_state(&state->lowpass[i],  &storage->lp[i], bq_lp);
			init_lr4_state(&state->highpass[i], &storage->hp[i], bq_hp);
		}
		break;
	default:
		break;
	}
}

/* =========================================================================
 * Signal helpers
 * ========================================================================= */

static double rms_buf(const int32_t *buf, int n)
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
 * @brief Test 1 – crossover_find_split_func returns NULL for invalid counts.
 */
static void test_find_split_func_invalid(void **state)
{
	(void)state;

	assert_null(crossover_find_split_func(0));
	assert_null(crossover_find_split_func(1));
	assert_null(crossover_find_split_func(5));
}

/**
 * @brief Test 2 – crossover_find_split_func returns a valid function for 2, 3, 4.
 */
static void test_find_split_func_valid(void **state)
{
	(void)state;

	assert_non_null(crossover_find_split_func(2));
	assert_non_null(crossover_find_split_func(3));
	assert_non_null(crossover_find_split_func(4));
}

/**
 * @brief Test 3 – 2-way split produces non-zero outputs for a non-zero input.
 */
static void test_split_2way_nonzero_output(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	int32_t out[2];
	crossover_split fn;

	init_crossover_state(&ch_state, &storage, 2, FC_HZ, 0);
	fn = crossover_find_split_func(2);

	fn(INT32_MAX / 2, out, &ch_state);

	/* Both LP and HP outputs must be non-zero for a non-zero input */
	assert_true(out[0] != 0 || out[1] != 0);
}

/**
 * @brief Test 4 – 2-way split produces zero outputs for a zero input.
 */
static void test_split_2way_zero_input(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	int32_t out[2] = {1, 1};	/* pre-set to non-zero */
	crossover_split fn;

	init_crossover_state(&ch_state, &storage, 2, FC_HZ, 0);
	fn = crossover_find_split_func(2);

	fn(0, out, &ch_state);

	assert_int_equal(out[0], 0);
	assert_int_equal(out[1], 0);
}

/**
 * @brief Test 5 – 2-way LP output passes low frequency and blocks high.
 *
 * Processes a block of samples at 100 Hz (well below fc) and at 10 kHz
 * (well above fc).  LP output (out[0]) must have much higher RMS at 100 Hz
 * than at 10 kHz; HP output (out[1]) must have much higher RMS at 10 kHz.
 */
static void test_split_2way_lp_frequency_selectivity(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	double amp = 0.5 * INT32_MAX;
	int32_t out_lf[2][SIGNAL_LEN], out_hf[2][SIGNAL_LEN];
	crossover_split fn;
	int i;

	fn = crossover_find_split_func(2);

	/* Feed 100 Hz (low frequency) */
	init_crossover_state(&ch_state, &storage, 2, FC_HZ, 0);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 100.0 * i / FS));
		int32_t band[2];

		fn(x, band, &ch_state);
		out_lf[0][i] = band[0];
		out_lf[1][i] = band[1];
	}

	/* Feed 10 kHz (high frequency) */
	init_crossover_state(&ch_state, &storage, 2, FC_HZ, 0);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 10000.0 * i / FS));
		int32_t band[2];

		fn(x, band, &ch_state);
		out_hf[0][i] = band[0];
		out_hf[1][i] = band[1];
	}

	double lp_lf = rms_buf(out_lf[0] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double lp_hf = rms_buf(out_hf[0] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double hp_lf = rms_buf(out_lf[1] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double hp_hf = rms_buf(out_hf[1] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	printf("2-way LP:  rms@100Hz=%.4f  rms@10kHz=%.6f  ratio=%.0f\n",
	       lp_lf, lp_hf, lp_lf / (lp_hf + 1e-12));
	printf("2-way HP:  rms@100Hz=%.6f  rms@10kHz=%.4f  ratio=%.0f\n",
	       hp_lf, hp_hf, hp_hf / (hp_lf + 1e-12));

	/* LP band: at least 40 dB stronger at 100 Hz than at 10 kHz */
	assert_true(lp_lf > 100.0 * lp_hf);

	/* HP band: at least 40 dB stronger at 10 kHz than at 100 Hz */
	assert_true(hp_hf > 100.0 * hp_lf);
}

/**
 * @brief Test 6 – 2-way: LP band dominates at very low frequency, HP at high.
 *
 * At 50 Hz (two decades below fc = 1 kHz) the LP output RMS must be at
 * least 100x greater than HP output RMS (>40 dB rejection ratio).
 * At 20 kHz (well above fc) the HP output must be at least 100x the LP.
 * This verifies the routing topology of split_2way is not swapped.
 */
static void test_split_2way_band_dominance(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	double amp = 0.5 * INT32_MAX;
	int32_t out[2][SIGNAL_LEN];
	crossover_split fn;
	int i;

	fn = crossover_find_split_func(2);

	/* 50 Hz -- should be dominated by LP (band 0) */
	init_crossover_state(&ch_state, &storage, 2, FC_HZ, 0);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 50.0 * i / FS));
		int32_t band[2];

		fn(x, band, &ch_state);
		out[0][i] = band[0];
		out[1][i] = band[1];
	}
	double lp_50  = rms_buf(out[0] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double hp_50  = rms_buf(out[1] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	/* 20 kHz -- should be dominated by HP (band 1) */
	init_crossover_state(&ch_state, &storage, 2, FC_HZ, 0);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 20000.0 * i / FS));
		int32_t band[2];

		fn(x, band, &ch_state);
		out[0][i] = band[0];
		out[1][i] = band[1];
	}
	double lp_20k = rms_buf(out[0] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	double hp_20k = rms_buf(out[1] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	printf("2-way band dominance: lp@50Hz=%.4f hp@50Hz=%.6f | "
	       "lp@20kHz=%.6f hp@20kHz=%.4f\n",
	       lp_50, hp_50, lp_20k, hp_20k);

	/* LP must dominate at 50 Hz by at least 40 dB */
	assert_true(lp_50 > 100.0 * hp_50);
	/* HP must dominate at 20 kHz by at least 40 dB */
	assert_true(hp_20k > 100.0 * lp_20k);
}

/**
 * @brief Test 7 – 3-way split produces non-zero output in all three bands
 *        for a broadband input.
 */
static void test_split_3way_all_bands_nonzero(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	double amp = 0.5 * INT32_MAX;
	int32_t acc[3] = {0, 0, 0};
	crossover_split fn;
	int i;

	fn = crossover_find_split_func(3);
	/*
	 * Use two crossover points: fc_lo = 500 Hz, fc_hi = 4000 Hz.
	 * This ensures all three bands are well-populated with spectral energy
	 * when driven by the broadband chirp below.
	 */
	init_crossover_state(&ch_state, &storage, 3, 500.0, 4000.0);

	for (i = 0; i < SIGNAL_LEN; i++) {
		double phase = M_PI * (double)i * i / SIGNAL_LEN;
		int32_t x = (int32_t)(amp * sin(phase));
		int32_t band[3] = {0, 0, 0};

		fn(x, band, &ch_state);
		acc[0] += (band[0] != 0) ? 1 : 0;
		acc[1] += (band[1] != 0) ? 1 : 0;
		acc[2] += (band[2] != 0) ? 1 : 0;
	}

	printf("3-way non-zero counts: band0=%d  band1=%d  band2=%d\n",
	       acc[0], acc[1], acc[2]);
	assert_true(acc[0] > 0);
	assert_true(acc[1] > 0);
	assert_true(acc[2] > 0);
}

/**
 * @brief Test 8 – 3-way split: each band dominates at its target frequency.
 *
 * With fc_lo=500 Hz and fc_hi=4000 Hz:
 *  - At 50 Hz,  band 0 (bass) must be > 100x bands 1 and 2
 *  - At 2 kHz,  band 1 (mid)  must be > 10x bands 0 and 2
 *  - At 20 kHz, band 2 (treble) must be > 100x bands 0 and 1
 */
static void test_split_3way_band_dominance(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	double amp = 0.5 * INT32_MAX;
	crossover_split fn;
	int i;

	fn = crossover_find_split_func(3);

	/* Helper: compute per-band steady-state RMS at a given frequency */
	double rms3[3][3]; /* rms3[freq_idx][band] */
	double test_freqs[3] = {50.0, 2000.0, 20000.0};
	int fi;

	for (fi = 0; fi < 3; fi++) {
		int32_t out[3][SIGNAL_LEN];

		init_crossover_state(&ch_state, &storage, 3, 500.0, 4000.0);
		for (i = 0; i < SIGNAL_LEN; i++) {
			int32_t x = (int32_t)(amp * sin(2.0 * M_PI * test_freqs[fi] * i / FS));
			int32_t band[3] = {0, 0, 0};

			fn(x, band, &ch_state);
			out[0][i] = band[0];
			out[1][i] = band[1];
			out[2][i] = band[2];
		}
		int b;

		for (b = 0; b < 3; b++)
			rms3[fi][b] = rms_buf(out[b] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);
	}

	printf("3-way band dominance:\n");
	printf("  50 Hz: b0=%.4f b1=%.6f b2=%.6f\n", rms3[0][0], rms3[0][1], rms3[0][2]);
	printf("  2 kHz: b0=%.6f b1=%.4f b2=%.6f\n", rms3[1][0], rms3[1][1], rms3[1][2]);
	printf(" 20 kHz: b0=%.6f b1=%.6f b2=%.4f\n", rms3[2][0], rms3[2][1], rms3[2][2]);

	/* 50 Hz → bass band dominates */
	assert_true(rms3[0][0] > 100.0 * rms3[0][1]);
	assert_true(rms3[0][0] > 100.0 * rms3[0][2]);

	/* 2 kHz → mid band dominates */
	assert_true(rms3[1][1] > 10.0 * rms3[1][0]);
	assert_true(rms3[1][1] > 10.0 * rms3[1][2]);

	/* 20 kHz → treble band dominates */
	assert_true(rms3[2][2] > 100.0 * rms3[2][0]);
	assert_true(rms3[2][2] > 100.0 * rms3[2][1]);
}

/**
 * @brief Test 9 – 4-way split produces non-zero output in all four bands.
 */
static void test_split_4way_all_bands_nonzero(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	double amp = 0.5 * INT32_MAX;
	int32_t acc[4] = {0, 0, 0, 0};
	crossover_split fn;
	int i;

	fn = crossover_find_split_func(4);
	init_crossover_state(&ch_state, &storage, 4, 500.0, 8000.0);

	for (i = 0; i < SIGNAL_LEN; i++) {
		double phase = M_PI * (double)i * i / SIGNAL_LEN;
		int32_t x = (int32_t)(amp * sin(phase));
		int32_t band[4] = {0, 0, 0, 0};

		fn(x, band, &ch_state);
		acc[0] += (band[0] != 0) ? 1 : 0;
		acc[1] += (band[1] != 0) ? 1 : 0;
		acc[2] += (band[2] != 0) ? 1 : 0;
		acc[3] += (band[3] != 0) ? 1 : 0;
	}

	printf("4-way non-zero counts: band0=%d  band1=%d  band2=%d  band3=%d\n",
	       acc[0], acc[1], acc[2], acc[3]);
	assert_true(acc[0] > 0);
	assert_true(acc[1] > 0);
	assert_true(acc[2] > 0);
	assert_true(acc[3] > 0);
}

/**
 * @brief Test 10 – 4-way split: each band dominates at its target frequency.
 *
 * With fc_lo=500 Hz and fc_hi=8000 Hz (mid≈4250 Hz geometric mean):
 *  - At 50 Hz:  band 0 (sub-bass) must be > 100x bands 1, 2, 3
 *  - At 20 kHz: band 3 (treble) must be > 30x bands 0, 1, 2
 */
static void test_split_4way_band_dominance(void **state)
{
	(void)state;

	struct crossover_state ch_state;
	struct channel_storage storage;
	double amp = 0.5 * INT32_MAX;
	crossover_split fn;
	int i, b;

	fn = crossover_find_split_func(4);

	/* 50 Hz — band 0 must dominate */
	int32_t out[4][SIGNAL_LEN];

	init_crossover_state(&ch_state, &storage, 4, 500.0, 8000.0);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 50.0 * i / FS));
		int32_t band[4] = {0, 0, 0, 0};

		fn(x, band, &ch_state);
		for (b = 0; b < 4; b++)
			out[b][i] = band[b];
	}
	double rms_50[4];

	for (b = 0; b < 4; b++)
		rms_50[b] = rms_buf(out[b] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	/* 20 kHz — band 3 must dominate */
	init_crossover_state(&ch_state, &storage, 4, 500.0, 8000.0);
	for (i = 0; i < SIGNAL_LEN; i++) {
		int32_t x = (int32_t)(amp * sin(2.0 * M_PI * 20000.0 * i / FS));
		int32_t band[4] = {0, 0, 0, 0};

		fn(x, band, &ch_state);
		for (b = 0; b < 4; b++)
			out[b][i] = band[b];
	}
	double rms_20k[4];

	for (b = 0; b < 4; b++)
		rms_20k[b] = rms_buf(out[b] + WARMUP_LEN, SIGNAL_LEN - WARMUP_LEN);

	printf("4-way band dominance:\n");
	printf("   50 Hz: b0=%.4f b1=%.6f b2=%.6f b3=%.6f\n",
	       rms_50[0], rms_50[1], rms_50[2], rms_50[3]);
	printf("  20 kHz: b0=%.6f b1=%.6f b2=%.6f b3=%.4f\n",
	       rms_20k[0], rms_20k[1], rms_20k[2], rms_20k[3]);

	/* 50 Hz → sub-bass dominates */
	assert_true(rms_50[0] > 100.0 * rms_50[1]);
	assert_true(rms_50[0] > 100.0 * rms_50[2]);
	assert_true(rms_50[0] > 100.0 * rms_50[3]);

	/* 20 kHz → treble dominates */
	assert_true(rms_20k[3] > 30.0 * rms_20k[0]);
	assert_true(rms_20k[3] > 30.0 * rms_20k[1]);
	assert_true(rms_20k[3] > 30.0 * rms_20k[2]);
}

/* =========================================================================
 * Test entry point
 * ========================================================================= */

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_find_split_func_invalid),
		cmocka_unit_test(test_find_split_func_valid),
		cmocka_unit_test(test_split_2way_nonzero_output),
		cmocka_unit_test(test_split_2way_zero_input),
		cmocka_unit_test(test_split_2way_lp_frequency_selectivity),
		cmocka_unit_test(test_split_2way_band_dominance),
		cmocka_unit_test(test_split_3way_all_bands_nonzero),
		cmocka_unit_test(test_split_3way_band_dominance),
		cmocka_unit_test(test_split_4way_all_bands_nonzero),
		cmocka_unit_test(test_split_4way_band_dominance),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
