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

#define ONE_Q20        Q_CONVERT_FLOAT(1.0f, 20)                /* Q12.20 */
#define ONE_Q21        Q_CONVERT_FLOAT(1.0f, 21)                /* Q11.21 */
#define ONE_Q30        Q_CONVERT_FLOAT(1.0f, 30)                /* Q2.30 */
#define TWELVE_Q21     Q_CONVERT_FLOAT(12.0f, 21)               /* Q11.21 */
#define HALF_Q24       Q_CONVERT_FLOAT(0.5f, 24)                /* Q8.24 */
#define NEG_TWO_DB_Q30 Q_CONVERT_FLOAT(0.7943282347242815f, 30) /* -2dB = 10^(-2/20); Q2.30 */

/* This is the knee part of the compression curve. Returns the output level
 * given the input level x. */
static int32_t knee_curveK(const struct sof_drc_params *p, int32_t x)
{
	int32_t knee_exp_gamma;

	/* The formula in knee_curveK is linear_threshold +
	 * (1 - expf(-k * (x - linear_threshold))) / k
	 * which simplifies to (alpha + beta * expf(gamma))
	 * where alpha = linear_threshold + 1 / k
	 *	 beta = -expf(k * linear_threshold) / k
	 *	 gamma = -k * x
	 */
	knee_exp_gamma = exp_fixed(Q_MULTSR_32X32((int64_t)x, -p->K, 31, 20, 27)); /* Q12.20 */
	return p->knee_alpha + Q_MULTSR_32X32((int64_t)p->knee_beta, knee_exp_gamma, 24, 20, 24);
}

/* Full compression curve with constant ratio after knee. Returns the ratio of
 * output and input signal. */
static int32_t volume_gain(const struct sof_drc_params *p, int32_t x)
{
	const int32_t knee_threshold =
		sat_int32(Q_SHIFT_LEFT((int64_t)p->knee_threshold, 24, 31));
	const int32_t linear_threshold =
		sat_int32(Q_SHIFT_LEFT((int64_t)p->linear_threshold, 30, 31));
	int32_t exp_knee;
	int32_t y;

	if (x < knee_threshold) {
		if (x < linear_threshold)
			return ONE_Q30;
		/* y = knee_curveK(x) / x */
		y = Q_MULTSR_32X32((int64_t)knee_curveK(p, x), drc_inv_fixed(x, 31, 20),
				   24, 20, 30);
	} else {
		/* Constant ratio after knee.
		 * log(y/y0) = s * log(x/x0)
		 * => y = y0 * (x/x0)^s
		 * => y = [y0 * (1/x0)^s] * x^s
		 * => y = ratio_base * x^s
		 * => y/x = ratio_base * x^(s - 1)
		 * => y/x = ratio_base * e^(log(x) * (s - 1))
		 */
		exp_knee = exp_fixed(Q_MULTSR_32X32((int64_t)drc_log_fixed(Q_SHIFT_RND(x, 31, 26)),
						    (p->slope - ONE_Q30), 26, 30, 27)); /* Q12.20 */
		y = Q_MULTSR_32X32((int64_t)p->ratio_base, exp_knee, 30, 20, 30);
	}

	return y;
}

/* Update detector_average from the last input division. */
void drc_update_detector_average(struct drc_state *state,
				 const struct sof_drc_params *p,
				 int nbyte,
				 int nch)
{
	int32_t detector_average = state->detector_average; /* Q2.30 */
	int32_t abs_input_array[DRC_DIVISION_FRAMES]; /* Q1.31 */
	int div_start, i, ch;
	int16_t *sample16_p; /* for s16 format case */
	int32_t *sample32_p; /* for s24 and s32 format cases */
	int32_t sample;
	int32_t gain;
	int32_t gain_diff;
	int is_release;
	int32_t db_per_frame;
	int32_t sat_release_rate;

	/* Calculate the start index of the last input division */
	if (state->pre_delay_write_index == 0) {
		div_start = DRC_MAX_PRE_DELAY_FRAMES - DRC_DIVISION_FRAMES;
	} else {
		div_start = state->pre_delay_write_index - DRC_DIVISION_FRAMES;
	}

	/* The max abs value across all channels for this frame */
	if (nbyte == 2) { /* 2 bytes per sample */
		for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
			abs_input_array[i] = 0;
			for (ch = 0; ch < nch; ch++) {
				sample16_p =
					(int16_t *)state->pre_delay_buffers[ch] + div_start + i;
				sample = Q_SHIFT_LEFT((int32_t)*sample16_p, 15, 31);
				abs_input_array[i] = MAX(abs_input_array[i], ABS(sample));
			}
		}
	} else { /* 4 bytes per sample */
		for (i = 0; i < DRC_DIVISION_FRAMES; i++) {
			abs_input_array[i] = 0;
			for (ch = 0; ch < nch; ch++) {
				sample32_p =
					(int32_t *)state->pre_delay_buffers[ch] + div_start + i;
				sample = *sample32_p;
				abs_input_array[i] = MAX(abs_input_array[i], ABS(sample));
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
		gain_diff = gain - detector_average; /* Q2.30 */
		is_release = (gain_diff > 0);
		if (is_release) {
			if (gain > NEG_TWO_DB_Q30) {
				detector_average +=
					Q_MULTSR_32X32((int64_t)gain_diff,
						       p->sat_release_rate_at_neg_two_db,
						       30, 30, 30);
			} else {
				db_per_frame =
					Q_MULTSR_32X32((int64_t)drc_lin2db_fixed(Q_SHIFT_RND(gain,
											     30,
											     26)),
						       p->sat_release_frames_inv_neg,
						       21, 30, 24); /* Q8.24 */
				sat_release_rate =
					db2lin_fixed(db_per_frame) - ONE_Q20; /* Q12.20 */
				detector_average += Q_MULTSR_32X32((int64_t)gain_diff,
								   sat_release_rate, 30, 20, 30);
			}
		} else {
			detector_average = gain;
		}

		detector_average = MIN(detector_average, ONE_Q30);
	}

	state->detector_average = detector_average;
}

/* Updates the envelope_rate used for the next division */
void drc_update_envelope(struct drc_state *state, const struct sof_drc_params *p)
{
	/* Calculate desired gain */

	/* Pre-warp so we get desired_gain after sin() warp below. */
	int32_t scaled_desired_gain = drc_asin_fixed(state->detector_average); /* Q2.30 */

	/* Deal with envelopes */

	/* envelope_rate is the rate we slew from current compressor level to
	 * the desired level.  The exact rate depends on if we're attacking or
	 * releasing and by how much.
	 */
	int32_t envelope_rate;

	int is_releasing = scaled_desired_gain > state->compressor_gain;

	/* compression_diff_db is the difference between current compression
	 * level and the desired level. */
	int is_bad_db = (state->compressor_gain == 0 || scaled_desired_gain == 0);
	int32_t compression_diff_db =
		drc_lin2db_fixed(Q_SHIFT_RND(state->compressor_gain, 30, 26)) -
			drc_lin2db_fixed(Q_SHIFT_RND(scaled_desired_gain, 30, 26)); /* Q11.21 */

	int32_t x, x2, x3, x4;
	int32_t release_frames;
	int32_t db_per_frame;
	int32_t eff_atten_diff_db;

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
		x = MAX(-TWELVE_Q21, x);
		x = MIN(0, x);
		/* x = 0.25f * (x + 12) */
		x = Q_SHIFT_RND(x + TWELVE_Q21, 21, 19);

		/* Compute adaptive release curve using 4th order polynomial.
		 * Normal values for the polynomial coefficients would create a
		 * monotonically increasing function.
		 */
		x2 = Q_MULTSR_32X32((int64_t)x, x, 21, 21, 21); /* Q11.21 */
		x3 = Q_MULTSR_32X32((int64_t)x2, x, 21, 21, 21); /* Q11.21 */
		x4 = Q_MULTSR_32X32((int64_t)x2, x2, 21, 21, 21); /* Q11.21 */

		release_frames = Q_MULTSR_32X32((int64_t)p->kE, x4, 12, 21, 12) +
			Q_MULTSR_32X32((int64_t)p->kD, x3, 12, 21, 12) +
				Q_MULTSR_32X32((int64_t)p->kC, x2, 12, 21, 12) +
					Q_MULTSR_32X32((int64_t)p->kB, x, 12, 21, 12) + p->kA;
		/* db_per_frame = kSpacingDb / release_frames */
		db_per_frame = drc_inv_fixed(release_frames, 12, 30); /* Q2.30 */
		db_per_frame = Q_MULTSR_32X32((int64_t)db_per_frame, p->kSpacingDb, 30, 0, 24);
		envelope_rate = db2lin_fixed(db_per_frame); /* Q12.20 */
	} else {
		int32_t sat32;
		/* Attack mode - compression_diff_db should be positive dB */

		/* Fix gremlins. */
		if (is_bad_db)
			compression_diff_db = ONE_Q21;

		/* As long as we're still in attack mode, use a rate based off
		 * the largest compression_diff_db we've encountered so far.
		 */
		sat32 = sat_int32(Q_SHIFT_LEFT((int64_t)compression_diff_db, 21, 24));
		state->max_attack_compression_diff_db =
			MAX(state->max_attack_compression_diff_db, sat32);

		eff_atten_diff_db =
			MAX(HALF_Q24, state->max_attack_compression_diff_db); /* Q8.24 */

		/* x = 0.25f / eff_atten_diff_db;
		 * => x = 1.0f / (eff_atten_diff_db << 2);
		 */
		x = drc_inv_fixed(eff_atten_diff_db, 22 /* Q8.24 << 2 */, 26); /* Q6.26 */
		envelope_rate = ONE_Q20 - drc_pow_fixed(x, p->one_over_attack_frames); /* Q12.20 */
	}

	state->envelope_rate = sat_int32(Q_SHIFT_LEFT((int64_t)envelope_rate, 20, 30));
	state->scaled_desired_gain = scaled_desired_gain;
}

/* Calculate compress_gain from the envelope and apply total_gain to compress
 * the next output division. */
void drc_compress_output(struct drc_state *state,
			 const struct sof_drc_params *p,
			 int nbyte,
			 int nch)
{
	const int div_start = state->pre_delay_read_index;
	int count = DRC_DIVISION_FRAMES >> 2;

	int32_t c, base, r, r2, r4; /* Q2.30 */
	int32_t x[4]; /* Q2.30 */
	int32_t post_warp_compressor_gain;
	int32_t total_gain;

	int i, j, ch, inc;
	int16_t *sample16_p; /* for s16 format case */
	int32_t *sample32_p; /* for s24 and s32 format cases */
	int32_t sample;
	int is_2byte = (nbyte == 2); /* otherwise is 4-bytes */

	/* Exponential approach to desired gain. */
	if (state->envelope_rate < ONE_Q30) {
		/* Attack - reduce gain to desired. */
		c = state->compressor_gain - state->scaled_desired_gain;
		base = state->scaled_desired_gain;
		r = ONE_Q30 - state->envelope_rate;
		x[0] = Q_MULTSR_32X32((int64_t)c,  r, 30, 30, 30);
		for (j = 1; j < 4; j++)
			x[j] = Q_MULTSR_32X32((int64_t)x[j - 1], r, 30, 30, 30);
		r2 = Q_MULTSR_32X32((int64_t)r, r, 30, 30, 30);
		r4 = Q_MULTSR_32X32((int64_t)r2, r2, 30, 30, 30);

		i = 0;
		inc = 0;
		while (1) {
			if (is_2byte) { /* 2 bytes per sample */
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain =
						drc_sin_fixed(x[j] + base); /* Q1.31 */

					/* Calculate total gain using master gain. */
					total_gain = Q_MULTSR_32X32((int64_t)p->master_linear_gain,
								    post_warp_compressor_gain,
								    24, 31, 24); /* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample16_p =
							(int16_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = (int32_t)*sample16_p;
						*sample16_p =
							sat_int16(Q_MULTSR_32X32((int64_t)sample,
										 total_gain,
										 15, 24, 15));
					}
					inc++;
				}
			} else { /* 4 bytes per sample */
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain =
						drc_sin_fixed(x[j] + base); /* Q1.31 */

					/* Calculate total gain using master gain. */
					total_gain = Q_MULTSR_32X32((int64_t)p->master_linear_gain,
								    post_warp_compressor_gain,
								    24, 31, 24); /* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample32_p =
							(int32_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = *sample32_p;
						*sample32_p =
							sat_int32(Q_MULTSR_32X32((int64_t)sample,
										 total_gain,
										 31, 24, 31));
					}
					inc++;
				}
			}

			if (++i == count)
				break;

			for (j = 0; j < 4; j++)
				x[j] = Q_MULTSR_32X32((int64_t)x[j], r4, 30, 30, 30);
		}

		state->compressor_gain = x[3] + base;
	} else {
		/* Release - exponentially increase gain to 1.0 */
		c = state->compressor_gain;
		r = state->envelope_rate;
		x[0] = Q_MULTSR_32X32((int64_t)c,  r, 30, 30, 30);
		for (j = 1; j < 4; j++)
			x[j] = Q_MULTSR_32X32((int64_t)x[j - 1], r, 30, 30, 30);
		r2 = Q_MULTSR_32X32((int64_t)r, r, 30, 30, 30);
		r4 = Q_MULTSR_32X32((int64_t)r2, r2, 30, 30, 30);

		i = 0;
		inc = 0;
		while (1) {
			if (is_2byte) { /* 2 bytes per sample */
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain = drc_sin_fixed(x[j]); /* Q1.31 */

					/* Calculate total gain using master gain. */
					total_gain = Q_MULTSR_32X32((int64_t)p->master_linear_gain,
								    post_warp_compressor_gain,
								    24, 31, 24); /* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample16_p =
							(int16_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = (int32_t)*sample16_p;
						*sample16_p =
							sat_int16(Q_MULTSR_32X32((int64_t)sample,
										 total_gain,
										 15, 24, 15));
					}
					inc++;
				}
			} else { /* 4 bytes per sample */
				for (j = 0; j < 4; j++) {
					/* Warp pre-compression gain to smooth out sharp
					 * exponential transition points.
					 */
					post_warp_compressor_gain = drc_sin_fixed(x[j]); /* Q1.31 */

					/* Calculate total gain using master gain. */
					total_gain = Q_MULTSR_32X32((int64_t)p->master_linear_gain,
								    post_warp_compressor_gain,
								    24, 31, 24); /* Q8.24 */

					/* Apply final gain. */
					for (ch = 0; ch < nch; ch++) {
						sample32_p =
							(int32_t *)state->pre_delay_buffers[ch] +
								div_start + inc;
						sample = *sample32_p;
						*sample32_p =
							sat_int32(Q_MULTSR_32X32((int64_t)sample,
										 total_gain,
										 31, 24, 31));
					}
					inc++;
				}
			}

			if (++i == count)
				break;

			for (j = 0; j < 4; j++)
				x[j] = MIN(ONE_Q30, Q_MULTSR_32X32((int64_t)x[j], r4, 30, 30, 30));
		}

		state->compressor_gain = x[3];
	}
}

/* After one complete divison of samples have been received (and one divison of
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

#if CONFIG_FORMAT_S16LE
static void drc_s16_default_pass(const struct comp_dev *dev,
				 const struct audio_stream *source,
				 struct audio_stream *sink,
				 uint32_t frames)
{
	int16_t *x;
	int16_t *y;
	int i;
	int n = source->channels * frames;

	for (i = 0; i < n; i++) {
		x = audio_stream_read_frag_s16(source, i);
		y = audio_stream_write_frag_s16(sink, i);
		*y = *x;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static void drc_s32_default_pass(const struct comp_dev *dev,
				 const struct audio_stream *source,
				 struct audio_stream *sink,
				 uint32_t frames)
{
	int32_t *x;
	int32_t *y;
	int i;
	int n = source->channels * frames;

	for (i = 0; i < n; i++) {
		x = audio_stream_read_frag_s32(source, i);
		y = audio_stream_write_frag_s32(sink, i);
		*y = *x;
	}
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
static void drc_s16_default(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int16_t *x;
	int16_t *y;
	int16_t *pd_write;
	int16_t *pd_read;
	int offset;
	int i = 0;
	int ch;
	int idx;
	int f;
	int fragment;
	int pd_write_index;
	int pd_read_index;
	int nch = source->channels;

	struct drc_comp_data *cd = comp_get_drvdata(dev);
	struct drc_state *state = &cd->state;
	const struct sof_drc_params *p = &cd->config->params; /* Read-only */

	if (!p->enabled) {
		/* Delay the input sample only and don't do other processing. This is used when the
		 * DRC is disabled. We want to do this to match the processing delay of other bands
		 * in multi-band DRC kernel case.
		 */
		for (ch = 0; ch < nch; ++ch) {
			pd_write_index = state->pre_delay_write_index;
			pd_read_index = state->pre_delay_read_index;
			pd_write = (int16_t *)state->pre_delay_buffers[ch] + pd_write_index;
			pd_read = (int16_t *)state->pre_delay_buffers[ch] + pd_read_index;
			idx = ch;
			for (i = 0; i < frames; ++i) {
				x = audio_stream_read_frag_s16(source, idx);
				y = audio_stream_write_frag_s16(sink, idx);
				*pd_write = *x;
				*y = *pd_read;
				if (++pd_write_index == DRC_MAX_PRE_DELAY_FRAMES) {
					pd_write_index = 0;
					pd_write = (int16_t *)state->pre_delay_buffers[ch];
				} else {
					pd_write++;
				}
				if (++pd_read_index == DRC_MAX_PRE_DELAY_FRAMES) {
					pd_read_index = 0;
					pd_read = (int16_t *)state->pre_delay_buffers[ch];
				} else {
					pd_read++;
				}
				idx += nch;
			}
		}

		state->pre_delay_write_index += frames;
		state->pre_delay_write_index &= DRC_MAX_PRE_DELAY_FRAMES_MASK;
		state->pre_delay_read_index += frames;
		state->pre_delay_read_index &= DRC_MAX_PRE_DELAY_FRAMES_MASK;
		return;
	}

	if (!state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 2, nch);
		state->processed = 1;
	}

	offset = state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK;
	while (i < frames) {
		/* Copy fragment data from source to pre-delay buffers, and copy the output fragment
		 * to sink.
		 */
		fragment = MIN(DRC_DIVISION_FRAMES - offset, frames - i);
		pd_write_index = state->pre_delay_write_index;
		pd_read_index = state->pre_delay_read_index;
		for (ch = 0; ch < nch; ++ch) {
			pd_write = (int16_t *)state->pre_delay_buffers[ch] + pd_write_index;
			pd_read = (int16_t *)state->pre_delay_buffers[ch] + pd_read_index;
			idx = i * nch + ch;
			for (f = 0; f < fragment; ++f) {
				x = audio_stream_read_frag_s16(source, idx);
				y = audio_stream_write_frag_s16(sink, idx);
				*pd_write = *x;
				*y = *pd_read;
				pd_write++;
				pd_read++;
				idx += nch;
			}
		}
		state->pre_delay_write_index =
			(pd_write_index + fragment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
		state->pre_delay_read_index =
			(pd_read_index + fragment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;

		i += fragment;
		offset = (offset + fragment) & DRC_DIVISION_FRAMES_MASK;

		/* Process the input division (32 frames). */
		if (offset == 0)
			drc_process_one_division(state, p, 2, nch);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void drc_s24_default(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int32_t *x;
	int32_t *y;
	int32_t *pd_write;
	int32_t *pd_read;
	int offset;
	int i = 0;
	int ch;
	int idx;
	int f;
	int fragment;
	int pd_write_index;
	int pd_read_index;
	int nch = source->channels;

	struct drc_comp_data *cd = comp_get_drvdata(dev);
	struct drc_state *state = &cd->state;
	const struct sof_drc_params *p = &cd->config->params; /* Read-only */

	if (!p->enabled) {
		/* Delay the input sample only and don't do other processing. This is used when the
		 * DRC is disabled. We want to do this to match the processing delay of other bands
		 * in multi-band DRC kernel case.
		 */
		for (ch = 0; ch < nch; ++ch) {
			pd_write_index = state->pre_delay_write_index;
			pd_read_index = state->pre_delay_read_index;
			pd_write = (int32_t *)state->pre_delay_buffers[ch] + pd_write_index;
			pd_read = (int32_t *)state->pre_delay_buffers[ch] + pd_read_index;
			idx = ch;
			for (i = 0; i < frames; ++i) {
				x = audio_stream_read_frag_s32(source, idx);
				y = audio_stream_write_frag_s32(sink, idx);
				*pd_write = *x;
				*y = *pd_read;
				if (++pd_write_index == DRC_MAX_PRE_DELAY_FRAMES) {
					pd_write_index = 0;
					pd_write = (int32_t *)state->pre_delay_buffers[ch];
				} else {
					pd_write++;
				}
				if (++pd_read_index == DRC_MAX_PRE_DELAY_FRAMES) {
					pd_read_index = 0;
					pd_read = (int32_t *)state->pre_delay_buffers[ch];
				} else {
					pd_read++;
				}
				idx += nch;
			}
		}

		state->pre_delay_write_index += frames;
		state->pre_delay_write_index &= DRC_MAX_PRE_DELAY_FRAMES_MASK;
		state->pre_delay_read_index += frames;
		state->pre_delay_read_index &= DRC_MAX_PRE_DELAY_FRAMES_MASK;
		return;
	}

	if (!state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 4, nch);
		state->processed = 1;
	}

	offset = state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK;
	while (i < frames) {
		/* Copy fragment data from source to pre-delay buffers, and copy the output fragment
		 * to sink.
		 */
		fragment = MIN(DRC_DIVISION_FRAMES - offset, frames - i);
		pd_write_index = state->pre_delay_write_index;
		pd_read_index = state->pre_delay_read_index;
		for (ch = 0; ch < nch; ++ch) {
			pd_write = (int32_t *)state->pre_delay_buffers[ch] + pd_write_index;
			pd_read = (int32_t *)state->pre_delay_buffers[ch] + pd_read_index;
			idx = i * nch + ch;
			for (f = 0; f < fragment; ++f) {
				x = audio_stream_read_frag_s32(source, idx);
				y = audio_stream_write_frag_s32(sink, idx);

				/* Write/Read pre_delay_buffer as s32 format */
				*pd_write = *x << 8;
				*y = sat_int24(Q_SHIFT_RND(*pd_read, 31, 23));

				pd_write++;
				pd_read++;
				idx += nch;
			}
		}
		state->pre_delay_write_index =
			(pd_write_index + fragment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
		state->pre_delay_read_index =
			(pd_read_index + fragment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;

		i += fragment;
		offset = (offset + fragment) & DRC_DIVISION_FRAMES_MASK;

		/* Process the input division (32 frames). */
		if (offset == 0)
			drc_process_one_division(state, p, 4, nch);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void drc_s32_default(const struct comp_dev *dev,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    uint32_t frames)
{
	int32_t *x;
	int32_t *y;
	int32_t *pd_write;
	int32_t *pd_read;
	int offset;
	int i = 0;
	int ch;
	int idx;
	int f;
	int fragment;
	int pd_write_index;
	int pd_read_index;
	int nch = source->channels;

	struct drc_comp_data *cd = comp_get_drvdata(dev);
	struct drc_state *state = &cd->state;
	const struct sof_drc_params *p = &cd->config->params; /* Read-only */

	if (!p->enabled) {
		/* Delay the input sample only and don't do other processing. This is used when the
		 * DRC is disabled. We want to do this to match the processing delay of other bands
		 * in multi-band DRC kernel case.
		 */
		for (ch = 0; ch < nch; ++ch) {
			pd_write_index = state->pre_delay_write_index;
			pd_read_index = state->pre_delay_read_index;
			pd_write = (int32_t *)state->pre_delay_buffers[ch] + pd_write_index;
			pd_read = (int32_t *)state->pre_delay_buffers[ch] + pd_read_index;
			idx = ch;
			for (i = 0; i < frames; ++i) {
				x = audio_stream_read_frag_s32(source, idx);
				y = audio_stream_write_frag_s32(sink, idx);
				*pd_write = *x;
				*y = *pd_read;
				if (++pd_write_index == DRC_MAX_PRE_DELAY_FRAMES) {
					pd_write_index = 0;
					pd_write = (int32_t *)state->pre_delay_buffers[ch];
				} else {
					pd_write++;
				}
				if (++pd_read_index == DRC_MAX_PRE_DELAY_FRAMES) {
					pd_read_index = 0;
					pd_read = (int32_t *)state->pre_delay_buffers[ch];
				} else {
					pd_read++;
				}
				idx += nch;
			}
		}

		state->pre_delay_write_index += frames;
		state->pre_delay_write_index &= DRC_MAX_PRE_DELAY_FRAMES_MASK;
		state->pre_delay_read_index += frames;
		state->pre_delay_read_index &= DRC_MAX_PRE_DELAY_FRAMES_MASK;
		return;
	}

	if (!state->processed) {
		drc_update_envelope(state, p);
		drc_compress_output(state, p, 4, nch);
		state->processed = 1;
	}

	offset = state->pre_delay_write_index & DRC_DIVISION_FRAMES_MASK;
	while (i < frames) {
		/* Copy fragment data from source to pre-delay buffers, and copy the output fragment
		 * to sink.
		 */
		fragment = MIN(DRC_DIVISION_FRAMES - offset, frames - i);
		pd_write_index = state->pre_delay_write_index;
		pd_read_index = state->pre_delay_read_index;
		for (ch = 0; ch < nch; ++ch) {
			pd_write = (int32_t *)state->pre_delay_buffers[ch] + pd_write_index;
			pd_read = (int32_t *)state->pre_delay_buffers[ch] + pd_read_index;
			idx = i * nch + ch;
			for (f = 0; f < fragment; ++f) {
				x = audio_stream_read_frag_s32(source, idx);
				y = audio_stream_write_frag_s32(sink, idx);
				*pd_write = *x;
				*y = *pd_read;
				pd_write++;
				pd_read++;
				idx += nch;
			}
		}
		state->pre_delay_write_index =
			(pd_write_index + fragment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;
		state->pre_delay_read_index =
			(pd_read_index + fragment) & DRC_MAX_PRE_DELAY_FRAMES_MASK;

		i += fragment;
		offset = (offset + fragment) & DRC_DIVISION_FRAMES_MASK;

		/* Process the input division (32 frames). */
		if (offset == 0)
			drc_process_one_division(state, p, 4, nch);
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

const struct drc_proc_fnmap drc_proc_fnmap_pass[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, drc_s16_default_pass },
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, drc_s32_default_pass },
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, drc_s32_default_pass },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t drc_proc_fncount = ARRAY_SIZE(drc_proc_fnmap);
