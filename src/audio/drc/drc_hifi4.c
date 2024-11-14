// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/math/exp_fcn.h>
#include <sof/math/numbers.h>
#include <sof/common.h>
#include <stdint.h>

#include "drc.h"
#include "drc_algorithm.h"
#include "drc_math.h"

#if SOF_USE_MIN_HIFI(4, DRC)

#include <xtensa/tie/xt_hifi4.h>

#define ONE_Q20        1048576    /* Q_CONVERT_FLOAT(1.0f, 20) */
#define ONE_Q21        2097152    /* Q_CONVERT_FLOAT(1.0f, 21) */
#define ONE_Q30        1073741824 /* Q_CONVERT_FLOAT(1.0f, 30) */
#define TWELVE_Q21     25165824   /* Q_CONVERT_FLOAT(12.0f, 21) */
#define HALF_Q24       8388608    /* Q_CONVERT_FLOAT(0.5f, 24) */
#define NEG_TWO_DB_Q30 852903424  /* Q_CONVERT_FLOAT(0.7943282347242815f, 30) */
#define LSHIFT_QX31_QY20_QZ27 7   /*drc_get_lshift(31, 20, 27)*/
#define LSHIFT_QX24_QY20_QZ24 11  /*drc_get_lshift(24, 20, 24)*/
#define LSHIFT_QX24_QY20_QZ30 17  /*drc_get_lshift(24, 20, 30)*/
#define LSHIFT_QX30_QY20_QZ30 11  /*drc_get_lshift(30, 20, 30)*/
#define LSHIFT_QX30_QY30_QZ30 1   /*drc_get_lshift(30, 30, 30)*/
#define LSHIFT_QX26_QY30_QZ27 2   /*drc_get_lshift(26, 30, 27)*/
#define LSHIFT_QX21_QY30_QZ24 4   /*drc_get_lshift(21, 30, 24)*/
#define LSHIFT_QX21_QY21_QZ21 10  /*drc_get_lshift(21, 21, 21)*/
#define LSHIFT_QX30_QY16_QZ24 9   /*drc_get_lshift(31, 16, 24)*/
#define LSHIFT_QX15_QY24_QZ15 7  /*drc_get_lshift(15, 24, 15)*/
#define LSHIFT_QX31_QY24_QZ31 7   /*drc_get_lshift(31, 24, 31)*/

/* This is the knee part of the compression curve. Returns the output level
 * given the input level x.
 */
static inline void set_circular_buf0(void *buf, void *buf_end)
{
	AE_SETCBEGIN0(buf);
	AE_SETCEND0(buf_end);
}

static inline void set_circular_buf1(void *buf, void *buf_end)
{
	AE_SETCBEGIN1(buf);
	AE_SETCEND1(buf_end);
}

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
	gamma = drc_mult_lshift(x, -p->K, LSHIFT_QX31_QY20_QZ27);
	knee_exp_gamma = sofm_exp_fixed(gamma);
	knee_curve_k = drc_mult_lshift(p->knee_beta, knee_exp_gamma, LSHIFT_QX24_QY20_QZ24);
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
				    LSHIFT_QX24_QY20_QZ30);
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
		exp_knee = sofm_exp_fixed(drc_mult_lshift(tmp, tmp2, LSHIFT_QX26_QY30_QZ27));
		y = drc_mult_lshift(p->ratio_base, exp_knee, LSHIFT_QX30_QY20_QZ30);
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
	ae_int32 abs_input_array[DRC_DIVISION_FRAMES]; /* Q1.31 */
	ae_int32 *abs_input_array_p;
	int div_start, i, ch;
	ae_int16 *sample16_p; /* for s16 format case */
	ae_int32 *sample32_p; /* for s24 and s32 format cases */
	ae_int32x2 sample32;
	ae_int32x2 temp;
	ae_int16x4 sample16;
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
			sample16_p = (ae_int16 *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				AE_L16_XP(sample16, sample16_p, nbyte);
				sample32 = AE_CVT32X2F16_10(sample16);
				temp = AE_L32_I(abs_input_array_p, 0);
				sample32 = AE_MAXABS32S(sample32, temp);
				AE_S32_L_XP(sample32, abs_input_array_p, 4);
			}
		}
	} else { /* 4 bytes per sample */
		for (ch = 0; ch < nch; ch++) {
			abs_input_array_p = abs_input_array;
			sample32_p = (ae_int32 *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				AE_L32_XP(sample32, sample32_p, nbyte);
				temp = AE_L32_I(abs_input_array_p, 0);
				sample32 = AE_MAXABS32S(sample32, temp);
				AE_S32_L_XP(sample32, abs_input_array_p, nbyte);
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
						      LSHIFT_QX30_QY30_QZ30);
			} else {
				gain = AE_SRAI32R(gain, 4); /* Q2.30 -> Q6.26 */
				db_per_frame = drc_mult_lshift(drc_lin2db_fixed(gain),
							       p->sat_release_frames_inv_neg,
							       LSHIFT_QX21_QY30_QZ24);
				sat_release_rate = AE_SUB32(sofm_db2lin_fixed(db_per_frame),
							    ONE_Q20);
				tmp = drc_mult_lshift(gain_diff, sat_release_rate,
						      LSHIFT_QX30_QY20_QZ30);
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
		x2 = drc_mult_lshift(x, x, LSHIFT_QX21_QY21_QZ21); /* Q11.21 */
		x3 = drc_mult_lshift(x2, x, LSHIFT_QX21_QY21_QZ21); /* Q11.21 */
		x4 = drc_mult_lshift(x2, x2, LSHIFT_QX21_QY21_QZ21); /* Q11.21 */

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
		/* Q8.24 */
		db_per_frame = drc_mult_lshift(db_per_frame, tmp, LSHIFT_QX30_QY16_QZ24);
		envelope_rate = sofm_db2lin_fixed(db_per_frame); /* Q12.20 */
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
	const int count = DRC_DIVISION_FRAMES >> 2;
	ae_f32 x[4]; /* Q2.30 */
	ae_f32 c, base, r, r2, r4; /* Q2.30 */
	ae_f32 post_warp_compressor_gain;
	ae_f32 total_gain;
	int i, j, ch;
	int inc = state->pre_delay_read_index;
	int16_t *sample16_p; /* for s16 format case */
	int32_t *sample32_p; /* for s24 and s32 format cases */
	int32_t sample;
	int is_2byte = (nbyte == 2); /* otherwise is 4-bytes */
	ae_f32 tmp;
	ae_f64 tmp64;
	ae_f32 master_linear_gain = p->master_linear_gain;

	/* Exponential approach to desired gain. */
	if (state->envelope_rate < ONE_Q30) {
		/* Attack - reduce gain to desired. */
		c = AE_SUB32(state->compressor_gain, state->scaled_desired_gain);
		base = state->scaled_desired_gain;
		r = AE_SUB32(ONE_Q30, state->envelope_rate);
		x[0] = drc_mult_lshift(c, r, LSHIFT_QX30_QY30_QZ30);
		for (j = 1; j < 4; j++)
			x[j] = drc_mult_lshift(x[j - 1], r, LSHIFT_QX30_QY30_QZ30);
		r2 = drc_mult_lshift(r, r, LSHIFT_QX30_QY30_QZ30);
		r4 = drc_mult_lshift(r2, r2, LSHIFT_QX30_QY30_QZ30);

		i = 0;
		if (is_2byte) { /* 2 bytes per sample */
			while (1) {
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					tmp = AE_ADD32(x[j], base);
					post_warp_compressor_gain = drc_sin_fixed(tmp); /* Q1.31 */

					/* Calculate total gain using master gain. */
					tmp64 = AE_MULF32R_LL(master_linear_gain,
							      post_warp_compressor_gain);
					total_gain = AE_ROUND32F48SSYM(tmp64); /* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample16_p =
							(int16_t *)state->pre_delay_buffers[ch] +
							inc;
						sample = (int32_t)*sample16_p;
						sample = drc_mult_lshift(sample, total_gain,
									 LSHIFT_QX15_QY24_QZ15);
						*sample16_p = sat_int16(sample);
					}
					inc++;
				}

				if (++i == count)
					break;

				for (j = 0; j < 4; j++)
					x[j] = drc_mult_lshift(x[j], r4, LSHIFT_QX30_QY30_QZ30);
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
					tmp64 = AE_MULF32R_LL(master_linear_gain,
							      post_warp_compressor_gain);
					total_gain = AE_ROUND32F48SSYM(tmp64);/* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample32_p =
							(int32_t *)state->pre_delay_buffers[ch] +
							inc;
						sample = *sample32_p;
						sample = drc_mult_lshift(sample, total_gain,
									 LSHIFT_QX31_QY24_QZ31);
						*sample32_p = sample;
					}
					inc++;
				}

				if (++i == count)
					break;

				for (j = 0; j < 4; j++)
					x[j] = drc_mult_lshift(x[j], r4, LSHIFT_QX30_QY30_QZ30);
			}
		}

		tmp = AE_ADD32(x[3], base);
		state->compressor_gain = tmp;
	} else {
		/* Release - exponentially increase gain to 1.0 */
		c = state->compressor_gain;
		r = state->envelope_rate;
		x[0] = drc_mult_lshift(c, r, LSHIFT_QX30_QY30_QZ30);
		for (j = 1; j < 4; j++)
			x[j] = drc_mult_lshift(x[j - 1], r, LSHIFT_QX30_QY30_QZ30);
		r2 = drc_mult_lshift(r, r, LSHIFT_QX30_QY30_QZ30);
		r4 = drc_mult_lshift(r2, r2, LSHIFT_QX30_QY30_QZ30);

		i = 0;
		if (is_2byte) { /* 2 bytes per sample */
			while (1) {
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain = drc_sin_fixed(x[j]); /* Q1.31 */

					/* Calculate total gain using master gain. */
					tmp64 = AE_MULF32R_LL(master_linear_gain,
							      post_warp_compressor_gain);
					total_gain = AE_ROUND32F48SSYM(tmp64);/* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample16_p =
							(int16_t *)state->pre_delay_buffers[ch] +
							inc;
						sample = (int32_t)*sample16_p;
						sample = drc_mult_lshift(sample, total_gain,
									 LSHIFT_QX15_QY24_QZ15);
						*sample16_p = sat_int16(sample);
					}
					inc++;
				}

				if (++i == count)
					break;

				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, LSHIFT_QX30_QY30_QZ30);
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
					tmp64 = AE_MULF32R_LL(master_linear_gain,
							      post_warp_compressor_gain);
					total_gain = AE_ROUND32F48SSYM(tmp64);/* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample32_p =
							(int32_t *)state->pre_delay_buffers[ch] +
							inc;
						sample = *sample32_p;
						sample = drc_mult_lshift(sample, total_gain,
									 LSHIFT_QX31_QY24_QZ31);
						*sample32_p = sample;
					}
					inc++;
				}

				if (++i == count)
					break;

				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, LSHIFT_QX30_QY30_QZ30);
					x[j] = AE_MIN32(ONE_Q30, tmp);
				}
			}
		}

		state->compressor_gain = x[3];
	}
}

/* After one complete division of samples have been received (and one division of
 * samples have been output), we calculate shaped power average
 * (detector_average) from the input division, update envelope parameters from
 * detector_average, then prepare the next output division by applying the
 * envelope to compress the samples.
 */
static void drc_process_one_division(struct drc_state *state,
				     const struct sof_drc_params *p,
				     int nbyte,
				     int nch)
{
	drc_update_detector_average(state, p, nbyte, nch);
	drc_update_envelope(state, p);
	drc_compress_output(state, p, nbyte, nch);
}

void drc_default_pass(struct processing_module *mod,
		      const struct audio_stream *source,
		      struct audio_stream *sink, uint32_t frames)
{
	audio_stream_copy(source, 0, sink, 0, frames * audio_stream_get_channels(source));
}

static inline void drc_pre_delay_index_inc(int *idx, int increment)
{
	*idx = (*idx + increment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
}

#if CONFIG_FORMAT_S16LE
static void drc_delay_input_sample_s16(struct drc_state *state,
				       const struct audio_stream *source,
				       struct audio_stream *sink,
				       int16_t **x, int16_t **y, int samples)
{
	ae_int16 *x1;
	ae_int16 *y1;
	ae_int16 *pd;
	ae_int16 *pd_r;
	ae_int16 *pd_w;
	int nbuf, npcm, nfrm;
	int ch;
	int i;
	ae_int16 *x0 = (ae_int16 *)*x;
	ae_int16 *y0 = (ae_int16 *)*y;
	int remaining_samples = samples;
	int nch = audio_stream_get_channels(source);
	const int sample_inc = nch * sizeof(int16_t);
	const int delay_inc = sizeof(int16_t);
	ae_int16x4 sample;

	while (remaining_samples) {
		nbuf = audio_stream_samples_without_wrap_s16(source, x0);
		npcm = MIN(remaining_samples, nbuf);
		nbuf = audio_stream_samples_without_wrap_s16(sink, y0);
		npcm = MIN(npcm, nbuf);
		nfrm = npcm / nch;
		for (ch = 0; ch < nch; ++ch) {
			pd = (ae_int16 *)state->pre_delay_buffers[ch];
			set_circular_buf0(pd, pd + CONFIG_DRC_MAX_PRE_DELAY_FRAMES);
			x1 = x0 + ch;
			y1 = y0 + ch;
			pd_r = pd + state->pre_delay_read_index;
			pd_w = pd + state->pre_delay_write_index;
			for (i = 0; i < nfrm; i++) {
				AE_L16_XP(sample, x1, sample_inc);
				AE_S16_0_XC(sample, pd_w, delay_inc);
				/*pop sample from delay buffer and store in output buffer*/
				AE_L16_XC(sample, pd_r, delay_inc);
				AE_S16_0_XP(sample, y1, sample_inc);
			}
		}
		remaining_samples -= npcm;
		x0 = audio_stream_wrap(source, x0 + npcm);
		y0 = audio_stream_wrap(sink, y0 + npcm);
		drc_pre_delay_index_inc(&state->pre_delay_write_index, nfrm);
		drc_pre_delay_index_inc(&state->pre_delay_read_index, nfrm);
	}

	*x = (int16_t *)x0;
	*y = (int16_t *)y0;
}

static void drc_s16_default(struct processing_module *mod,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int16_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int nch = audio_stream_get_channels(source);
	int samples = frames * nch;
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct drc_state *state = &cd->state;
	const struct sof_drc_params *p = &cd->config->params; /* Read-only */
	int fragment;
	ae_int16 *pd;
	ae_int16 *pd_r;
	ae_int16 *pd_w;
	ae_int16 *x1;
	ae_int16 *y1;
	int i, ch;
	ae_int16x4 sample;
	const int sample_inc = nch * sizeof(ae_int16);
	const int delay_inc = sizeof(ae_int16);

	if (!cd->enabled) {
		/* Delay the input sample only and don't do other processing. This is used when the
		 * DRC is disabled. We want to do this to match the processing delay of other bands
		 * in multi-band DRC kernel case.
		 */
		drc_delay_input_sample_s16(state, source, sink, &x, &y, samples);
		return;
	}

	if (!state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, sizeof(int16_t), nch);
		state->processed = 1;
	}

	set_circular_buf0(source->addr, source->end_addr);
	set_circular_buf1(sink->addr, sink->end_addr);

	while (frames) {
		fragment = DRC_DIVISION_FRAMES -
			(state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK);
		fragment = MIN(frames, fragment);

		for (ch = 0; ch < nch; ++ch) {
			pd = (ae_int16 *)state->pre_delay_buffers[ch];
			x1 = (ae_int16 *)x + ch;
			y1 = (ae_int16 *)y + ch;
			pd_r = pd + state->pre_delay_read_index;
			pd_w = pd + state->pre_delay_write_index;

			/* don't need to check the boundary of pre-delay because the
			 * state->pre_delay_write_index + frames will always be aligned with
			 * 32(DRC_DIVISION_FRAMES), and the pre-delay buffer size (a multiple
			 * of CONFIG_DRC_MAX_PRE_DELAY_FRAMES) will always be the multiple of
			 * DRC_DIVISION_FRAMES
			 */
			for (i = 0; i < fragment; i++) {
				AE_L16_XC(sample, x1, sample_inc);
				AE_S16_0_XP(sample, pd_w, delay_inc);
				/*pop sample from delay buffer and store in output buffer*/
				AE_L16_XP(sample, pd_r, delay_inc);
				AE_S16_0_XC1(sample, y1, sample_inc);
			}
		}
		drc_pre_delay_index_inc(&state->pre_delay_write_index, fragment);
		drc_pre_delay_index_inc(&state->pre_delay_read_index, fragment);
		x = audio_stream_wrap(source, x + fragment * nch);
		y = audio_stream_wrap(sink, y + fragment * nch);
		frames -= fragment;

		/* Process the input division (32 frames). */
		if ((state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK) == 0)
			drc_process_one_division(state, p, sizeof(int16_t), nch);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void drc_delay_input_sample_s32(struct drc_state *state,
				       const struct audio_stream *source,
				       struct audio_stream *sink,
				       int32_t **x, int32_t **y, int samples)
{
	ae_int32 *x1;
	ae_int32 *y1;
	ae_int32 *pd_r;
	ae_int32 *pd_w;
	ae_int32 *pd;
	int nbuf, npcm, nfrm;
	int ch;
	int i;
	ae_int32 *x0 = (ae_int32 *)*x;
	ae_int32 *y0 = (ae_int32 *)*y;
	ae_int32x2 sample;

	int remaining_samples = samples;
	int nch = audio_stream_get_channels(source);
	const int sample_inc = nch * sizeof(int32_t);
	const int delay_inc = sizeof(int32_t);

	while (remaining_samples) {
		nbuf = audio_stream_samples_without_wrap_s32(source, x0);
		npcm = MIN(remaining_samples, nbuf);
		nbuf = audio_stream_samples_without_wrap_s32(sink, y0);
		npcm = MIN(npcm, nbuf);
		nfrm = npcm / nch;
		for (ch = 0; ch < nch; ++ch) {
			pd = (ae_int32 *)state->pre_delay_buffers[ch];
			set_circular_buf0(pd, pd + CONFIG_DRC_MAX_PRE_DELAY_FRAMES);
			x1 = x0 + ch;
			y1 = y0 + ch;
			pd_r = pd + state->pre_delay_read_index;
			pd_w = pd + state->pre_delay_write_index;

			for (i = 0; i < nfrm; i++) {
				/*store the input sample to delay buffer*/
				AE_L32_XP(sample, x1, sample_inc);
				AE_S32_L_XC(sample, pd_w, delay_inc);

				/*pop sample from delay buffer and store in output buffer*/
				AE_L32_XC(sample, pd_r, delay_inc);
				AE_S32_L_XP(sample, y1, sample_inc);
			}
		}
		remaining_samples -= npcm;
		x0 = audio_stream_wrap(source, x0 + npcm);
		y0 = audio_stream_wrap(sink, y0 + npcm);
		drc_pre_delay_index_inc(&state->pre_delay_write_index, nfrm);
		drc_pre_delay_index_inc(&state->pre_delay_read_index, nfrm);
	}

	*x = (int32_t *)x0;
	*y = (int32_t *)y0;
}
#endif

#if CONFIG_FORMAT_S24LE

static void drc_s24_default(struct processing_module *mod,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nch = audio_stream_get_channels(source);
	int samples = frames * nch;
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct drc_state *state = &cd->state;
	const struct sof_drc_params *p = &cd->config->params; /* Read-only */
	int fragment;
	ae_int32 *pd;
	ae_int32 *pd_r;
	ae_int32 *pd_w;
	ae_int32 *x1;
	ae_int32 *y1;
	int i, ch;
	ae_int32x2 sample;
	const int sample_inc = nch * sizeof(int32_t);
	const int delay_inc = sizeof(int32_t);

	if (!cd->enabled) {
		/* Delay the input sample only and don't do other processing. This is used when the
		 * DRC is disabled. We want to do this to match the processing delay of other bands
		 * in multi-band DRC kernel case. Note: use 32 bit delay function.
		 */
		drc_delay_input_sample_s32(state, source, sink, &x, &y, samples);
		return;
	}

	if (!state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, sizeof(int32_t), nch);
		state->processed = 1;
	}

	set_circular_buf0(source->addr, source->end_addr);
	set_circular_buf1(sink->addr, sink->end_addr);

	while (frames) {
		fragment = DRC_DIVISION_FRAMES -
			(state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK);
		fragment = MIN(frames, fragment);

		for (ch = 0; ch < nch; ++ch) {
			pd = (ae_int32 *)state->pre_delay_buffers[ch];
			x1 = (ae_int32 *)x + ch;
			y1 = (ae_int32 *)y + ch;
			pd_r = pd + state->pre_delay_read_index;
			pd_w = pd + state->pre_delay_write_index;

			/* don't need to check the boundary of pre-delay because the
			 * state->pre_delay_write_index + frames will always be aligned with
			 * 32(DRC_DIVISION_FRAMES), and the pre-delay buffer size (a multiple
			 * of CONFIG_DRC_MAX_PRE_DELAY_FRAMES) will always be the multiple of
			 * DRC_DIVISION_FRAMES
			 */
			for (i = 0; i < fragment; i++) {
				/*store the input sample to delay buffer*/
				AE_L32_XC(sample, x1, sample_inc);
				sample = AE_SLAI32(sample, 8); /*Q9.23 -> Q1.31*/
				AE_S32_L_XP(sample, pd_w, delay_inc);

				/*pop sample from delay buffer and store in output buffer*/
				AE_L32_XP(sample, pd_r, delay_inc);
				sample = AE_SRAI32R(sample, 8);
				sample = AE_SLAA32S(sample, 8);
				sample = AE_SRAI32(sample, 8);

				AE_S32_L_XC1(sample, y1, sample_inc);
			}
		}
		drc_pre_delay_index_inc(&state->pre_delay_write_index, fragment);
		drc_pre_delay_index_inc(&state->pre_delay_read_index, fragment);
		x = audio_stream_wrap(source, x + fragment * nch);
		y = audio_stream_wrap(sink, y + fragment * nch);
		frames -= fragment;

		/* Process the input division (32 frames). */
		if ((state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK) == 0)
			drc_process_one_division(state, p, sizeof(int32_t), nch);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void drc_s32_default(struct processing_module *mod,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nch = audio_stream_get_channels(source);
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct drc_state *state = &cd->state;
	const struct sof_drc_params *p = &cd->config->params; /* Read-only */
	int fragment;
	ae_int32 *pd;
	ae_int32 *pd_r;
	ae_int32 *pd_w;
	ae_int32 *x1;
	ae_int32 *y1;
	int i, ch;
	ae_int32x2 sample;
	const int sample_inc = nch * sizeof(int32_t);
	const int delay_inc = sizeof(int32_t);

	if (!cd->enabled) {
		/* Delay the input sample only and don't do other processing. This is used when the
		 * DRC is disabled. We want to do this to match the processing delay of other bands
		 * in multi-band DRC kernel case.
		 */
		drc_delay_input_sample_s32(state, source, sink, &x, &y, frames * nch);
		return;
	}

	if (!state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, sizeof(int32_t), nch);
		state->processed = 1;
	}

	set_circular_buf0(source->addr, source->end_addr);
	set_circular_buf1(sink->addr, sink->end_addr);

	while (frames) {
		fragment = DRC_DIVISION_FRAMES -
			(state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK);
		fragment = MIN(frames, fragment);

		for (ch = 0; ch < nch; ++ch) {
			pd = (ae_int32 *)state->pre_delay_buffers[ch];
			x1 = (ae_int32 *)x + ch;
			y1 = (ae_int32 *)y + ch;
			pd_r = pd + state->pre_delay_read_index;
			pd_w = pd + state->pre_delay_write_index;

			/* don't need to check the boundary of pre-delay because the
			 * state->pre_delay_write_index + frames will always be aligned with
			 * 32(DRC_DIVISION_FRAMES), and the pre-delay buffer size (a multiple
			 * of CONFIG_DRC_MAX_PRE_DELAY_FRAMES) will always be the multiple of
			 * DRC_DIVISION_FRAMES
			 */
			for (i = 0; i < fragment; i++) {
				/*store the input sample to delay buffer*/
				AE_L32_XC(sample, x1, sample_inc);
				AE_S32_L_XP(sample, pd_w, delay_inc);

				/*pop sample from delay buffer and store in output buffer*/
				AE_L32_XP(sample, pd_r, delay_inc);
				AE_S32_L_XC1(sample, y1, sample_inc);
			}
		}
		drc_pre_delay_index_inc(&state->pre_delay_write_index, fragment);
		drc_pre_delay_index_inc(&state->pre_delay_read_index, fragment);
		x = audio_stream_wrap(source, x + fragment * nch);
		y = audio_stream_wrap(sink, y + fragment * nch);
		frames -= fragment;

		/* Process the input division (32 frames). */
		if ((state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK) == 0)
			drc_process_one_division(state, p, sizeof(int32_t), nch);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct drc_proc_fnmap drc_proc_fnmap[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, drc_s16_default },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, drc_s24_default },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, drc_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t drc_proc_fncount = ARRAY_SIZE(drc_proc_fnmap);

#endif /* DRC_HIFI_4 */
