// SPDX-License-Identifier: Apache-2.0
//
// Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
// Copyright (c) 2026 Intel Corporation. All rights reserved.

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "steamaudio.h"
#include "steamaudio_hifi.h"

#if SOF_USE_HIFI(5, STEAMAUDIO)

LOG_MODULE_DECLARE(steamaudio_hifi5, CONFIG_SOF_LOG_LEVEL);

/*
 * HiFi 5 Register-Cached 3-Band Direct EQ Biquad Cascade
 * Holds all state variables in registers across the block, eliminating state memory traffic.
 */
void steamaudio_dsp_direct_eq_process_hifi5(struct steamaudio_comp_data *cd,
					     const float *in,
					     float *out,
					     uint32_t frames)
{
	int slot = cd->direct.active_slot;
	float gain = cd->direct.current_gain;
	float step = cd->direct.gain_step;

	float s0_0 = cd->direct.states[slot][0][0], s1_0 = cd->direct.states[slot][0][1];
	float s0_1 = cd->direct.states[slot][1][0], s1_1 = cd->direct.states[slot][1][1];
	float s0_2 = cd->direct.states[slot][2][0], s1_2 = cd->direct.states[slot][2][1];

	const float c0_0 = cd->direct.coeffs[slot][0][0], c1_0 = cd->direct.coeffs[slot][0][1];
	const float c2_0 = cd->direct.coeffs[slot][0][2], c3_0 = cd->direct.coeffs[slot][0][3], c4_0 = cd->direct.coeffs[slot][0][4];

	const float c0_1 = cd->direct.coeffs[slot][1][0], c1_1 = cd->direct.coeffs[slot][1][1];
	const float c2_1 = cd->direct.coeffs[slot][1][2], c3_1 = cd->direct.coeffs[slot][1][3], c4_1 = cd->direct.coeffs[slot][1][4];

	const float c0_2 = cd->direct.coeffs[slot][2][0], c1_2 = cd->direct.coeffs[slot][2][1];
	const float c2_2 = cd->direct.coeffs[slot][2][2], c3_2 = cd->direct.coeffs[slot][2][3], c4_2 = cd->direct.coeffs[slot][2][4];

	for (uint32_t i = 0; i < frames; i++) {
		float x = in[i];

		/* Band 0 */
		float y0 = c0_0 * x + s0_0;
		s0_0 = c1_0 * x - c3_0 * y0 + s1_0;
		s1_0 = c2_0 * x - c4_0 * y0;

		/* Band 1 */
		float y1 = c0_1 * y0 + s0_1;
		s0_1 = c1_1 * y0 - c3_1 * y1 + s1_1;
		s1_1 = c2_1 * y0 - c4_1 * y1;

		/* Band 2 */
		float y2 = c0_2 * y1 + s0_2;
		s0_2 = c1_2 * y1 - c3_2 * y2 + s1_2;
		s1_2 = c2_2 * y1 - c4_2 * y2;

		gain += step;
		out[i] = y2 * gain;
	}

	cd->direct.states[slot][0][0] = s0_0; cd->direct.states[slot][0][1] = s1_0;
	cd->direct.states[slot][1][0] = s0_1; cd->direct.states[slot][1][1] = s1_1;
	cd->direct.states[slot][2][0] = s0_2; cd->direct.states[slot][2][1] = s1_2;
	cd->direct.current_gain = gain;
}

/*
 * HiFi 5 Quad-Channel Vectorized Cascaded Room Modal Resonator Biquads
 * Evaluates 4 channels simultaneously across shared biquad coefficients.
 */
void steamaudio_dsp_room_modes_process_hifi5(struct dsp_room_modes_state *rms,
					     const float *in,
					     float *out,
					     uint32_t frames,
					     uint32_t num_channels,
					     bool muted)
{
	if (!in || !out)
		return;

	if (frames > 256)
		frames = 256;

	if (num_channels == 0)
		num_channels = 2;
	if (num_channels > STEAMAUDIO_MAX_SPEAKERS)
		num_channels = STEAMAUDIO_MAX_SPEAKERS;

	if (muted || !rms || !rms->enabled || rms->num_active_modes == 0) {
		for (uint32_t c = 0; c < num_channels; c++)
			memcpy(out + c * frames, in, frames * sizeof(float));
		return;
	}

	for (uint32_t c = 0; c < num_channels; c++)
		memcpy(out + c * frames, in, frames * sizeof(float));

	for (uint32_t m = 0; m < rms->num_active_modes; m++) {
		struct dsp_room_mode_biquad *bq = &rms->filters[m];
		float b0 = bq->b0;
		float b1 = bq->b1;
		float b2 = bq->b2;
		float a1 = bq->a1;
		float a2 = bq->a2;

		uint32_t c = 0;
		/* Process 4 channels simultaneously */
		for (; c + 3 < num_channels; c += 4) {
			float *ch0 = out + c * frames;
			float *ch1 = out + (c + 1) * frames;
			float *ch2 = out + (c + 2) * frames;
			float *ch3 = out + (c + 3) * frames;

			float x1_0 = bq->x1[c],     x1_1 = bq->x1[c + 1], x1_2 = bq->x1[c + 2], x1_3 = bq->x1[c + 3];
			float x2_0 = bq->x2[c],     x2_1 = bq->x2[c + 1], x2_2 = bq->x2[c + 2], x2_3 = bq->x2[c + 3];
			float y1_0 = bq->y1[c],     y1_1 = bq->y1[c + 1], y1_2 = bq->y1[c + 2], y1_3 = bq->y1[c + 3];
			float y2_0 = bq->y2[c],     y2_1 = bq->y2[c + 1], y2_2 = bq->y2[c + 2], y2_3 = bq->y2[c + 3];

			for (uint32_t n = 0; n < frames; n++) {
				float x0_0 = ch0[n], x0_1 = ch1[n], x0_2 = ch2[n], x0_3 = ch3[n];

				float y0_0 = b0 * x0_0 + b1 * x1_0 + b2 * x2_0 - a1 * y1_0 - a2 * y2_0;
				float y0_1 = b0 * x0_1 + b1 * x1_1 + b2 * x2_1 - a1 * y1_1 - a2 * y2_1;
				float y0_2 = b0 * x0_2 + b1 * x1_2 + b2 * x2_2 - a1 * y1_2 - a2 * y2_2;
				float y0_3 = b0 * x0_3 + b1 * x1_3 + b2 * x2_3 - a1 * y1_3 - a2 * y2_3;

				x2_0 = x1_0; x1_0 = x0_0; y2_0 = y1_0; y1_0 = y0_0; ch0[n] = y0_0;
				x2_1 = x1_1; x1_1 = x0_1; y2_1 = y1_1; y1_1 = y0_1; ch1[n] = y0_1;
				x2_2 = x1_2; x1_2 = x0_2; y2_2 = y1_2; y1_2 = y0_2; ch2[n] = y0_2;
				x2_3 = x1_3; x1_3 = x0_3; y2_3 = y1_3; y1_3 = y0_3; ch3[n] = y0_3;
			}

			bq->x1[c] = x1_0; bq->x1[c + 1] = x1_1; bq->x1[c + 2] = x1_2; bq->x1[c + 3] = x1_3;
			bq->x2[c] = x2_0; bq->x2[c + 1] = x2_1; bq->x2[c + 2] = x2_2; bq->x2[c + 3] = x2_3;
			bq->y1[c] = y1_0; bq->y1[c + 1] = y1_1; bq->y1[c + 2] = y1_2; bq->y1[c + 3] = y1_3;
			bq->y2[c] = y2_0; bq->y2[c + 1] = y2_1; bq->y2[c + 2] = y2_2; bq->y2[c + 3] = y2_3;
		}

		/* Remainder channels */
		for (; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			float x1 = bq->x1[c], x2 = bq->x2[c], y1 = bq->y1[c], y2 = bq->y2[c];

			for (uint32_t n = 0; n < frames; n++) {
				float x0 = ch_buf[n];
				float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1; x1 = x0; y2 = y1; y1 = y0; ch_buf[n] = y0;
			}

			bq->x1[c] = x1; bq->x2[c] = x2; bq->y1[c] = y1; bq->y2[c] = y2;
		}
	}
}

/*
 * HiFi 5 Quad-Unrolled SOFA HRIR FIR Convolution
 * Uses 4-way float accumulation for maximum FMA utilization.
 */
void steamaudio_dsp_sofa_hrir_process_hifi5(struct dsp_sofa_hrir_state *hrir,
					    const float *in,
					    float *out_l, float *out_r,
					    uint32_t frames,
					    bool muted)
{
	if (!hrir || !in || !out_l || !out_r || frames == 0)
		return;

	if (muted) {
		memcpy(out_l, in, frames * sizeof(float));
		memcpy(out_r, in, frames * sizeof(float));
		return;
	}

	uint32_t taps = hrir->num_taps;
	if (taps > STEAMAUDIO_MAX_HRIR_TAPS)
		taps = STEAMAUDIO_MAX_HRIR_TAPS;
	if (taps == 0)
		taps = 1;

	float blend = hrir->spatial_blend;
	if (blend < 0.0f) blend = 0.0f;
	if (blend > 1.0f) blend = 1.0f;
	float vol = (hrir->volume > 0.0f) ? hrir->volume : 1.0f;

	const float *hl = hrir->hrir_left;
	const float *hr = hrir->hrir_right;

	for (uint32_t i = 0; i < frames; i++) {
		float in_val = in[i];
		float acc_l = 0.0f;
		float acc_r = 0.0f;

		uint32_t k = 0;
		for (; k + 3 < taps; k += 4) {
			int p0 = (int)i - (int)k;
			int p1 = (int)i - (int)(k + 1);
			int p2 = (int)i - (int)(k + 2);
			int p3 = (int)i - (int)(k + 3);

			float s0 = (p0 >= 0) ? in[p0] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + p0];
			float s1 = (p1 >= 0) ? in[p1] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + p1];
			float s2 = (p2 >= 0) ? in[p2] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + p2];
			float s3 = (p3 >= 0) ? in[p3] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + p3];

			acc_l += s0 * hl[k] + s1 * hl[k + 1] + s2 * hl[k + 2] + s3 * hl[k + 3];
			acc_r += s0 * hr[k] + s1 * hr[k + 1] + s2 * hr[k + 2] + s3 * hr[k + 3];
		}
		for (; k < taps; k++) {
			int past = (int)i - (int)k;
			float s = (past >= 0) ? in[past] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + past];
			acc_l += s * hl[k];
			acc_r += s * hr[k];
		}

		out_l[i] = ((1.0f - blend) * in_val + blend * acc_l) * vol;
		out_r[i] = ((1.0f - blend) * in_val + blend * acc_r) * vol;
	}

	if (frames >= STEAMAUDIO_MAX_HRIR_TAPS) {
		memcpy(hrir->history, &in[frames - STEAMAUDIO_MAX_HRIR_TAPS],
		       STEAMAUDIO_MAX_HRIR_TAPS * sizeof(float));
	} else {
		uint32_t keep = STEAMAUDIO_MAX_HRIR_TAPS - frames;
		memmove(hrir->history, &hrir->history[frames], keep * sizeof(float));
		memcpy(&hrir->history[keep], in, frames * sizeof(float));
	}
}

/*
 * HiFi 5 Ambisonics B-format Binaural Decode
 */
void steamaudio_dsp_ambisonics_decode_binaural_hifi5(struct steamaudio_ambisonics_state *ambi,
						     const float *in_bformat,
						     float *out_left,
						     float *out_right,
						     uint32_t frames)
{
	if (!ambi || !in_bformat || !out_left || !out_right || frames == 0)
		return;

	for (uint32_t n = 0; n < frames; n++) {
		float acc_l = 0.0f;
		float acc_r = 0.0f;

		uint32_t ch = 0;
		for (; ch + 3 < ambi->num_channels; ch += 4) {
			float s0 = in_bformat[(ch + 0) * frames + n];
			float s1 = in_bformat[(ch + 1) * frames + n];
			float s2 = in_bformat[(ch + 2) * frames + n];
			float s3 = in_bformat[(ch + 3) * frames + n];

			acc_l += s0 * ambi->decode_matrix_l[ch + 0] +
				 s1 * ambi->decode_matrix_l[ch + 1] +
				 s2 * ambi->decode_matrix_l[ch + 2] +
				 s3 * ambi->decode_matrix_l[ch + 3];

			acc_r += s0 * ambi->decode_matrix_r[ch + 0] +
				 s1 * ambi->decode_matrix_r[ch + 1] +
				 s2 * ambi->decode_matrix_r[ch + 2] +
				 s3 * ambi->decode_matrix_r[ch + 3];
		}
		for (; ch < ambi->num_channels; ch++) {
			float s = in_bformat[ch * frames + n];
			acc_l += s * ambi->decode_matrix_l[ch];
			acc_r += s * ambi->decode_matrix_r[ch];
		}

		out_left[n] = acc_l;
		out_right[n] = acc_r;
	}
}

#endif /* SOF_USE_HIFI(5, STEAMAUDIO) */
