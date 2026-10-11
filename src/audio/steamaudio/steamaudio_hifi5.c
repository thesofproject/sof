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

#endif /* SOF_USE_HIFI(5, STEAMAUDIO) */
