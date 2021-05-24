// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/component.h>
#include <sof/audio/drc/drc.h>
#include <sof/audio/drc/drc_algorithm.h>
#include <sof/audio/drc/drc_math.h>
#include <sof/audio/format.h>
#include <sof/math/decibels.h>
#include <sof/math/numbers.h>
#include <stdint.h>

#if DRC_HIFI3

#include <xtensa/tie/xt_hifi3.h>

#define ONE_Q20        1048576    /* Q_CONVERT_FLOAT(1.0f, 20) */
#define ONE_Q21        2097152    /* Q_CONVERT_FLOAT(1.0f, 21) */
#define ONE_Q30        1073741824 /* Q_CONVERT_FLOAT(1.0f, 30) */
#define TWELVE_Q21     25165824   /* Q_CONVERT_FLOAT(12.0f, 21) */
#define HALF_Q24       8388608    /* Q_CONVERT_FLOAT(0.5f, 24) */
#define NEG_TWO_DB_Q30 852903424  /* Q_CONVERT_FLOAT(0.7943282347242815f, 30) */

/* This is the knee part of the compression curve. Returns the output level
 * given the input level x.
 */
static int32_t knee_curveK(const struct sof_drc_params *p, int32_t x)
{
	ae_f32 gamma; /* Q5.27 */
	ae_f32 knee_exp_gamma; /* Q12.20 */
	ae_f32 knee_curve_k; /* Q8.24 */

	/* The formula in knee_curveK is linear_threshold +
	 * (1 - expf(-k * (x - linear_threshold))) / k
	 * which simplifies to (alpha + beta * expf(gamma))
	 * where alpha = linear_threshold + 1 / k
	 *	 beta = -expf(k * linear_threshold) / k
	 *	 gamma = -k * x
	 */
	gamma = drc_mult_lshift(x, -p->K, drc_get_lshift(31, 20, 27));
	knee_exp_gamma = exp_fixed(gamma);
	knee_curve_k = drc_mult_lshift(p->knee_beta, knee_exp_gamma, drc_get_lshift(24, 20, 24));
	knee_curve_k = AE_ADD32(knee_curve_k, p->knee_alpha);
	return knee_curve_k;
}

/* Full compression curve with constant ratio after knee. Returns the ratio of
 * output and input signal.
 */
static int32_t volume_gain(const struct sof_drc_params *p, int32_t x)
{
	const ae_f32 knee_threshold = AE_SLAI32S(p->knee_threshold, 7); /* Q8.24 -> Q1.31 */
	const ae_f32 linear_threshold = AE_SLAI32S(p->linear_threshold, 1); /* Q2.30 -> Q1.31 */
	ae_f32 exp_knee; /* Q12.20 */
	ae_f32 y; /* Q2.30 */
	ae_f32 tmp;
	ae_f32 tmp2;

	if (x < (int32_t)knee_threshold) {
		if (x < (int32_t)linear_threshold)
			return ONE_Q30;
		/* y = knee_curveK(x) / x */
		y = drc_mult_lshift(knee_curveK(p, x), drc_inv_fixed(x, 31, 20),
				    drc_get_lshift(24, 20, 30));
	} else {
		/* Constant ratio after knee.
		 * log(y/y0) = s * log(x/x0)
		 * => y = y0 * (x/x0)^s
		 * => y = [y0 * (1/x0)^s] * x^s
		 * => y = ratio_base * x^s
		 * => y/x = ratio_base * x^(s - 1)
		 * => y/x = ratio_base * e^(log(x) * (s - 1))
		 */
		tmp = AE_SRAI32R(x, 5); /* Q1.31 -> Q5.26 */
		tmp = drc_log_fixed(tmp); /* Q6.26 */
		tmp2 = AE_SUB32(p->slope, ONE_Q30); /* Q2.30 */
		exp_knee = exp_fixed(drc_mult_lshift(tmp, tmp2, drc_get_lshift(26, 30, 27)));
		y = drc_mult_lshift(p->ratio_base, exp_knee, drc_get_lshift(30, 20, 30));
	}

	return y;
}

/* Update detector_average from the last input division. */
void drc_update_detector_average(struct drc_state *state,
				 const struct sof_drc_params *p,
				 int nbyte,
				 int nch)
{
	ae_f32 detector_average = state->detector_average; /* Q2.30 */
	int32_t abs_input_array[DRC_DIVISION_FRAMES]; /* Q1.31 */
	int32_t *abs_input_array_p;
	int div_start, i, ch;
	int16_t *sample16_p; /* for s16 format case */
	int32_t *sample32_p; /* for s24 and s32 format cases */
	int32_t sample;
	ae_f32 gain;
	ae_f32 gain_diff;
	ae_f32 db_per_frame;
	ae_f32 sat_release_rate;
	ae_f32 tmp;
	int is_release;

	/* Calculate the start index of the last input division */
	if (state->pre_delay_write_index == 0)
		div_start = CONFIG_DRC_MAX_PRE_DELAY_FRAMES - DRC_DIVISION_FRAMES;
	else
		div_start = state->pre_delay_write_index - DRC_DIVISION_FRAMES;

	/* The max abs value across all channels for this frame */
	memset(abs_input_array, 0, DRC_DIVISION_FRAMES * sizeof(int32_t));
	if (nbyte == 2) { /* 2 bytes per sample */
		for (ch = 0; ch < nch; ch++) {
			abs_input_array_p = abs_input_array;
			sample16_p = (int16_t *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				sample = (int32_t)*sample16_p << 16;
				*abs_input_array_p = MAX(*abs_input_array_p, ABS(sample));
				abs_input_array_p++;
				sample16_p++;
			}
		}
	} else { /* 4 bytes per sample */
		for (ch = 0; ch < nch; ch++) {
			abs_input_array_p = abs_input_array;
			sample32_p = (int32_t *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				sample = *sample32_p;
				*abs_input_array_p = MAX(*abs_input_array_p, ABS(sample));
				abs_input_array_p++;
				sample32_p++;
			}
		}
	}

	for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
		/* Compute compression amount from un-delayed signal */

		/* Calculate shaped power on undelayed input.  Put through
		 * shaping curve. This is linear up to the threshold, then
		 * enters a "knee" portion followed by the "ratio" portion. The
		 * transition from the threshold to the knee is smooth (1st
		 * derivative matched). The transition from the knee to the
		 * ratio portion is smooth (1st derivative matched).
		 */
		gain = volume_gain(p, abs_input_array[i]); /* Q2.30 */
		gain_diff = AE_SUB32(gain, detector_average); /* Q2.30 */
		is_release = ((int32_t)gain_diff > 0);
		if (is_release) {
			if ((int32_t)gain > NEG_TWO_DB_Q30) {
				tmp = drc_mult_lshift(gain_diff, p->sat_release_rate_at_neg_two_db,
						      drc_get_lshift(30, 30, 30));
			} else {
				gain = AE_SRAI32R(gain, 4); /* Q2.30 -> Q6.26 */
				db_per_frame = drc_mult_lshift(drc_lin2db_fixed(gain),
							       p->sat_release_frames_inv_neg,
							       drc_get_lshift(21, 30, 24));
				sat_release_rate = AE_SUB32(db2lin_fixed(db_per_frame), ONE_Q20);
				tmp = drc_mult_lshift(gain_diff, sat_release_rate,
						      drc_get_lshift(30, 20, 30));
			}
			detector_average = AE_ADD32(detector_average, tmp);
		} else {
			detector_average = gain;
		}

		detector_average = AE_MIN32(detector_average, ONE_Q30);
	}

	state->detector_average = detector_average;
}

/* Updates the envelope_rate used for the next division */
void drc_update_envelope(struct drc_state *state, const struct sof_drc_params *p)
{
	/* Deal with envelopes */

	/* envelope_rate is the rate we slew from current compressor level to
	 * the desired level.  The exact rate depends on if we're attacking or
	 * releasing and by how much.
	 */
	ae_f32 envelope_rate;

	/* compression_diff_db is the difference between current compression
	 * level and the desired level.
	 */
	ae_f32 compression_diff_db;
	ae_f32 x, x2, x3, x4;
	ae_f64 release_frames_f64;
	ae_f32 release_frames;
	ae_f32 db_per_frame;
	ae_f32 tmp;
	ae_f32 tmp2;
	int32_t scaled_desired_gain;
	int32_t eff_atten_diff_db;
	int32_t lshift;
	int32_t is_releasing;
	int32_t is_bad_db;

	/* Calculate desired gain */

	/* Pre-warp so we get desired_gain after sin() warp below. */
	scaled_desired_gain = drc_asin_fixed(state->detector_average); /* Q2.30 */

	is_releasing = scaled_desired_gain > state->compressor_gain;
	is_bad_db = (state->compressor_gain == 0 || scaled_desired_gain == 0);

	tmp = AE_SRAI32R(state->compressor_gain, 4); /* Q2.30 -> Q6.26 */
	tmp2 = AE_SRAI32R(scaled_desired_gain, 4); /* Q2.30 -> Q6.26 */
	compression_diff_db = AE_SUB32(drc_lin2db_fixed(tmp), drc_lin2db_fixed(tmp2)); /* Q11.21 */

	if (is_releasing) {
		/* Release mode - compression_diff_db should be negative dB */
		state->max_attack_compression_diff_db = INT32_MIN;

		/* Fix gremlins. */
		if (is_bad_db)
			compression_diff_db = -ONE_Q21;

		/* Adaptive release - higher compression (lower
		 * compression_diff_db) releases faster. Contain within range:
		 * -12 -> 0 then scale to go from 0 -> 3
		 */
		x = compression_diff_db; /* Q11.21 */
		x = AE_MAX32(-TWELVE_Q21, x);
		x = AE_MIN32(0, x);
		/* x = 0.25f * (x + 12) */
		x = AE_SRAI32R(AE_ADD32(x, TWELVE_Q21), 2); /* Q11.21 -> Q13.19 */

		/* Compute adaptive release curve using 4th order polynomial.
		 * Normal values for the polynomial coefficients would create a
		 * monotonically increasing function.
		 */
		lshift = drc_get_lshift(21, 21, 21);
		x2 = drc_mult_lshift(x, x, lshift); /* Q11.21 */
		x3 = drc_mult_lshift(x2, x, lshift); /* Q11.21 */
		x4 = drc_mult_lshift(x2, x2, lshift); /* Q11.21 */

		release_frames_f64 = AE_CVT48A32(p->kA); /* Q20.12 -> Q36.28 */
		release_frames_f64 = AE_SRAI64(release_frames_f64, 10); /* Q36.28 -> Q46.18 */
		AE_MULAF32R_LL(release_frames_f64, p->kB, x); /* Q20.12 * Q11.21 = Q46.18 */
		AE_MULAF32R_LL(release_frames_f64, p->kC, x2);
		AE_MULAF32R_LL(release_frames_f64, p->kD, x3);
		AE_MULAF32R_LL(release_frames_f64, p->kE, x4);
		release_frames_f64 = AE_SLAI64S(release_frames_f64, 10); /* Q46.18 -> Q36.28 */
		release_frames = AE_ROUND32F48SSYM(release_frames_f64); /* Q36.28 -> Q20.12 */

		/* db_per_frame = kSpacingDb / release_frames */
		db_per_frame = drc_inv_fixed(release_frames, 12, 30); /* Q2.30 */
		tmp = p->kSpacingDb << 16; /* Q16.16 */
		lshift = drc_get_lshift(30, 16, 24);
		db_per_frame = drc_mult_lshift(db_per_frame, tmp, lshift); /* Q8.24 */
		envelope_rate = db2lin_fixed(db_per_frame); /* Q12.20 */
	} else {
		/* Attack mode - compression_diff_db should be positive dB */

		/* Fix gremlins. */
		if (is_bad_db)
			compression_diff_db = ONE_Q21;

		/* As long as we're still in attack mode, use a rate based off
		 * the largest compression_diff_db we've encountered so far.
		 */
		tmp = AE_SLAI32S(compression_diff_db, 3); /* Q11.21 -> Q8.24 */
		state->max_attack_compression_diff_db =
			AE_MAX32(state->max_attack_compression_diff_db, tmp);

		eff_atten_diff_db =
			MAX(HALF_Q24, state->max_attack_compression_diff_db); /* Q8.24 */

		/* x = 0.25f / eff_atten_diff_db;
		 * => x = 1.0f / (eff_atten_diff_db << 2);
		 */
		x = drc_inv_fixed(eff_atten_diff_db, 22 /* Q8.24 << 2 */, 26); /* Q6.26 */
		envelope_rate = AE_SUB32(ONE_Q20,
					 drc_pow_fixed(x, p->one_over_attack_frames)); /* Q12.20 */
	}

	tmp = AE_SLAI32S(envelope_rate, 10); /* Q12.20 -> Q2.30 */
	state->envelope_rate = tmp;
	state->scaled_desired_gain = scaled_desired_gain;
}

/* Calculate compress_gain from the envelope and apply total_gain to compress
 * the next output division.
 */
void drc_compress_output(struct drc_state *state,
			 const struct sof_drc_params *p,
			 int nbyte,
			 int nch)
{
	const int div_start = state->pre_delay_read_index;
	const int count = DRC_DIVISION_FRAMES >> 2;

	ae_f32 x[4]; /* Q2.30 */
	ae_f32 c, base, r, r2, r4; /* Q2.30 */
	ae_f32 post_warp_compressor_gain;
	ae_f32 total_gain;
	ae_f32 tmp;

	int i, j, ch, inc;
	int16_t *sample16_p; /* for s16 format case */
	int32_t *sample32_p; /* for s24 and s32 format cases */
	int32_t sample;
	int32_t lshift;
	int is_2byte = (nbyte == 2); /* otherwise is 4-bytes */

	/* Exponential approach to desired gain. */
	if (state->envelope_rate < ONE_Q30) {
		/* Attack - reduce gain to desired. */
		c = AE_SUB32(state->compressor_gain, state->scaled_desired_gain);
		base = state->scaled_desired_gain;
		r = AE_SUB32(ONE_Q30, state->envelope_rate);
		lshift = drc_get_lshift(30, 30, 30);
		x[0] = drc_mult_lshift(c, r, lshift);
		for (j = 1; j < 4; j++)
			x[j] = drc_mult_lshift(x[j - 1], r, lshift);
		r2 = drc_mult_lshift(r, r, lshift);
		r4 = drc_mult_lshift(r2, r2, lshift);

		i = 0;
		inc = 0;
		if (is_2byte) { /* 2 bytes per sample */
			while (1) {
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					tmp = AE_ADD32(x[j], base);
					post_warp_compressor_gain = drc_sin_fixed(tmp); /* Q1.31 */

					/* Calculate total gain using master gain. */
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift); /* Q8.24 */

					/* Apply final gain. */
					lshift = drc_get_lshift(15, 24, 15);
					for (ch = 0; ch < nch; ch++) {
						sample16_p =
							(int16_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = (int32_t)*sample16_p;
						sample = drc_mult_lshift(sample, total_gain,
									 lshift);
						*sample16_p = sat_int16(sample);
					}
					inc++;
				}

				if (++i == count)
					break;

				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++)
					x[j] = drc_mult_lshift(x[j], r4, lshift);
			}
		} else { /* 4 bytes per sample */
			while (1) {
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					tmp = AE_ADD32(x[j], base);
					post_warp_compressor_gain = drc_sin_fixed(tmp); /* Q1.31 */

					/* Calculate total gain using master gain. */
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift); /* Q8.24 */

					/* Apply final gain. */
					lshift = drc_get_lshift(31, 24, 31);
					for (ch = 0; ch < nch; ch++) {
						sample32_p =
							(int32_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = *sample32_p;
						sample = drc_mult_lshift(sample, total_gain,
									 lshift);
						*sample32_p = sample;
					}
					inc++;
				}

				if (++i == count)
					break;

				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++)
					x[j] = drc_mult_lshift(x[j], r4, lshift);
			}
		}

		tmp = AE_ADD32(x[3], base);
		state->compressor_gain = tmp;
	} else {
		/* Release - exponentially increase gain to 1.0 */
		c = state->compressor_gain;
		r = state->envelope_rate;
		lshift = drc_get_lshift(30, 30, 30);
		x[0] = drc_mult_lshift(c, r, lshift);
		for (j = 1; j < 4; j++)
			x[j] = drc_mult_lshift(x[j - 1], r, lshift);
		r2 = drc_mult_lshift(r, r, lshift);
		r4 = drc_mult_lshift(r2, r2, lshift);

		i = 0;
		inc = 0;
		if (is_2byte) { /* 2 bytes per sample */
			while (1) {
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain = drc_sin_fixed(x[j]); /* Q1.31 */

					/* Calculate total gain using master gain. */
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift); /* Q8.24 */

					/* Apply final gain. */
					lshift = drc_get_lshift(15, 24, 15);
					for (ch = 0; ch < nch; ch++) {
						sample16_p =
							(int16_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = (int32_t)*sample16_p;
						sample = drc_mult_lshift(sample, total_gain,
									 lshift);
						*sample16_p = sat_int16(sample);
					}
					inc++;
				}

				if (++i == count)
					break;

				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, lshift);
					x[j] = AE_MIN32(ONE_Q30, tmp);
				}
			}
		} else { /* 4 bytes per sample */
			while (1) {
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain = drc_sin_fixed(x[j]); /* Q1.31 */

					/* Calculate total gain using master gain. */
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift); /* Q8.24 */

					/* Apply final gain. */
					lshift = drc_get_lshift(31, 24, 31);
					for (ch = 0; ch < nch; ch++) {
						sample32_p =
							(int32_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = *sample32_p;
						sample = drc_mult_lshift(sample, total_gain,
									 lshift);
						*sample32_p = sample;
					}
					inc++;
				}

				if (++i == count)
					break;

				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, lshift);
					x[j] = AE_MIN32(ONE_Q30, tmp);
				}
			}
		}

		state->compressor_gain = x[3];
	}
}

#endif /* DRC_HIFI3 */
