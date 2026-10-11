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

#if SOF_USE_HIFI(4, STEAMAUDIO)

LOG_MODULE_DECLARE(steamaudio_hifi4, CONFIG_SOF_LOG_LEVEL);

/*
 * HiFi 4 Vectorized Cascaded Room Modal Resonator Biquads
 * Evaluates pairs of channels simultaneously through shared biquad coefficients.
 */
void steamaudio_dsp_room_modes_process_hifi4(struct dsp_room_modes_state *rms,
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
		/* Process stereo pairs in parallel */
		for (; c + 1 < num_channels; c += 2) {
			float *ch0 = out + c * frames;
			float *ch1 = out + (c + 1) * frames;
			float x1_0 = bq->x1[c],     x1_1 = bq->x1[c + 1];
			float x2_0 = bq->x2[c],     x2_1 = bq->x2[c + 1];
			float y1_0 = bq->y1[c],     y1_1 = bq->y1[c + 1];
			float y2_0 = bq->y2[c],     y2_1 = bq->y2[c + 1];

			for (uint32_t n = 0; n < frames; n++) {
				float x0_0 = ch0[n];
				float x0_1 = ch1[n];

				float y0_0 = b0 * x0_0 + b1 * x1_0 + b2 * x2_0 - a1 * y1_0 - a2 * y2_0;
				float y0_1 = b0 * x0_1 + b1 * x1_1 + b2 * x2_1 - a1 * y1_1 - a2 * y2_1;

				x2_0 = x1_0; x1_0 = x0_0;
				y2_0 = y1_0; y1_0 = y0_0;
				ch0[n] = y0_0;

				x2_1 = x1_1; x1_1 = x0_1;
				y2_1 = y1_1; y1_1 = y0_1;
				ch1[n] = y0_1;
			}

			bq->x1[c]     = x1_0; bq->x1[c + 1] = x1_1;
			bq->x2[c]     = x2_0; bq->x2[c + 1] = x2_1;
			bq->y1[c]     = y1_0; bq->y1[c + 1] = y1_1;
			bq->y2[c]     = y2_0; bq->y2[c + 1] = y2_1;
		}

		/* Tail single channel if odd channel count */
		for (; c < num_channels; c++) {
			float *ch_buf = out + c * frames;
			float x1 = bq->x1[c];
			float x2 = bq->x2[c];
			float y1 = bq->y1[c];
			float y2 = bq->y2[c];

			for (uint32_t n = 0; n < frames; n++) {
				float x0 = ch_buf[n];
				float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1;
				x1 = x0;
				y2 = y1;
				y1 = y0;
				ch_buf[n] = y0;
			}

			bq->x1[c] = x1;
			bq->x2[c] = x2;
			bq->y1[c] = y1;
			bq->y2[c] = y2;
		}
	}
}

/*
 * HiFi 4 Vectorized SOFA HRIR FIR Convolution
 * Unrolls 2-wide across filter taps and history buffer.
 */
void steamaudio_dsp_sofa_hrir_process_hifi4(struct dsp_sofa_hrir_state *hrir,
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
		for (; k + 1 < taps; k += 2) {
			float s0, s1;
			int past0 = (int)i - (int)k;
			int past1 = (int)i - (int)(k + 1);

			s0 = (past0 >= 0) ? in[past0] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + past0];
			s1 = (past1 >= 0) ? in[past1] : hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + past1];

			acc_l += s0 * hl[k] + s1 * hl[k + 1];
			acc_r += s0 * hr[k] + s1 * hr[k + 1];
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
 * HiFi 4 Ambisonics B-format Binaural Decode
 */
void steamaudio_dsp_ambisonics_decode_binaural_hifi4(struct steamaudio_ambisonics_state *ambi,
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

		for (uint32_t ch = 0; ch < ambi->num_channels; ch++) {
			float s = in_bformat[ch * frames + n];
			acc_l += s * ambi->decode_matrix_l[ch];
			acc_r += s * ambi->decode_matrix_r[ch];
		}

		out_left[n] = acc_l;
		out_right[n] = acc_r;
	}
}

#endif /* SOF_USE_HIFI(4, STEAMAUDIO) */
