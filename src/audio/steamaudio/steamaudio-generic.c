// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio Spatial Processing Core for SOF

#include "steamaudio.h"
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <rtos/string.h>

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)

static inline float sat_clamp(float val, float min_v, float max_v)
{
	if (val < min_v) return min_v;
	if (val > max_v) return max_v;
	return val;
}

static inline float fast_sin(float x)
{
	while (x < -PI) x += TWO_PI;
	while (x > PI) x -= TWO_PI;
	float x2 = x * x;
	return x * (1.0f - x2 * (0.16666667f - x2 * 0.00833333f));
}

static inline float fast_cos(float x)
{
	return fast_sin(x + 0.5f * PI);
}

static inline float fast_atan2(float y, float x)
{
	if (x == 0.0f)
		return (y > 0.0f) ? (0.5f * PI) : ((y < 0.0f) ? (-0.5f * PI) : 0.0f);

	float abs_y = (y < 0.0f) ? -y : y;
	float abs_x = (x < 0.0f) ? -x : x;
	float a = (abs_x > abs_y) ? (abs_y / abs_x) : (abs_x / abs_y);
	float s = a * a;
	float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;

	if (abs_y > abs_x)
		r = 1.570796327f - r;
	if (x < 0.0f)
		r = PI - r;
	if (y < 0.0f)
		r = -r;

	return r;
}

/* 3-Band Biquad Filter Computation */
static void calc_biquad_coeffs(float gain_db, float freq, float sample_rate, int type, float coeffs[5])
{
	float w0 = 2.0f * PI * freq / sample_rate;
	float cos_w0 = fast_cos(w0);
	float sin_w0 = fast_sin(w0);
	float a = 1.0f; /* Q = 1.0 */
	float alpha = sin_w0 / (2.0f * a);
	float A = 1.0f;
	if (gain_db != 0.0f)
		A = 1.0f + gain_db * 0.115129f; /* 10^(dB/20) approx */

	float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

	if (type == 0) {
		/* Low shelf (400 Hz) */
		float sqrt_A = (A > 0.0f) ? (1.0f + 0.5f * (A - 1.0f)) : 1.0f;
		b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrt_A * alpha);
		b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
		b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrt_A * alpha);
		a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrt_A * alpha;
		a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
		a2 = (A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrt_A * alpha;
	} else if (type == 1) {
		/* Peaking (2.5 kHz) */
		b0 = 1.0f + alpha * A;
		b1 = -2.0f * cos_w0;
		b2 = 1.0f - alpha * A;
		a0 = 1.0f + alpha / A;
		a1 = -2.0f * cos_w0;
		a2 = 1.0f - alpha / A;
	} else {
		/* High shelf (15 kHz) */
		float sqrt_A = (A > 0.0f) ? (1.0f + 0.5f * (A - 1.0f)) : 1.0f;
		b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrt_A * alpha);
		b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
		b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrt_A * alpha);
		a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrt_A * alpha;
		a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
		a2 = (A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrt_A * alpha;
	}

	float inv_a0 = 1.0f / a0;
	coeffs[0] = b0 * inv_a0;
	coeffs[1] = b1 * inv_a0;
	coeffs[2] = b2 * inv_a0;
	coeffs[3] = a1 * inv_a0;
	coeffs[4] = a2 * inv_a0;
}

void steamaudio_dsp_init(struct steamaudio_comp_data *cd, uint32_t sample_rate)
{
	cd->sample_rate = sample_rate ? sample_rate : 48000;
	cd->enable = true;
	cd->bitstream_mode = false;

	/* Initialize Direct Path */
	cd->direct.active_slot = 0;
	cd->direct.current_gain = 1.0f;
	cd->direct.target_gain = 1.0f;
	cd->direct.gain_step = 0.0f;
	cd->direct.needs_crossfade = false;
	cd->direct.crossfade_remaining = 0;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float freq = (b == 0) ? 400.0f : ((b == 1) ? 2500.0f : 15000.0f);
		calc_biquad_coeffs(0.0f, freq, (float)cd->sample_rate, b, cd->direct.coeffs[0][b]);
		calc_biquad_coeffs(0.0f, freq, (float)cd->sample_rate, b, cd->direct.coeffs[1][b]);
	}

	/* Initialize Binaural */
	memset(cd->binaural.delay_line, 0, sizeof(cd->binaural.delay_line));
	cd->binaural.write_idx = 0;
	cd->binaural.spatial_blend = 1.0f;
	cd->binaural.direction[0] = 0.0f;
	cd->binaural.direction[1] = 0.0f;
	cd->binaural.direction[2] = 1.0f;
	cd->binaural.itd_samples[0] = 0.0f;
	cd->binaural.itd_samples[1] = 0.0f;
	cd->binaural.ild_gains[0] = 1.0f;
	cd->binaural.ild_gains[1] = 1.0f;

	/* Initialize FDN Reverb with prime delays */
	static const int prime_delays[STEAMAUDIO_NUM_FDN_LINES] = {
		1087, 1223, 1361, 1489, 1619, 1753, 1877, 2017
	};
	for (int i = 0; i < STEAMAUDIO_NUM_FDN_LINES; i++) {
		cd->reverb.delay_lengths[i] = prime_delays[i];
		cd->reverb.delay_indices[i] = 0;
		cd->reverb.absorption[i] = 0.25f;
		cd->reverb.damp_states[i] = 0.0f;
	}
	cd->reverb.wet_gain = 0.25f;

	/* Initialize Ambisonics */
	cd->ambisonics.order = 1;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++)
			cd->ambisonics.rotation[i][j] = (i == j) ? 1.0f : 0.0f;
	}

	/* Virtual loudspeaker layout (8 cube vertices) */
	static const float speaker_pos[8][2] = {
		{ -0.785f, -0.615f }, {  0.785f, -0.615f },
		{ -2.356f, -0.615f }, {  2.356f, -0.615f },
		{ -0.785f,  0.615f }, {  0.785f,  0.615f },
		{ -2.356f,  0.615f }, {  2.356f,  0.615f }
	};
	memcpy(cd->ambisonics.virtual_speaker_angles, speaker_pos, sizeof(speaker_pos));

	/* Initialize BVH scene with standard test room (8m x 10m x 3.5m) */
	steamaudio_dsp_scene_init_box_room(&cd->scene, 8.0f, 10.0f, 3.5f);
}

static inline void process_direct_path(struct steamaudio_comp_data *cd, const float *in, float *out, uint32_t frames)
{
	int slot = cd->direct.active_slot;
	float gain = cd->direct.current_gain;
	float step = cd->direct.gain_step;

	for (uint32_t i = 0; i < frames; i++) {
		float x = in[i];

		/* 3-Band Biquad Direct Form II Transposed */
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			float *c = cd->direct.coeffs[slot][b];
			float *s = cd->direct.states[slot][b];
			float y = c[0] * x + s[0];
			s[0] = c[1] * x - c[3] * y + s[1];
			s[1] = c[2] * x - c[4] * y;
			x = y;
		}

		/* Apply linear interpolated gain */
		gain += step;
		out[i] = x * gain;
	}

	cd->direct.current_gain = gain;
	if ((step > 0.0f && gain >= cd->direct.target_gain) ||
	    (step < 0.0f && gain <= cd->direct.target_gain)) {
		cd->direct.current_gain = cd->direct.target_gain;
		cd->direct.gain_step = 0.0f;
	}
}

static inline void process_binaural(struct steamaudio_comp_data *cd, const float *in,
				    float *out_l, float *out_r, uint32_t frames)
{
	float x = cd->binaural.direction[0];
	float z = cd->binaural.direction[2];

	float azimuth = fast_atan2(x, z);
	float sin_az = fast_sin(azimuth);

	/* ITD: Woodworth spherical model (~0.66ms max delay = ~32 samples at 48kHz) */
	float max_itd_samples = 32.0f * (float)cd->sample_rate / 48000.0f;
	float itd_l = (sin_az < 0.0f) ? 0.0f : (sin_az * max_itd_samples);
	float itd_r = (sin_az > 0.0f) ? 0.0f : (-sin_az * max_itd_samples);

	/* ILD: head-shadow attenuation */
	float ild_l = (sin_az > 0.0f) ? (1.0f - 0.5f * sin_az) : 1.0f;
	float ild_r = (sin_az < 0.0f) ? (1.0f + 0.5f * sin_az) : 1.0f;

	float blend = cd->binaural.spatial_blend;

	for (uint32_t i = 0; i < frames; i++) {
		cd->binaural.delay_line[cd->binaural.write_idx] = in[i];

		/* Delay left ear */
		int read_idx_l = cd->binaural.write_idx - (int)itd_l;
		if (read_idx_l < 0) read_idx_l += STEAMAUDIO_DELAY_LINE_SIZE;
		float left_sample = cd->binaural.delay_line[read_idx_l] * ild_l;

		/* Delay right ear */
		int read_idx_r = cd->binaural.write_idx - (int)itd_r;
		if (read_idx_r < 0) read_idx_r += STEAMAUDIO_DELAY_LINE_SIZE;
		float right_sample = cd->binaural.delay_line[read_idx_r] * ild_r;

		cd->binaural.write_idx = (cd->binaural.write_idx + 1) % STEAMAUDIO_DELAY_LINE_SIZE;

		/* Blend between mono and spatial */
		out_l[i] = in[i] * (1.0f - blend) + left_sample * blend;
		out_r[i] = in[i] * (1.0f - blend) + right_sample * blend;
	}
}

static inline void process_reverb(struct steamaudio_comp_data *cd, const float *in,
				  float *out_l, float *out_r, uint32_t frames)
{
	float wet = cd->reverb.wet_gain;
	if (wet < 1e-4f)
		return;

	float fdn_outputs[STEAMAUDIO_NUM_FDN_LINES];

	for (uint32_t i = 0; i < frames; i++) {
		float in_val = in[i];
		float sum_fdn = 0.0f;

		/* Read delay lines and compute Householder sum */
		for (int j = 0; j < STEAMAUDIO_NUM_FDN_LINES; j++) {
			int ptr = cd->reverb.delay_indices[j];
			float val = cd->reverb.delay_buffers[j][ptr];
			/* 1-pole absorption filter */
			float alpha = cd->reverb.absorption[j];
			val = val * (1.0f - alpha) + cd->reverb.damp_states[j] * alpha;
			cd->reverb.damp_states[j] = val;
			fdn_outputs[j] = val;
			sum_fdn += val;
		}

		/* Householder 8x8 reflection: out_k = input - 2/8 * sum */
		float feedback_factor = sum_fdn * 0.25f;
		for (int j = 0; j < STEAMAUDIO_NUM_FDN_LINES; j++) {
			float new_val = in_val + (fdn_outputs[j] - feedback_factor);
			int ptr = cd->reverb.delay_indices[j];
			cd->reverb.delay_buffers[j][ptr] = new_val;
			cd->reverb.delay_indices[j] = (ptr + 1) % cd->reverb.delay_lengths[j];
		}

		/* Decorrelated stereo reverb sum */
		float rev_l = (fdn_outputs[0] + fdn_outputs[2] + fdn_outputs[4] + fdn_outputs[6]) * 0.25f;
		float rev_r = (fdn_outputs[1] + fdn_outputs[3] + fdn_outputs[5] + fdn_outputs[7]) * 0.25f;

		out_l[i] += rev_l * wet;
		out_r[i] += rev_r * wet;
	}
}

static void check_inband_bitstream(struct steamaudio_comp_data *cd, const void *src_ptr, uint32_t avail_bytes)
{
	if (avail_bytes < sizeof(struct steamaudio_bitstream_header))
		return;

	const struct steamaudio_bitstream_header *hdr = (const struct steamaudio_bitstream_header *)src_ptr;
	if (hdr->sync_word != STEAMAUDIO_SOF_SYNC_WORD)
		return;

	/* Valid synchronized bitstream frame detected */
	cd->bitstream_mode = true;
	cd->direct.target_gain = hdr->distance_attenuation * (1.0f - hdr->occlusion);
	cd->direct.gain_step = (cd->direct.target_gain - cd->direct.current_gain) / (float)hdr->num_samples;

	cd->binaural.direction[0] = hdr->direction[0];
	cd->binaural.direction[1] = hdr->direction[1];
	cd->binaural.direction[2] = hdr->direction[2];
	cd->binaural.spatial_blend = hdr->spatial_blend;

	cd->reverb.wet_gain = hdr->reverb_wet_gain;
}

#if CONFIG_FORMAT_S16LE
static int steamaudio_process_s16(struct processing_module *mod,
				  struct sof_source *source,
				  struct sof_sink *sink,
				  uint32_t frames)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);
	int16_t const *src, *x_start;
	int16_t *dst, *y_start;
	int x_size, y_size;
	uint32_t in_bytes = frames * cd->channels * sizeof(int16_t);
	uint32_t out_bytes = frames * 2 * sizeof(int16_t);
	int ret;

	ret = source_get_data_s16(source, in_bytes, &src, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, out_bytes, &dst, &y_start, &y_size);
	if (ret)
		return ret;

	check_inband_bitstream(cd, src, in_bytes);

	/* Convert mono/first-channel input to float */
	for (uint32_t i = 0; i < frames; i++)
		cd->in_scratch[i] = (float)src[i * cd->channels] * (1.0f / 32768.0f);

	/* DSP Pipeline */
	process_direct_path(cd, cd->in_scratch, cd->out_left, frames);
	memcpy(cd->in_scratch, cd->out_left, frames * sizeof(float));

	process_binaural(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);

	/* Interleave stereo float into S16_LE */
	for (uint32_t i = 0; i < frames; i++) {
		float l = sat_clamp(cd->out_left[i] * 32767.0f, -32768.0f, 32767.0f);
		float r = sat_clamp(cd->out_right[i] * 32767.0f, -32768.0f, 32767.0f);
		dst[2 * i] = (int16_t)l;
		dst[2 * i + 1] = (int16_t)r;
	}

	source_release_data(source, in_bytes);
	sink_commit_buffer(sink, out_bytes);
	return 0;
}
#endif

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
static int steamaudio_process_s32(struct processing_module *mod,
				  struct sof_source *source,
				  struct sof_sink *sink,
				  uint32_t frames)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);
	int32_t const *src, *x_start;
	int32_t *dst, *y_start;
	int x_size, y_size;
	uint32_t in_bytes = frames * cd->channels * sizeof(int32_t);
	uint32_t out_bytes = frames * 2 * sizeof(int32_t);
	int ret;

	ret = source_get_data_s32(source, in_bytes, &src, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, out_bytes, &dst, &y_start, &y_size);
	if (ret)
		return ret;

	check_inband_bitstream(cd, src, in_bytes);

	for (uint32_t i = 0; i < frames; i++)
		cd->in_scratch[i] = (float)src[i * cd->channels] * (1.0f / 2147483648.0f);

	process_direct_path(cd, cd->in_scratch, cd->out_left, frames);
	memcpy(cd->in_scratch, cd->out_left, frames * sizeof(float));

	process_binaural(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);

	for (uint32_t i = 0; i < frames; i++) {
		float l = sat_clamp(cd->out_left[i] * 2147483647.0f, -2147483648.0f, 2147483647.0f);
		float r = sat_clamp(cd->out_right[i] * 2147483647.0f, -2147483648.0f, 2147483647.0f);
		dst[2 * i] = (int32_t)l;
		dst[2 * i + 1] = (int32_t)r;
	}

	source_release_data(source, in_bytes);
	sink_commit_buffer(sink, out_bytes);
	return 0;
}
#endif

steamaudio_func steamaudio_find_proc_func(enum sof_ipc_frame src_fmt)
{
	switch (src_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		return steamaudio_process_s16;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		return steamaudio_process_s32;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		return steamaudio_process_s32;
#endif
	default:
		return NULL;
	}
}
