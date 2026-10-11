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
 * Uses contiguous signal buffer and 4-way float parallel accumulation across 4 frames
 * for maximum FMA throughput and zero branch penalties.
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

	/* Linear contiguous history + input buffer */
	float sig_buf[STEAMAUDIO_MAX_HRIR_TAPS + 256];
	memcpy(sig_buf, hrir->history, STEAMAUDIO_MAX_HRIR_TAPS * sizeof(float));
	memcpy(sig_buf + STEAMAUDIO_MAX_HRIR_TAPS, in, frames * sizeof(float));

	uint32_t i = 0;
	/* Process 4 frames in parallel */
	for (; i + 3 < frames; i += 4) {
		float in_val0 = in[i + 0], in_val1 = in[i + 1];
		float in_val2 = in[i + 2], in_val3 = in[i + 3];

		const float *s0 = &sig_buf[STEAMAUDIO_MAX_HRIR_TAPS + i + 0];
		const float *s1 = &sig_buf[STEAMAUDIO_MAX_HRIR_TAPS + i + 1];
		const float *s2 = &sig_buf[STEAMAUDIO_MAX_HRIR_TAPS + i + 2];
		const float *s3 = &sig_buf[STEAMAUDIO_MAX_HRIR_TAPS + i + 3];

		float al0 = 0.0f, ar0 = 0.0f;
		float al1 = 0.0f, ar1 = 0.0f;
		float al2 = 0.0f, ar2 = 0.0f;
		float al3 = 0.0f, ar3 = 0.0f;

		uint32_t k = 0;
		for (; k + 3 < taps; k += 4) {
			float tl0 = hl[k + 0], tl1 = hl[k + 1], tl2 = hl[k + 2], tl3 = hl[k + 3];
			float tr0 = hr[k + 0], tr1 = hr[k + 1], tr2 = hr[k + 2], tr3 = hr[k + 3];

			float x0_0 = s0[-((int)k + 0)], x0_1 = s0[-((int)k + 1)];
			float x0_2 = s0[-((int)k + 2)], x0_3 = s0[-((int)k + 3)];

			float x1_0 = s1[-((int)k + 0)], x1_1 = s1[-((int)k + 1)];
			float x1_2 = s1[-((int)k + 2)], x1_3 = s1[-((int)k + 3)];

			float x2_0 = s2[-((int)k + 0)], x2_1 = s2[-((int)k + 1)];
			float x2_2 = s2[-((int)k + 2)], x2_3 = s2[-((int)k + 3)];

			float x3_0 = s3[-((int)k + 0)], x3_1 = s3[-((int)k + 1)];
			float x3_2 = s3[-((int)k + 2)], x3_3 = s3[-((int)k + 3)];

			al0 += x0_0 * tl0 + x0_1 * tl1 + x0_2 * tl2 + x0_3 * tl3;
			ar0 += x0_0 * tr0 + x0_1 * tr1 + x0_2 * tr2 + x0_3 * tr3;

			al1 += x1_0 * tl0 + x1_1 * tl1 + x1_2 * tl2 + x1_3 * tl3;
			ar1 += x1_0 * tr0 + x1_1 * tr1 + x1_2 * tr2 + x1_3 * tr3;

			al2 += x2_0 * tl0 + x2_1 * tl1 + x2_2 * tl2 + x2_3 * tl3;
			ar2 += x2_0 * tr0 + x2_1 * tr1 + x2_2 * tr2 + x2_3 * tr3;

			al3 += x3_0 * tl0 + x3_1 * tl1 + x3_2 * tl2 + x3_3 * tl3;
			ar3 += x3_0 * tr0 + x3_1 * tr1 + x3_2 * tr2 + x3_3 * tr3;
		}
		for (; k < taps; k++) {
			float tl = hl[k], tr = hr[k];
			al0 += s0[-((int)k)] * tl; ar0 += s0[-((int)k)] * tr;
			al1 += s1[-((int)k)] * tl; ar1 += s1[-((int)k)] * tr;
			al2 += s2[-((int)k)] * tl; ar2 += s2[-((int)k)] * tr;
			al3 += s3[-((int)k)] * tl; ar3 += s3[-((int)k)] * tr;
		}

		out_l[i + 0] = ((1.0f - blend) * in_val0 + blend * al0) * vol;
		out_r[i + 0] = ((1.0f - blend) * in_val0 + blend * ar0) * vol;

		out_l[i + 1] = ((1.0f - blend) * in_val1 + blend * al1) * vol;
		out_r[i + 1] = ((1.0f - blend) * in_val1 + blend * ar1) * vol;

		out_l[i + 2] = ((1.0f - blend) * in_val2 + blend * al2) * vol;
		out_r[i + 2] = ((1.0f - blend) * in_val2 + blend * ar2) * vol;

		out_l[i + 3] = ((1.0f - blend) * in_val3 + blend * al3) * vol;
		out_r[i + 3] = ((1.0f - blend) * in_val3 + blend * ar3) * vol;
	}

	/* Remainder frames */
	for (; i < frames; i++) {
		float in_val = in[i];
		const float *s = &sig_buf[STEAMAUDIO_MAX_HRIR_TAPS + i];
		float al = 0.0f, ar = 0.0f;

		uint32_t k = 0;
		for (; k + 3 < taps; k += 4) {
			float tl0 = hl[k + 0], tl1 = hl[k + 1], tl2 = hl[k + 2], tl3 = hl[k + 3];
			float tr0 = hr[k + 0], tr1 = hr[k + 1], tr2 = hr[k + 2], tr3 = hr[k + 3];
			float x0 = s[-((int)k + 0)], x1 = s[-((int)k + 1)];
			float x2 = s[-((int)k + 2)], x3 = s[-((int)k + 3)];

			al += x0 * tl0 + x1 * tl1 + x2 * tl2 + x3 * tl3;
			ar += x0 * tr0 + x1 * tr1 + x2 * tr2 + x3 * tr3;
		}
		for (; k < taps; k++) {
			al += s[-((int)k)] * hl[k];
			ar += s[-((int)k)] * hr[k];
		}

		out_l[i] = ((1.0f - blend) * in_val + blend * al) * vol;
		out_r[i] = ((1.0f - blend) * in_val + blend * ar) * vol;
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
 * HiFi 5 Quad-Vectorized Ambisonics 3D Rotation
 * Rotates Order 1 soundfield dipoles using 4-way parallel matrix-vector kernels.
 */
void steamaudio_dsp_ambisonics_rotate_hifi5(uint32_t order, const float rot[3][3],
					    const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					    float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					    uint32_t frames)
{
	int num_ch = (order + 1) * (order + 1);
	if (num_ch > STEAMAUDIO_MAX_HOA_CHANNELS)
		num_ch = STEAMAUDIO_MAX_HOA_CHANNELS;

	/* Channel 0: W (omni invariant to rotation) */
	memcpy(out_ch[0], in_ch[0], frames * sizeof(float));

	if (num_ch >= 4) {
		const float r00 = rot[0][0], r01 = rot[0][1], r02 = rot[0][2];
		const float r10 = rot[1][0], r11 = rot[1][1], r12 = rot[1][2];
		const float r20 = rot[2][0], r21 = rot[2][1], r22 = rot[2][2];

		uint32_t i = 0;
		for (; i + 3 < frames; i += 4) {
			float y0 = in_ch[1][i + 0], y1 = in_ch[1][i + 1];
			float y2 = in_ch[1][i + 2], y3 = in_ch[1][i + 3];

			float z0 = in_ch[2][i + 0], z1 = in_ch[2][i + 1];
			float z2 = in_ch[2][i + 2], z3 = in_ch[2][i + 3];

			float x0 = in_ch[3][i + 0], x1 = in_ch[3][i + 1];
			float x2 = in_ch[3][i + 2], x3 = in_ch[3][i + 3];

			out_ch[1][i + 0] = y0 * r00 + z0 * r01 + x0 * r02;
			out_ch[2][i + 0] = y0 * r10 + z0 * r11 + x0 * r12;
			out_ch[3][i + 0] = y0 * r20 + z0 * r21 + x0 * r22;

			out_ch[1][i + 1] = y1 * r00 + z1 * r01 + x1 * r02;
			out_ch[2][i + 1] = y1 * r10 + z1 * r11 + x1 * r12;
			out_ch[3][i + 1] = y1 * r20 + z1 * r21 + x1 * r22;

			out_ch[1][i + 2] = y2 * r00 + z2 * r01 + x2 * r02;
			out_ch[2][i + 2] = y2 * r10 + z2 * r11 + x2 * r12;
			out_ch[3][i + 2] = y2 * r20 + z2 * r21 + x2 * r22;

			out_ch[1][i + 3] = y3 * r00 + z3 * r01 + x3 * r02;
			out_ch[2][i + 3] = y3 * r10 + z3 * r11 + x3 * r12;
			out_ch[3][i + 3] = y3 * r20 + z3 * r21 + x3 * r22;
		}
		for (; i < frames; i++) {
			float y = in_ch[1][i];
			float z = in_ch[2][i];
			float x = in_ch[3][i];

			out_ch[1][i] = y * r00 + z * r01 + x * r02;
			out_ch[2][i] = y * r10 + z * r11 + x * r12;
			out_ch[3][i] = y * r20 + z * r21 + x * r22;
		}
	}

	for (int ch = 4; ch < num_ch; ch++)
		memcpy(out_ch[ch], in_ch[ch], frames * sizeof(float));
}

/*
 * HiFi 5 Quad-Vectorized Ambisonics B-format Binaural Decode
 * Precomputes composite binaural decoding weights across the 8 cube virtual speakers,
 * reducing per-sample multiply-accumulate operations by 8x, and evaluates 4 stereo frames
 * in parallel with 4-way channel unrolled inner products.
 */
void steamaudio_dsp_ambisonics_decode_binaural_hifi5(struct steamaudio_ambisonics_state *ambi,
						     const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
						     float *out_left,
						     float *out_right,
						     uint32_t frames)
{
	static const float cube_v[8][3] = {
		{ -0.57735f, -0.57735f, -0.57735f },
		{  0.57735f, -0.57735f, -0.57735f },
		{ -0.57735f,  0.57735f, -0.57735f },
		{  0.57735f,  0.57735f, -0.57735f },
		{ -0.57735f, -0.57735f,  0.57735f },
		{  0.57735f, -0.57735f,  0.57735f },
		{ -0.57735f,  0.57735f,  0.57735f },
		{  0.57735f,  0.57735f,  0.57735f }
	};

	if (!ambi || !in_ch || !out_left || !out_right || frames == 0)
		return;

	int num_ch = (ambi->order + 1) * (ambi->order + 1);
	if (num_ch > 16)
		num_ch = 16;

	float weight_l[STEAMAUDIO_MAX_HOA_CHANNELS] = { 0.0f };
	float weight_r[STEAMAUDIO_MAX_HOA_CHANNELS] = { 0.0f };

	/* Precompute composite binaural weights across 8 virtual cube speakers */
	for (int k = 0; k < 8; k++) {
		float vx = cube_v[k][0], vy = cube_v[k][1], vz = cube_v[k][2];
		float rx = ambi->rotation[0][0] * vx + ambi->rotation[0][1] * vy + ambi->rotation[0][2] * vz;
		float ry = ambi->rotation[1][0] * vx + ambi->rotation[1][1] * vy + ambi->rotation[1][2] * vz;
		float rz = ambi->rotation[2][0] * vx + ambi->rotation[2][1] * vy + ambi->rotation[2][2] * vz;

		float sh[16];
		eval_sh_basis(rx, ry, rz, (int)ambi->order, sh);

		float az = fast_atan2(vx, vz);
		float sin_az = fast_sin(az);
		float ild_l = (sin_az > 0.0f) ? (1.0f - 0.4f * sin_az) : 1.0f;
		float ild_r = (sin_az < 0.0f) ? (1.0f + 0.4f * sin_az) : 1.0f;

		float factor_l = 0.125f * ild_l;
		float factor_r = 0.125f * ild_r;

		for (int ch = 0; ch < num_ch; ch++) {
			weight_l[ch] += factor_l * sh[ch];
			weight_r[ch] += factor_r * sh[ch];
		}
	}

	uint32_t i = 0;
	/* Process 4 stereo frames in parallel */
	for (; i + 3 < frames; i += 4) {
		float al0 = 0.0f, ar0 = 0.0f;
		float al1 = 0.0f, ar1 = 0.0f;
		float al2 = 0.0f, ar2 = 0.0f;
		float al3 = 0.0f, ar3 = 0.0f;

		int ch = 0;
		for (; ch + 3 < num_ch; ch += 4) {
			float s0_0 = in_ch[ch + 0][i + 0], s1_0 = in_ch[ch + 0][i + 1];
			float s2_0 = in_ch[ch + 0][i + 2], s3_0 = in_ch[ch + 0][i + 3];

			float s0_1 = in_ch[ch + 1][i + 0], s1_1 = in_ch[ch + 1][i + 1];
			float s2_1 = in_ch[ch + 1][i + 2], s3_1 = in_ch[ch + 1][i + 3];

			float s0_2 = in_ch[ch + 2][i + 0], s1_2 = in_ch[ch + 2][i + 1];
			float s2_2 = in_ch[ch + 2][i + 2], s3_2 = in_ch[ch + 2][i + 3];

			float s0_3 = in_ch[ch + 3][i + 0], s1_3 = in_ch[ch + 3][i + 1];
			float s2_3 = in_ch[ch + 3][i + 2], s3_3 = in_ch[ch + 3][i + 3];

			float wl0 = weight_l[ch + 0], wl1 = weight_l[ch + 1];
			float wl2 = weight_l[ch + 2], wl3 = weight_l[ch + 3];

			float wr0 = weight_r[ch + 0], wr1 = weight_r[ch + 1];
			float wr2 = weight_r[ch + 2], wr3 = weight_r[ch + 3];

			al0 += s0_0 * wl0 + s0_1 * wl1 + s0_2 * wl2 + s0_3 * wl3;
			ar0 += s0_0 * wr0 + s0_1 * wr1 + s0_2 * wr2 + s0_3 * wr3;

			al1 += s1_0 * wl0 + s1_1 * wl1 + s1_2 * wl2 + s1_3 * wl3;
			ar1 += s1_0 * wr0 + s1_1 * wr1 + s1_2 * wr2 + s1_3 * wr3;

			al2 += s2_0 * wl0 + s2_1 * wl1 + s2_2 * wl2 + s2_3 * wl3;
			ar2 += s2_0 * wr0 + s2_1 * wr1 + s2_2 * wr2 + s2_3 * wr3;

			al3 += s3_0 * wl0 + s3_1 * wl1 + s3_2 * wl2 + s3_3 * wl3;
			ar3 += s3_0 * wr0 + s3_1 * wr1 + s3_2 * wr2 + s3_3 * wr3;
		}
		for (; ch < num_ch; ch++) {
			float wl = weight_l[ch], wr = weight_r[ch];

			al0 += in_ch[ch][i + 0] * wl; ar0 += in_ch[ch][i + 0] * wr;
			al1 += in_ch[ch][i + 1] * wl; ar1 += in_ch[ch][i + 1] * wr;
			al2 += in_ch[ch][i + 2] * wl; ar2 += in_ch[ch][i + 2] * wr;
			al3 += in_ch[ch][i + 3] * wl; ar3 += in_ch[ch][i + 3] * wr;
		}

		out_left[i + 0]  = al0; out_right[i + 0] = ar0;
		out_left[i + 1]  = al1; out_right[i + 1] = ar1;
		out_left[i + 2]  = al2; out_right[i + 2] = ar2;
		out_left[i + 3]  = al3; out_right[i + 3] = ar3;
	}

	for (; i < frames; i++) {
		float acc_l = 0.0f, acc_r = 0.0f;
		int ch = 0;
		for (; ch + 3 < num_ch; ch += 4) {
			acc_l += in_ch[ch + 0][i] * weight_l[ch + 0] +
				 in_ch[ch + 1][i] * weight_l[ch + 1] +
				 in_ch[ch + 2][i] * weight_l[ch + 2] +
				 in_ch[ch + 3][i] * weight_l[ch + 3];

			acc_r += in_ch[ch + 0][i] * weight_r[ch + 0] +
				 in_ch[ch + 1][i] * weight_r[ch + 1] +
				 in_ch[ch + 2][i] * weight_r[ch + 2] +
				 in_ch[ch + 3][i] * weight_r[ch + 3];
		}
		for (; ch < num_ch; ch++) {
			acc_l += in_ch[ch][i] * weight_l[ch];
			acc_r += in_ch[ch][i] * weight_r[ch];
		}

		out_left[i]  = acc_l;
		out_right[i] = acc_r;
	}
}

/*
 * HiFi 5 Quad-Vectorized Feedback Delay Network (FDN) Reverb
 * Holds 8 circular delay pointers in local registers across the block,
 * eliminates integer modulo operations, and unrolls 8x8 Householder matrix reflections.
 */
void steamaudio_dsp_reverb_process_hifi5(struct steamaudio_comp_data *cd,
					 const float *in,
					 float *out_l, float *out_r,
					 uint32_t frames)
{
	if (!cd || !in || !out_l || !out_r || frames == 0)
		return;

	if (cd->mute_mask & ((1u << STEAMAUDIO_STEP_REVERB) | (1u << STEAMAUDIO_STEP_CONVOLUTION)))
		return;

	float wet = cd->reverb.wet_gain;
	if (wet < 1e-4f)
		return;

	/* Cache delay line pointers, lengths, and filter states in local registers */
	int ptr[8];
	int len[8];
	float alpha[8];
	float damp[8];

	for (int j = 0; j < 8; j++) {
		ptr[j] = cd->reverb.delay_indices[j];
		len[j] = cd->reverb.delay_lengths[j];
		alpha[j] = cd->reverb.absorption[j];
		damp[j] = cd->reverb.damp_states[j];
	}

	for (uint32_t i = 0; i < frames; i++) {
		float in_val = in[i];
		float v0 = cd->reverb.delay_buffers[0][ptr[0]];
		float v1 = cd->reverb.delay_buffers[1][ptr[1]];
		float v2 = cd->reverb.delay_buffers[2][ptr[2]];
		float v3 = cd->reverb.delay_buffers[3][ptr[3]];
		float v4 = cd->reverb.delay_buffers[4][ptr[4]];
		float v5 = cd->reverb.delay_buffers[5][ptr[5]];
		float v6 = cd->reverb.delay_buffers[6][ptr[6]];
		float v7 = cd->reverb.delay_buffers[7][ptr[7]];

		/* 1-pole absorption filter */
		v0 = v0 * (1.0f - alpha[0]) + damp[0] * alpha[0]; damp[0] = v0;
		v1 = v1 * (1.0f - alpha[1]) + damp[1] * alpha[1]; damp[1] = v1;
		v2 = v2 * (1.0f - alpha[2]) + damp[2] * alpha[2]; damp[2] = v2;
		v3 = v3 * (1.0f - alpha[3]) + damp[3] * alpha[3]; damp[3] = v3;
		v4 = v4 * (1.0f - alpha[4]) + damp[4] * alpha[4]; damp[4] = v4;
		v5 = v5 * (1.0f - alpha[5]) + damp[5] * alpha[5]; damp[5] = v5;
		v6 = v6 * (1.0f - alpha[6]) + damp[6] * alpha[6]; damp[6] = v6;
		v7 = v7 * (1.0f - alpha[7]) + damp[7] * alpha[7]; damp[7] = v7;

		/* Householder 8x8 reflection: out_k = input - 2/8 * sum */
		float sum_fdn = (v0 + v1 + v2 + v3) + (v4 + v5 + v6 + v7);
		float feedback_factor = sum_fdn * 0.25f;

		cd->reverb.delay_buffers[0][ptr[0]] = in_val + (v0 - feedback_factor);
		cd->reverb.delay_buffers[1][ptr[1]] = in_val + (v1 - feedback_factor);
		cd->reverb.delay_buffers[2][ptr[2]] = in_val + (v2 - feedback_factor);
		cd->reverb.delay_buffers[3][ptr[3]] = in_val + (v3 - feedback_factor);
		cd->reverb.delay_buffers[4][ptr[4]] = in_val + (v4 - feedback_factor);
		cd->reverb.delay_buffers[5][ptr[5]] = in_val + (v5 - feedback_factor);
		cd->reverb.delay_buffers[6][ptr[6]] = in_val + (v6 - feedback_factor);
		cd->reverb.delay_buffers[7][ptr[7]] = in_val + (v7 - feedback_factor);

		/* Branchless circular index advancement */
		ptr[0] = (ptr[0] + 1 < len[0]) ? ptr[0] + 1 : 0;
		ptr[1] = (ptr[1] + 1 < len[1]) ? ptr[1] + 1 : 0;
		ptr[2] = (ptr[2] + 1 < len[2]) ? ptr[2] + 1 : 0;
		ptr[3] = (ptr[3] + 1 < len[3]) ? ptr[3] + 1 : 0;
		ptr[4] = (ptr[4] + 1 < len[4]) ? ptr[4] + 1 : 0;
		ptr[5] = (ptr[5] + 1 < len[5]) ? ptr[5] + 1 : 0;
		ptr[6] = (ptr[6] + 1 < len[6]) ? ptr[6] + 1 : 0;
		ptr[7] = (ptr[7] + 1 < len[7]) ? ptr[7] + 1 : 0;

		/* Decorrelated stereo reverb sum */
		float rev_l = (v0 + v2 + v4 + v6) * 0.25f;
		float rev_r = (v1 + v3 + v5 + v7) * 0.25f;

		out_l[i] += rev_l * wet;
		out_r[i] += rev_r * wet;
	}

	/* Store back updated pointers and states */
	for (int j = 0; j < 8; j++) {
		cd->reverb.delay_indices[j] = ptr[j];
		cd->reverb.damp_states[j] = damp[j];
	}
}

/*
 * HiFi 5 Quad-Vectorized Early Reflections Synthesis
 * Precomputes tap delay parameters outside the frame loop and processes
 * circular delay line interpolation with 4-way channel unrolled accumulation.
 */
void steamaudio_dsp_early_reflections_process_hifi5(struct dsp_early_reflections_state *ers,
						    const float *in,
						    float out[STEAMAUDIO_MAX_MIXER_CHANNELS][256],
						    uint32_t frames,
						    bool muted)
{
	if (!ers || !out)
		return;

	uint32_t channels = ers->num_channels;
	if (channels > STEAMAUDIO_MAX_MIXER_CHANNELS)
		channels = STEAMAUDIO_MAX_MIXER_CHANNELS;
	if (channels == 0)
		channels = 2;

	if (frames > 256)
		frames = 256;

	if (muted || !ers->enabled) {
		for (uint32_t ch = 0; ch < channels; ch++)
			memset(out[ch], 0, frames * sizeof(float));
		return;
	}

	for (uint32_t ch = 0; ch < channels; ch++)
		memset(out[ch], 0, frames * sizeof(float));

	if (!in || ers->num_taps == 0)
		return;

	const uint32_t mask = STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - 1;
	float fs = ers->sample_rate > 0.0f ? ers->sample_rate : 48000.0f;

	/* Precalculate per-tap delay offsets and interpolation weights */
	uint32_t tap_k[STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS];
	float tap_frac[STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS];
	float tap_gain[STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS][STEAMAUDIO_MAX_MIXER_CHANNELS];
	bool tap_active[STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS];

	uint32_t num_taps = ers->num_taps;
	if (num_taps > STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS)
		num_taps = STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS;

	for (uint32_t t = 0; t < num_taps; t++) {
		tap_active[t] = ers->taps[t].active;
		if (!tap_active[t])
			continue;

		float d = ers->taps[t].delay_ms * 0.001f * fs;
		if (d < 0.0f) d = 0.0f;
		if (d > (float)(STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - 2))
			d = (float)(STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - 2);

		uint32_t k = (uint32_t)d;
		tap_k[t] = k;
		tap_frac[t] = d - (float)k;

		for (uint32_t ch = 0; ch < channels; ch++)
			tap_gain[t][ch] = ers->taps[t].gain[ch];
	}

	uint32_t write_pos = ers->write_pos;

	for (uint32_t i = 0; i < frames; i++) {
		ers->delay_line[write_pos] = in[i];

		for (uint32_t t = 0; t < num_taps; t++) {
			if (!tap_active[t])
				continue;

			uint32_t k = tap_k[t];
			float frac = tap_frac[t];

			uint32_t idx0 = (write_pos + STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - k) & mask;
			uint32_t idx1 = (write_pos + STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE - k - 1) & mask;

			float s = (1.0f - frac) * ers->delay_line[idx0] + frac * ers->delay_line[idx1];

			uint32_t ch = 0;
			for (; ch + 3 < channels; ch += 4) {
				out[ch + 0][i] += s * tap_gain[t][ch + 0];
				out[ch + 1][i] += s * tap_gain[t][ch + 1];
				out[ch + 2][i] += s * tap_gain[t][ch + 2];
				out[ch + 3][i] += s * tap_gain[t][ch + 3];
			}
			for (; ch < channels; ch++)
				out[ch][i] += s * tap_gain[t][ch];
		}

		write_pos = (write_pos + 1) & mask;
	}

	ers->write_pos = write_pos;
}

/*
 * HiFi 5 Accelerated Ray-AABB Bounding Box Intersection
 */
bool steamaudio_bvh_intersect_ray_aabb_hifi5(const struct dsp_ray *ray,
					     const struct dsp_aabb *box)
{
	float tmin = ray->min_distance;
	float tmax = ray->max_distance;

	float inv_dx = (ray->direction.x > -1e-7f && ray->direction.x < 1e-7f) ? 0.0f : 1.0f / ray->direction.x;
	float inv_dy = (ray->direction.y > -1e-7f && ray->direction.y < 1e-7f) ? 0.0f : 1.0f / ray->direction.y;
	float inv_dz = (ray->direction.z > -1e-7f && ray->direction.z < 1e-7f) ? 0.0f : 1.0f / ray->direction.z;

	/* X axis */
	if (inv_dx == 0.0f) {
		if (ray->origin.x < box->min.x || ray->origin.x > box->max.x)
			return false;
	} else {
		float t1 = (box->min.x - ray->origin.x) * inv_dx;
		float t2 = (box->max.x - ray->origin.x) * inv_dx;
		if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
		tmin = fast_fmaxf(tmin, t1);
		tmax = fast_fminf(tmax, t2);
		if (tmin > tmax) return false;
	}

	/* Y axis */
	if (inv_dy == 0.0f) {
		if (ray->origin.y < box->min.y || ray->origin.y > box->max.y)
			return false;
	} else {
		float t1 = (box->min.y - ray->origin.y) * inv_dy;
		float t2 = (box->max.y - ray->origin.y) * inv_dy;
		if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
		tmin = fast_fmaxf(tmin, t1);
		tmax = fast_fminf(tmax, t2);
		if (tmin > tmax) return false;
	}

	/* Z axis */
	if (inv_dz == 0.0f) {
		if (ray->origin.z < box->min.z || ray->origin.z > box->max.z)
			return false;
	} else {
		float t1 = (box->min.z - ray->origin.z) * inv_dz;
		float t2 = (box->max.z - ray->origin.z) * inv_dz;
		if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
		tmin = fast_fmaxf(tmin, t1);
		tmax = fast_fminf(tmax, t2);
		if (tmin > tmax) return false;
	}

	return true;
}

/*
 * HiFi 5 Vectorized Möller-Trumbore Ray-Triangle Intersection
 */
bool steamaudio_bvh_intersect_ray_triangle_hifi5(const struct dsp_ray *ray,
						 const struct dsp_triangle *tri,
						 struct dsp_hit *hit)
{
	float edge1_x = tri->v1.x - tri->v0.x;
	float edge1_y = tri->v1.y - tri->v0.y;
	float edge1_z = tri->v1.z - tri->v0.z;

	float edge2_x = tri->v2.x - tri->v0.x;
	float edge2_y = tri->v2.y - tri->v0.y;
	float edge2_z = tri->v2.z - tri->v0.z;

	/* h = ray->direction x edge2 */
	float h_x = ray->direction.y * edge2_z - ray->direction.z * edge2_y;
	float h_y = ray->direction.z * edge2_x - ray->direction.x * edge2_z;
	float h_z = ray->direction.x * edge2_y - ray->direction.y * edge2_x;

	float a = edge1_x * h_x + edge1_y * h_y + edge1_z * h_z;
	if (a > -1e-6f && a < 1e-6f)
		return false;

	float f = 1.0f / a;
	float s_x = ray->origin.x - tri->v0.x;
	float s_y = ray->origin.y - tri->v0.y;
	float s_z = ray->origin.z - tri->v0.z;

	float u = f * (s_x * h_x + s_y * h_y + s_z * h_z);
	if (u < 0.0f || u > 1.0f)
		return false;

	/* q = s x edge1 */
	float q_x = s_y * edge1_z - s_z * edge1_y;
	float q_y = s_z * edge1_x - s_x * edge1_z;
	float q_z = s_x * edge1_y - s_y * edge1_x;

	float v = f * (ray->direction.x * q_x + ray->direction.y * q_y + ray->direction.z * q_z);
	if (v < 0.0f || u + v > 1.0f)
		return false;

	float t = f * (edge2_x * q_x + edge2_y * q_y + edge2_z * q_z);
	if (t >= ray->min_distance && t <= ray->max_distance) {
		hit->distance = t;
		hit->normal = tri->normal;
		hit->has_hit = true;
		return true;
	}

	return false;
}

#endif /* SOF_USE_HIFI(5, STEAMAUDIO) */
