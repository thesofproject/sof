// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/math/exp_fcn.h>
#include <sof/math/numbers.h>
#include <sof/common.h>
#include <stdint.h>

#include "drc.h"
#include "drc_algorithm.h"
#include "drc_math.h"

#if SOF_USE_RISCV_SIMD(DRC)

#define ONE_Q20        1048576    /* Q_CONVERT_FLOAT(1.0f, 20) */
#define ONE_Q21        2097152    /* Q_CONVERT_FLOAT(1.0f, 21) */
#define ONE_Q30        1073741824 /* Q_CONVERT_FLOAT(1.0f, 30) */
#define TWELVE_Q21     25165824   /* Q_CONVERT_FLOAT(12.0f, 21) */
#define HALF_Q24       8388608    /* Q_CONVERT_FLOAT(0.5f, 24) */
#define NEG_TWO_DB_Q30 852903424  /* Q_CONVERT_FLOAT(0.7943282347242815f, 30) */

static inline int32_t sat_clamp64(int64_t x)
{
	if (x > INT32_MAX)
		return INT32_MAX;
	if (x < INT32_MIN)
		return INT32_MIN;
	return (int32_t)x;
}

/* Knee curve calculation */
static int32_t knee_curveK(const struct sof_drc_params *p, int32_t x)
{
	int32_t gamma;
	int32_t knee_exp_gamma;
	int32_t knee_curve_k;

	gamma = drc_mult_lshift(x, -p->K, drc_get_lshift(31, 20, 27));
	knee_exp_gamma = sofm_exp_fixed(gamma);
	knee_curve_k = drc_mult_lshift(p->knee_beta, knee_exp_gamma, drc_get_lshift(24, 20, 24));
	knee_curve_k += p->knee_alpha;
	return knee_curve_k;
}

/* Full compression curve */
static int32_t volume_gain(const struct sof_drc_params *p, int32_t x)
{
	const int32_t knee_threshold = sat_clamp64(((int64_t)p->knee_threshold) << 7);
	const int32_t linear_threshold = sat_clamp64(((int64_t)p->linear_threshold) << 1);
	int32_t exp_knee;
	int32_t y;
	int32_t tmp;
	int32_t tmp2;

	if (x < knee_threshold) {
		if (x < linear_threshold)
			return ONE_Q30;
		y = drc_mult_lshift(knee_curveK(p, x), drc_inv_fixed(x, 31, 20),
				    drc_get_lshift(24, 20, 30));
	} else {
		tmp = (x + 16) >> 5;
		tmp = drc_log_fixed(tmp);
		tmp2 = p->slope - ONE_Q30;
		exp_knee = sofm_exp_fixed(drc_mult_lshift(tmp, tmp2, drc_get_lshift(26, 30, 27)));
		y = drc_mult_lshift(p->ratio_base, exp_knee, drc_get_lshift(30, 20, 30));
	}

	return y;
}

/* Update detector average across 32 frames */
void drc_update_detector_average(struct drc_state *state,
				 const struct sof_drc_params *p,
				 int nbyte,
				 int nch)
{
	int32_t detector_average = state->detector_average;
	int32_t abs_input_array[DRC_DIVISION_FRAMES];
	int div_start, i, ch;
	const int16_t *sample16_p;
	const int32_t *sample32_p;
	int32_t gain;
	int32_t gain_diff;
	int32_t db_per_frame;
	int32_t sat_release_rate;
	int32_t tmp;
	int is_release;

	if (state->pre_delay_write_index == 0)
		div_start = CONFIG_DRC_MAX_PRE_DELAY_FRAMES - DRC_DIVISION_FRAMES;
	else
		div_start = state->pre_delay_write_index - DRC_DIVISION_FRAMES;

	memset(abs_input_array, 0, sizeof(abs_input_array));

	if (nbyte == 2) {
		for (ch = 0; ch < nch; ch++) {
			sample16_p = (const int16_t *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				int32_t sample = ((int32_t)sample16_p[i]) << 16;
				int32_t s_abs = sample < 0 ? -sample : sample;
				if (s_abs > abs_input_array[i])
					abs_input_array[i] = s_abs;
			}
		}
#if CONFIG_FORMAT_FLOAT
	} else if (nbyte == 0) { /* float */
		for (ch = 0; ch < nch; ch++) {
			const float *sample_f = (const float *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				float val = sample_f[i] < 0.0f ? -sample_f[i] : sample_f[i];
				if (val > 1.0f)
					val = 1.0f;
				int32_t s_abs = (int32_t)(val * 2147483647.0f);
				if (s_abs > abs_input_array[i])
					abs_input_array[i] = s_abs;
			}
		}
#endif
	} else {
		for (ch = 0; ch < nch; ch++) {
			sample32_p = (const int32_t *)state->pre_delay_buffers[ch] + div_start;
			for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
				int32_t sample = sample32_p[i];
				int32_t s_abs = sample < 0 ? -sample : sample;
				if (s_abs > abs_input_array[i])
					abs_input_array[i] = s_abs;
			}
		}
	}

	for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
		gain = volume_gain(p, abs_input_array[i]);
		gain_diff = gain - detector_average;
		is_release = (gain_diff > 0);

		if (is_release) {
			if (gain > NEG_TWO_DB_Q30) {
				tmp = drc_mult_lshift(gain_diff, p->sat_release_rate_at_neg_two_db,
						      drc_get_lshift(30, 30, 30));
			} else {
				int32_t gain_q26 = (gain + 8) >> 4;
				db_per_frame = drc_mult_lshift(drc_lin2db_fixed(gain_q26),
							       p->sat_release_frames_inv_neg,
							       drc_get_lshift(21, 30, 24));
				sat_release_rate = sofm_db2lin_fixed(db_per_frame) - ONE_Q20;
				tmp = drc_mult_lshift(gain_diff, sat_release_rate,
						      drc_get_lshift(30, 20, 30));
			}
			detector_average += tmp;
		} else {
			detector_average = gain;
		}

		if (detector_average > ONE_Q30)
			detector_average = ONE_Q30;
	}

	state->detector_average = detector_average;
}

/* Update envelope parameters for the next division */
void drc_update_envelope(struct drc_state *state, const struct sof_drc_params *p)
{
	int32_t envelope_rate;
	int32_t compression_diff_db;
	int32_t x, x2, x3, x4;
	int32_t release_frames;
	int32_t db_per_frame;
	int32_t tmp, tmp2;
	int32_t scaled_desired_gain;
	int32_t eff_atten_diff_db;
	int32_t lshift;
	int is_releasing;
	int is_bad_db;

	scaled_desired_gain = drc_asin_fixed(state->detector_average);

	is_releasing = scaled_desired_gain > state->compressor_gain;
	is_bad_db = (state->compressor_gain == 0 || scaled_desired_gain == 0);

	tmp = (state->compressor_gain + 8) >> 4;
	tmp2 = (scaled_desired_gain + 8) >> 4;
	compression_diff_db = drc_lin2db_fixed(tmp) - drc_lin2db_fixed(tmp2);

	if (is_releasing) {
		state->max_attack_compression_diff_db = INT32_MIN;

		if (is_bad_db)
			compression_diff_db = -ONE_Q21;

		x = compression_diff_db;
		if (x < -TWELVE_Q21)
			x = -TWELVE_Q21;
		if (x > 0)
			x = 0;
		x = (x + TWELVE_Q21 + 2) >> 2;

		lshift = drc_get_lshift(21, 21, 21);
		x2 = drc_mult_lshift(x, x, lshift);
		x3 = drc_mult_lshift(x2, x, lshift);
		x4 = drc_mult_lshift(x2, x2, lshift);

		/* 4th-order polynomial evaluation */
		int64_t rel64 = ((int64_t)p->kA) << 6;
		rel64 += (((int64_t)p->kB) * x) >> 15;
		rel64 += (((int64_t)p->kC) * x2) >> 15;
		rel64 += (((int64_t)p->kD) * x3) >> 15;
		rel64 += (((int64_t)p->kE) * x4) >> 15;
		release_frames = sat_clamp64((rel64 + 32) >> 6);

		db_per_frame = drc_inv_fixed(release_frames, 12, 30);
		tmp = p->kSpacingDb << 16;
		lshift = drc_get_lshift(30, 16, 24);
		db_per_frame = drc_mult_lshift(db_per_frame, tmp, lshift);
		envelope_rate = sofm_db2lin_fixed(db_per_frame);
	} else {
		if (is_bad_db)
			compression_diff_db = ONE_Q21;

		tmp = compression_diff_db << 3;
		if (tmp > state->max_attack_compression_diff_db)
			state->max_attack_compression_diff_db = tmp;

		eff_atten_diff_db = state->max_attack_compression_diff_db;
		if (eff_atten_diff_db < HALF_Q24)
			eff_atten_diff_db = HALF_Q24;

		x = drc_inv_fixed(eff_atten_diff_db, 22, 26);
		envelope_rate = ONE_Q20 - drc_pow_fixed(x, p->one_over_attack_frames);
	}

	state->envelope_rate = envelope_rate << 10;
	state->scaled_desired_gain = scaled_desired_gain;
}

/* Compress output across 4-frame unrolled iterations */
void drc_compress_output(struct drc_state *state,
			 const struct sof_drc_params *p,
			 int nbyte,
			 int nch)
{
	const int div_start = state->pre_delay_read_index;
	const int count = DRC_DIVISION_FRAMES >> 2;
	int32_t x[4];
	int32_t c, base, r, r2, r4;
	int32_t post_warp_compressor_gain;
	int32_t total_gain;
	int32_t tmp;
	int i, j, ch, inc;
	int16_t *sample16_p;
	int32_t *sample32_p;
	int32_t lshift;
	const int is_2byte = (nbyte == 2);

	if (state->envelope_rate < ONE_Q30) {
		/* Attack mode */
		c = state->compressor_gain - state->scaled_desired_gain;
		base = state->scaled_desired_gain;
		r = ONE_Q30 - state->envelope_rate;
		lshift = drc_get_lshift(30, 30, 30);
		x[0] = drc_mult_lshift(c, r, lshift);
		for (j = 1; j < 4; j++)
			x[j] = drc_mult_lshift(x[j - 1], r, lshift);
		r2 = drc_mult_lshift(r, r, lshift);
		r4 = drc_mult_lshift(r2, r2, lshift);

		i = 0;
		inc = 0;
		if (is_2byte) {
			while (1) {
				for (j = 0; j < 4; j++) {
					tmp = x[j] + base;
					post_warp_compressor_gain = drc_sin_fixed(tmp);
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift);
					lshift = drc_get_lshift(15, 24, 15);
					for (ch = 0; ch < nch; ch++) {
						sample16_p = (int16_t *)state->pre_delay_buffers[ch] +
							     div_start + inc;
						int32_t sample = (int32_t)*sample16_p;
						sample = drc_mult_lshift(sample, total_gain, lshift);
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
#if CONFIG_FORMAT_FLOAT
		} else if (nbyte == 0) { /* float */
			const float gain_scale = 1.0f / 16777216.0f;
			while (1) {
				for (j = 0; j < 4; j++) {
					tmp = x[j] + base;
					post_warp_compressor_gain = drc_sin_fixed(tmp);
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift);
					float total_gain_f = (float)total_gain * gain_scale;
					for (ch = 0; ch < nch; ch++) {
						float *sample_f = (float *)state->pre_delay_buffers[ch] +
								  div_start + inc;
						*sample_f = *sample_f * total_gain_f;
					}
					inc++;
				}
				if (++i == count)
					break;
				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++)
					x[j] = drc_mult_lshift(x[j], r4, lshift);
			}
#endif
		} else {
			while (1) {
				for (j = 0; j < 4; j++) {
					tmp = x[j] + base;
					post_warp_compressor_gain = drc_sin_fixed(tmp);
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift);
					lshift = drc_get_lshift(31, 24, 31);
					for (ch = 0; ch < nch; ch++) {
						sample32_p = (int32_t *)state->pre_delay_buffers[ch] +
							     div_start + inc;
						int32_t sample = *sample32_p;
						sample = drc_mult_lshift(sample, total_gain, lshift);
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

		state->compressor_gain = x[3] + base;
	} else {
		/* Release mode */
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
		if (is_2byte) {
			while (1) {
				for (j = 0; j < 4; j++) {
					post_warp_compressor_gain = drc_sin_fixed(x[j]);
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift);
					lshift = drc_get_lshift(15, 24, 15);
					for (ch = 0; ch < nch; ch++) {
						sample16_p = (int16_t *)state->pre_delay_buffers[ch] +
							     div_start + inc;
						int32_t sample = (int32_t)*sample16_p;
						sample = drc_mult_lshift(sample, total_gain, lshift);
						*sample16_p = sat_int16(sample);
					}
					inc++;
				}
				if (++i == count)
					break;
				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, lshift);
					x[j] = tmp < ONE_Q30 ? tmp : ONE_Q30;
				}
			}
#if CONFIG_FORMAT_FLOAT
		} else if (nbyte == 0) { /* float */
			const float gain_scale = 1.0f / 16777216.0f;
			while (1) {
				for (j = 0; j < 4; j++) {
					post_warp_compressor_gain = drc_sin_fixed(x[j]);
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift);
					float total_gain_f = (float)total_gain * gain_scale;
					for (ch = 0; ch < nch; ch++) {
						float *sample_f = (float *)state->pre_delay_buffers[ch] +
								  div_start + inc;
						*sample_f = *sample_f * total_gain_f;
					}
					inc++;
				}
				if (++i == count)
					break;
				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, lshift);
					x[j] = tmp < ONE_Q30 ? tmp : ONE_Q30;
				}
			}
#endif
		} else {
			while (1) {
				for (j = 0; j < 4; j++) {
					post_warp_compressor_gain = drc_sin_fixed(x[j]);
					lshift = drc_get_lshift(24, 31, 24);
					total_gain = drc_mult_lshift(p->master_linear_gain,
								     post_warp_compressor_gain,
								     lshift);
					lshift = drc_get_lshift(31, 24, 31);
					for (ch = 0; ch < nch; ch++) {
						sample32_p = (int32_t *)state->pre_delay_buffers[ch] +
							     div_start + inc;
						int32_t sample = *sample32_p;
						sample = drc_mult_lshift(sample, total_gain, lshift);
						*sample32_p = sample;
					}
					inc++;
				}
				if (++i == count)
					break;
				lshift = drc_get_lshift(30, 30, 30);
				for (j = 0; j < 4; j++) {
					tmp = drc_mult_lshift(x[j], r4, lshift);
					x[j] = tmp < ONE_Q30 ? tmp : ONE_Q30;
				}
			}
		}

		state->compressor_gain = x[3];
	}
}

#endif /* SOF_USE_RISCV_SIMD(DRC) */
