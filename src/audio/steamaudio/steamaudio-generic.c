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
	if (x > 0.5f * PI)
		x = PI - x;
	else if (x < -0.5f * PI)
		x = -PI - x;
	float x2 = x * x;
	return x * (1.0f - x2 * (0.16666667f - x2 * (0.00833333f - x2 * 0.00019841f)));
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

static inline float fast_inv_sqrt(float x)
{
	float xhalf = 0.5f * x;
	union {
		float x;
		int32_t i;
	} u;
	u.x = x;
	u.i = 0x5f3759df - (u.i >> 1);
	u.x = u.x * (1.5f - xhalf * u.x * u.x);
	return u.x;
}

static inline float fast_sqrt(float x)
{
	if (x <= 0.0f)
		return 0.0f;
	return x * fast_inv_sqrt(x);
}

static inline float fast_acos(float x)
{
	x = sat_clamp(x, -1.0f, 1.0f);
	float s = fast_sqrt(1.0f - x * x);
	return fast_atan2(s, x);
}

/* 3-Band Biquad Filter Computation */
static void calc_biquad_coeffs(float linear_gain, float freq, float sample_rate, int type, float coeffs[5])
{
	float w0 = 2.0f * PI * freq / sample_rate;
	float cos_w0 = fast_cos(w0);
	float sin_w0 = fast_sin(w0);
	float a = 1.0f; /* Q = 1.0 */
	float alpha = sin_w0 / (2.0f * a);

	float g = sat_clamp(linear_gain, 0.0001f, 1.0f);
	float A = fast_sqrt(g);
	float sqrt_A = fast_sqrt(A);

	float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

	if (type == 0) {
		/* Low shelf (800 Hz) */
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
		/* High shelf (8000 Hz) */
		b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrt_A * alpha);
		b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
		b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrt_A * alpha);
		a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrt_A * alpha;
		a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
		a2 = (A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrt_A * alpha;
	}

	float inv_a0 = (a0 > 0.0001f) ? (1.0f / a0) : 1.0f;
	coeffs[0] = b0 * inv_a0;
	coeffs[1] = b1 * inv_a0;
	coeffs[2] = b2 * inv_a0;
	coeffs[3] = a1 * inv_a0;
	coeffs[4] = a2 * inv_a0;
}

static inline float apply_biquad(float in, const float coeffs[5], float state[2])
{
	float out = coeffs[0] * in + state[0];
	state[0] = coeffs[1] * in - coeffs[3] * out + state[1];
	state[1] = coeffs[2] * in - coeffs[4] * out;
	return out;
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
		float freq = (b == 0) ? 800.0f : ((b == 1) ? 2500.0f : 8000.0f);
		calc_biquad_coeffs(1.0f, freq, (float)cd->sample_rate, b, cd->direct.coeffs[0][b]);
		calc_biquad_coeffs(1.0f, freq, (float)cd->sample_rate, b, cd->direct.coeffs[1][b]);
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
	steamaudio_dsp_ambisonics_init(&cd->ambisonics, 1);

	/* Initialize Multi-Channel Surround Panning */
	steamaudio_dsp_panning_init(&cd->panning, STEAMAUDIO_SPEAKER_LAYOUT_STEREO);

	/* Initialize Virtual Surround Sound */
	steamaudio_dsp_virtual_surround_init(&cd->virtual_surround, STEAMAUDIO_SPEAKER_LAYOUT_5_1, cd->sample_rate);

	/* Initialize Acoustic Pathing */
	steamaudio_dsp_pathing_init(&cd->pathing, 1, cd->sample_rate);

	cd->output_mode = STEAMAUDIO_OUTPUT_BINAURAL;
	memset(cd->in_channels, 0, sizeof(cd->in_channels));
	memset(cd->out_channels, 0, sizeof(cd->out_channels));

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

/* Multi-Channel Speaker Layout Coordinates */
static const struct dsp_vec3 s_quad_speakers[4] = {
	{ -1.0f, 0.0f, -1.0f }, /* FL */
	{  1.0f, 0.0f, -1.0f }, /* FR */
	{ -1.0f, 0.0f,  1.0f }, /* RL */
	{  1.0f, 0.0f,  1.0f }  /* RR */
};

static const struct dsp_vec3 s_51_speakers[6] = {
	{ -1.0f, 0.0f, -1.0f }, /* 0: FL */
	{  1.0f, 0.0f, -1.0f }, /* 1: FR */
	{  0.0f, 0.0f, -1.0f }, /* 2: FC */
	{  0.0f, 0.0f,  0.0f }, /* 3: LFE */
	{ -1.0f, 0.0f,  1.0f }, /* 4: RL */
	{  1.0f, 0.0f,  1.0f }  /* 5: RR */
};

static const struct dsp_vec3 s_71_speakers[8] = {
	{ -1.0f, 0.0f, -1.0f }, /* 0: FL */
	{  1.0f, 0.0f, -1.0f }, /* 1: FR */
	{  0.0f, 0.0f, -1.0f }, /* 2: FC */
	{  0.0f, 0.0f,  0.0f }, /* 3: LFE */
	{ -1.0f, 0.0f,  1.0f }, /* 4: RL */
	{  1.0f, 0.0f,  1.0f }, /* 5: RR */
	{ -1.0f, 0.0f,  0.0f }, /* 6: SL */
	{  1.0f, 0.0f,  0.0f }  /* 7: SR */
};

void steamaudio_dsp_panning_init(struct steamaudio_panning_state *pan, uint32_t layout_type)
{
	pan->layout_type = layout_type;
	switch (layout_type) {
	case STEAMAUDIO_SPEAKER_LAYOUT_STEREO:
		pan->num_speakers = 2;
		break;
	case STEAMAUDIO_SPEAKER_LAYOUT_QUAD:
		pan->num_speakers = 4;
		break;
	case STEAMAUDIO_SPEAKER_LAYOUT_5_1:
		pan->num_speakers = 6;
		break;
	case STEAMAUDIO_SPEAKER_LAYOUT_7_1:
		pan->num_speakers = 8;
		break;
	default:
		pan->layout_type = STEAMAUDIO_SPEAKER_LAYOUT_STEREO;
		pan->num_speakers = 2;
		break;
	}

	pan->direction[0] = 0.0f;
	pan->direction[1] = 0.0f;
	pan->direction[2] = -1.0f;
	pan->prev_direction[0] = 0.0f;
	pan->prev_direction[1] = 0.0f;
	pan->prev_direction[2] = -1.0f;

	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++) {
		pan->current_weights[i] = 0.0f;
		pan->target_weights[i] = 0.0f;
	}
	steamaudio_dsp_panning_set_direction(pan, pan->direction);
	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++)
		pan->current_weights[i] = pan->target_weights[i];
}

void steamaudio_dsp_panning_set_direction(struct steamaudio_panning_state *pan, const float dir[3])
{
	pan->direction[0] = dir[0];
	pan->direction[1] = dir[1];
	pan->direction[2] = dir[2];

	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++)
		pan->target_weights[i] = 0.0f;

	float x = dir[0];
	float z = dir[2];
	float len2 = x * x + z * z;
	if (len2 < 1e-6f) {
		if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_STEREO) {
			pan->target_weights[0] = 0.7071f;
			pan->target_weights[1] = 0.7071f;
		} else if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1 ||
			   pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) {
			pan->target_weights[2] = 1.0f;
		} else {
			pan->target_weights[0] = 0.7071f;
			pan->target_weights[1] = 0.7071f;
		}
		return;
	}

	float inv_len = fast_inv_sqrt(len2);
	x *= inv_len;
	z *= inv_len;

	if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_STEREO) {
		float q = (x + 1.0f) * (PI * 0.25f);
		pan->target_weights[0] = fast_cos(q);
		pan->target_weights[1] = fast_sin(q);
		return;
	}

	float phi = PI + fast_atan2(x, z);
	while (phi < 0.0f) phi += TWO_PI;
	while (phi >= TWO_PI) phi -= TWO_PI;

	int s0 = 0, s1 = 1;
	float angle_between = PI * 0.5f;

	if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_QUAD) {
		if (phi <= (PI * 0.25f) || phi > (7.0f * PI * 0.25f)) {
			s0 = 0; s1 = 1; angle_between = PI * 0.5f;
		} else if (phi > (PI * 0.25f) && phi <= (3.0f * PI * 0.25f)) {
			s0 = 2; s1 = 0; angle_between = PI * 0.5f;
		} else if (phi > (3.0f * PI * 0.25f) && phi <= (5.0f * PI * 0.25f)) {
			s0 = 3; s1 = 2; angle_between = PI * 0.5f;
		} else {
			s0 = 1; s1 = 3; angle_between = PI * 0.5f;
		}

		const struct dsp_vec3 *spk0 = &s_quad_speakers[s0];
		float s0_inv = fast_inv_sqrt(spk0->x * spk0->x + spk0->z * spk0->z);
		float dot = x * (spk0->x * s0_inv) + z * (spk0->z * s0_inv);
		float dphi = fast_acos(dot);
		float u = sat_clamp(dphi / angle_between, 0.0f, 1.0f);
		pan->target_weights[s0] = fast_cos(u * (PI * 0.5f));
		pan->target_weights[s1] = fast_sin(u * (PI * 0.5f));
	} else if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_5_1) {
		if (phi >= 0.0f && phi < (PI * 0.25f)) {
			s0 = 0; s1 = 2; angle_between = PI * 0.25f;
		} else if (phi >= (PI * 0.25f) && phi < (3.0f * PI * 0.25f)) {
			s0 = 4; s1 = 0; angle_between = PI * 0.5f;
		} else if (phi >= (3.0f * PI * 0.25f) && phi < (5.0f * PI * 0.25f)) {
			s0 = 5; s1 = 4; angle_between = PI * 0.5f;
		} else if (phi >= (5.0f * PI * 0.25f) && phi < (7.0f * PI * 0.25f)) {
			s0 = 1; s1 = 5; angle_between = PI * 0.5f;
		} else {
			s0 = 2; s1 = 1; angle_between = PI * 0.25f;
		}

		const struct dsp_vec3 *spk0 = &s_51_speakers[s0];
		float s0_inv = fast_inv_sqrt(spk0->x * spk0->x + spk0->z * spk0->z);
		float dot = x * (spk0->x * s0_inv) + z * (spk0->z * s0_inv);
		float dphi = fast_acos(dot);
		float u = sat_clamp(dphi / angle_between, 0.0f, 1.0f);
		pan->target_weights[s0] = fast_cos(u * (PI * 0.5f));
		pan->target_weights[s1] = fast_sin(u * (PI * 0.5f));
	} else if (pan->layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) {
		if (phi >= 0.0f && phi < (PI * 0.25f)) {
			s0 = 0; s1 = 2; angle_between = PI * 0.25f;
		} else if (phi >= (PI * 0.25f) && phi < (2.0f * PI * 0.25f)) {
			s0 = 6; s1 = 0; angle_between = PI * 0.25f;
		} else if (phi >= (2.0f * PI * 0.25f) && phi < (3.0f * PI * 0.25f)) {
			s0 = 4; s1 = 6; angle_between = PI * 0.25f;
		} else if (phi >= (3.0f * PI * 0.25f) && phi < (5.0f * PI * 0.25f)) {
			s0 = 5; s1 = 4; angle_between = PI * 0.5f;
		} else if (phi >= (5.0f * PI * 0.25f) && phi < (6.0f * PI * 0.25f)) {
			s0 = 7; s1 = 5; angle_between = PI * 0.25f;
		} else if (phi >= (6.0f * PI * 0.25f) && phi < (7.0f * PI * 0.25f)) {
			s0 = 1; s1 = 7; angle_between = PI * 0.25f;
		} else {
			s0 = 2; s1 = 1; angle_between = PI * 0.25f;
		}

		const struct dsp_vec3 *spk0 = &s_71_speakers[s0];
		float s0_inv = fast_inv_sqrt(spk0->x * spk0->x + spk0->z * spk0->z);
		float dot = x * (spk0->x * s0_inv) + z * (spk0->z * s0_inv);
		float dphi = fast_acos(dot);
		float u = sat_clamp(dphi / angle_between, 0.0f, 1.0f);
		pan->target_weights[s0] = fast_cos(u * (PI * 0.5f));
		pan->target_weights[s1] = fast_sin(u * (PI * 0.5f));
	}
}

void steamaudio_dsp_panning_process(struct steamaudio_panning_state *pan, const float *in,
				    float out_ch[STEAMAUDIO_MAX_SPEAKERS][256], uint32_t frames)
{
	int num_spk = pan->num_speakers;
	float inv_frames = (frames > 0) ? (1.0f / (float)frames) : 1.0f;

	for (int ch = 0; ch < num_spk; ch++) {
		float w_curr = pan->current_weights[ch];
		float w_targ = pan->target_weights[ch];
		float w_step = (w_targ - w_curr) * inv_frames;

		for (uint32_t i = 0; i < frames; i++) {
			float w = w_curr + w_step * (float)i;
			out_ch[ch][i] = in[i] * w;
		}
		pan->current_weights[ch] = w_targ;
	}
	for (int ch = num_spk; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
		for (uint32_t i = 0; i < frames; i++)
			out_ch[ch][i] = 0.0f;
	}
}

void steamaudio_dsp_virtual_surround_init(struct steamaudio_virtual_surround_state *vsurr,
					  uint32_t layout_type, uint32_t sample_rate)
{
	vsurr->layout_type = layout_type;
	vsurr->hrtf_blend = 1.0f;
	vsurr->num_speakers = (layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) ? 8 : 6;

	memset(vsurr->delay_lines, 0, sizeof(vsurr->delay_lines));
	for (int i = 0; i < STEAMAUDIO_MAX_SPEAKERS; i++)
		vsurr->write_idx[i] = 0;

	const struct dsp_vec3 *spk = (layout_type == STEAMAUDIO_SPEAKER_LAYOUT_7_1) ?
				     s_71_speakers : s_51_speakers;

	float max_itd_samples = 32.0f * (float)sample_rate / 48000.0f;

	for (int i = 0; i < vsurr->num_speakers; i++) {
		if (i == 3) {
			/* LFE channel */
			vsurr->itd_samples[i][0] = 0.0f;
			vsurr->itd_samples[i][1] = 0.0f;
			vsurr->ild_gains[i][0] = 0.7071f;
			vsurr->ild_gains[i][1] = 0.7071f;
			continue;
		}

		float az = fast_atan2(spk[i].x, spk[i].z);
		float sin_az = fast_sin(az);

		vsurr->itd_samples[i][0] = (sin_az < 0.0f) ? 0.0f : (sin_az * max_itd_samples);
		vsurr->itd_samples[i][1] = (sin_az > 0.0f) ? 0.0f : (-sin_az * max_itd_samples);
		vsurr->ild_gains[i][0] = (sin_az > 0.0f) ? (1.0f - 0.5f * sin_az) : 1.0f;
		vsurr->ild_gains[i][1] = (sin_az < 0.0f) ? (1.0f + 0.5f * sin_az) : 1.0f;
	}
}

void steamaudio_dsp_virtual_surround_process(struct steamaudio_virtual_surround_state *vsurr,
					     const float in_ch[STEAMAUDIO_MAX_SPEAKERS][256],
					     float *out_l, float *out_r, uint32_t frames)
{
	for (uint32_t i = 0; i < frames; i++) {
		out_l[i] = 0.0f;
		out_r[i] = 0.0f;
	}

	float blend = vsurr->hrtf_blend;

	for (int ch = 0; ch < vsurr->num_speakers; ch++) {
		if (ch == 3) {
			for (uint32_t i = 0; i < frames; i++) {
				float lfe = in_ch[ch][i] * 0.7071f;
				out_l[i] += lfe;
				out_r[i] += lfe;
			}
			continue;
		}

		float itd_l = vsurr->itd_samples[ch][0];
		float itd_r = vsurr->itd_samples[ch][1];
		float ild_l = vsurr->ild_gains[ch][0];
		float ild_r = vsurr->ild_gains[ch][1];
		int w_idx = vsurr->write_idx[ch];

		for (uint32_t i = 0; i < frames; i++) {
			vsurr->delay_lines[ch][w_idx] = in_ch[ch][i];

			int r_l = w_idx - (int)itd_l;
			if (r_l < 0) r_l += STEAMAUDIO_DELAY_LINE_SIZE;
			float left_s = vsurr->delay_lines[ch][r_l] * ild_l;

			int r_r = w_idx - (int)itd_r;
			if (r_r < 0) r_r += STEAMAUDIO_DELAY_LINE_SIZE;
			float right_s = vsurr->delay_lines[ch][r_r] * ild_r;

			w_idx = (w_idx + 1) % STEAMAUDIO_DELAY_LINE_SIZE;

			out_l[i] += in_ch[ch][i] * (1.0f - blend) * 0.7071f + left_s * blend;
			out_r[i] += in_ch[ch][i] * (1.0f - blend) * 0.7071f + right_s * blend;
		}
		vsurr->write_idx[ch] = w_idx;
	}
}

static inline void eval_sh_basis(float x, float y, float z, int order, float sh[16])
{
	sh[0] = 0.282095f;
	if (order < 1) return;

	sh[1] = 0.488603f * y;
	sh[2] = 0.488603f * z;
	sh[3] = 0.488603f * x;
	if (order < 2) return;

	sh[4] = 1.092548f * x * y;
	sh[5] = 1.092548f * y * z;
	sh[6] = 0.315392f * (-x * x - y * y + 2.0f * z * z);
	sh[7] = 1.092548f * x * z;
	sh[8] = 0.546274f * (x * x - y * y);
	if (order < 3) return;

	sh[9]  = 0.590044f * y * (3.0f * x * x - y * y);
	sh[10] = 2.890611f * x * y * z;
	sh[11] = 0.457046f * y * (4.0f * z * z - x * x - y * y);
	sh[12] = 0.373176f * z * (2.0f * z * z - 3.0f * x * x - 3.0f * y * y);
	sh[13] = 0.457046f * x * (4.0f * z * z - x * x - y * y);
	sh[14] = 1.445306f * z * (x * x - y * y);
	sh[15] = 0.590044f * x * (x * x - 3.0f * y * y);
}

void steamaudio_dsp_ambisonics_init(struct steamaudio_ambisonics_state *ambi, uint32_t order)
{
	if (order < 1) order = 1;
	if (order > 3) order = 3;

	ambi->order = order;
	ambi->num_channels = (order + 1) * (order + 1);
	ambi->direction[0] = 0.0f;
	ambi->direction[1] = 0.0f;
	ambi->direction[2] = 1.0f;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++)
			ambi->rotation[i][j] = (i == j) ? 1.0f : 0.0f;
	}
}

void steamaudio_dsp_ambisonics_encode(uint32_t order, const float dir[3], const float *in,
				      float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256], uint32_t frames)
{
	float len2 = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
	float inv_len = (len2 > 1e-6f) ? fast_inv_sqrt(len2) : 1.0f;
	float x = dir[0] * inv_len;
	float y = dir[1] * inv_len;
	float z = dir[2] * inv_len;

	float sh[16];
	eval_sh_basis(x, y, z, (int)order, sh);
	int num_ch = (order + 1) * (order + 1);
	if (num_ch > 16) num_ch = 16;

	for (int ch = 0; ch < num_ch; ch++) {
		float gain = sh[ch];
		for (uint32_t i = 0; i < frames; i++)
			out_ch[ch][i] = in[i] * gain;
	}
}

void steamaudio_dsp_ambisonics_decode_binaural(struct steamaudio_ambisonics_state *ambi,
					       const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					       float *out_l, float *out_r, uint32_t frames)
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

	int num_ch = (ambi->order + 1) * (ambi->order + 1);
	if (num_ch > 16) num_ch = 16;

	for (uint32_t i = 0; i < frames; i++) {
		out_l[i] = 0.0f;
		out_r[i] = 0.0f;
	}

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

		for (uint32_t i = 0; i < frames; i++) {
			float feed = 0.0f;
			for (int ch = 0; ch < num_ch; ch++)
				feed += in_ch[ch][i] * sh[ch];
			feed *= 0.125f;

			out_l[i] += feed * ild_l;
			out_r[i] += feed * ild_r;
		}
	}
}

void steamaudio_dsp_pathing_init(struct steamaudio_pathing_state *pathing, uint32_t order, uint32_t sample_rate)
{
	memset(pathing, 0, sizeof(*pathing));
	pathing->order = (order <= 3) ? order : 1;
	pathing->num_channels = (pathing->order + 1) * (pathing->order + 1);
	pathing->binaural = true;
	pathing->eq_coeffs[0] = 1.0f;
	pathing->eq_coeffs[1] = 1.0f;
	pathing->eq_coeffs[2] = 1.0f;
	pathing->sh_coeffs[0] = 1.0f;
	pathing->rotation[0][0] = 1.0f;
	pathing->rotation[1][1] = 1.0f;
	pathing->rotation[2][2] = 1.0f;

	calc_biquad_coeffs(1.0f, 800.0f, (float)sample_rate, 0, pathing->filter_coeffs[0]);
	calc_biquad_coeffs(1.0f, 2500.0f, (float)sample_rate, 1, pathing->filter_coeffs[1]);
	calc_biquad_coeffs(1.0f, 8000.0f, (float)sample_rate, 2, pathing->filter_coeffs[2]);
}

void steamaudio_dsp_pathing_set_params(struct steamaudio_pathing_state *pathing,
				       const float eq[STEAMAUDIO_NUM_EQ_BANDS],
				       const float sh[STEAMAUDIO_MAX_HOA_CHANNELS],
				       uint32_t order, bool binaural,
				       const float rot[3][3],
				       uint32_t sample_rate)
{
	pathing->order = (order <= 3) ? order : 1;
	pathing->num_channels = (pathing->order + 1) * (pathing->order + 1);
	pathing->binaural = binaural;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		pathing->eq_coeffs[b] = sat_clamp(eq[b], 0.0f, 1.0f);
		int type = (b == 0) ? 0 : ((b == 1) ? 1 : 2);
		float freq = (b == 0) ? 800.0f : ((b == 1) ? 2500.0f : 8000.0f);
		calc_biquad_coeffs(pathing->eq_coeffs[b], freq, (float)sample_rate, type, pathing->filter_coeffs[b]);
	}

	for (int i = 0; i < pathing->num_channels; i++)
		pathing->sh_coeffs[i] = sh[i];

	if (rot) {
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				pathing->rotation[r][c] = rot[r][c];
	}
}

void steamaudio_dsp_pathing_process(struct steamaudio_pathing_state *pathing,
				    struct steamaudio_ambisonics_state *ambi,
				    struct steamaudio_panning_state *panning,
				    const float *in, float *out_l, float *out_r,
				    float out_ch[STEAMAUDIO_MAX_SPEAKERS][256],
				    uint32_t frames)
{
	float eq_buffer[256];
	for (uint32_t i = 0; i < frames; i++) {
		float s = in[i];
		s = apply_biquad(s, pathing->filter_coeffs[0], pathing->filter_states[0]);
		s = apply_biquad(s, pathing->filter_coeffs[1], pathing->filter_states[1]);
		s = apply_biquad(s, pathing->filter_coeffs[2], pathing->filter_states[2]);
		eq_buffer[i] = s;
	}

	float hoa_channels[STEAMAUDIO_MAX_HOA_CHANNELS][256];
	int num_ch = pathing->num_channels;
	for (int ch = 0; ch < num_ch; ch++) {
		float coeff = pathing->sh_coeffs[ch];
		for (uint32_t i = 0; i < frames; i++)
			hoa_channels[ch][i] = eq_buffer[i] * coeff;
	}

	if (pathing->binaural) {
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				ambi->rotation[r][c] = pathing->rotation[r][c];
		ambi->order = pathing->order;

		steamaudio_dsp_ambisonics_decode_binaural(ambi,
							  (const float (*)[256])hoa_channels,
							  out_l, out_r, frames);
	} else {
		float dir[3] = { 0.0f, 0.0f, 1.0f };
		if (num_ch >= 4) {
			dir[0] = pathing->sh_coeffs[3];
			dir[1] = pathing->sh_coeffs[1];
			dir[2] = pathing->sh_coeffs[2];
			float len = fast_sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
			if (len > 0.0001f) {
				float inv_len = 1.0f / len;
				dir[0] *= inv_len; dir[1] *= inv_len; dir[2] *= inv_len;
			}
		}
		steamaudio_dsp_panning_set_direction(panning, dir);
		steamaudio_dsp_panning_process(panning, eq_buffer, out_ch, frames);
		memcpy(out_l, out_ch[0], frames * sizeof(float));
		memcpy(out_r, out_ch[1], frames * sizeof(float));
	}
}

static inline void steamaudio_dsp_render(struct steamaudio_comp_data *cd, uint32_t frames)
{
	if (cd->output_mode == STEAMAUDIO_OUTPUT_SURROUND_PANNING) {
		process_direct_path(cd, cd->in_scratch, cd->out_left, frames);
		memcpy(cd->in_scratch, cd->out_left, frames * sizeof(float));
		steamaudio_dsp_panning_process(&cd->panning, cd->in_scratch, cd->out_channels, frames);
		memcpy(cd->out_left, cd->out_channels[0], frames * sizeof(float));
		memcpy(cd->out_right, cd->out_channels[1], frames * sizeof(float));
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND) {
		steamaudio_dsp_virtual_surround_process(&cd->virtual_surround,
							(const float (*)[256])cd->in_channels,
							cd->out_left, cd->out_right, frames);
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_AMBISONICS) {
		if (cd->channels > 1) {
			steamaudio_dsp_ambisonics_decode_binaural(&cd->ambisonics,
								  (const float (*)[256])cd->in_channels,
								  cd->out_left, cd->out_right, frames);
		} else {
			steamaudio_dsp_ambisonics_encode(cd->ambisonics.order, cd->ambisonics.direction,
							 cd->in_scratch, cd->in_channels, frames);
			steamaudio_dsp_ambisonics_decode_binaural(&cd->ambisonics,
								  (const float (*)[256])cd->in_channels,
								  cd->out_left, cd->out_right, frames);
		}
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_PATHING) {
		steamaudio_dsp_pathing_process(&cd->pathing, &cd->ambisonics, &cd->panning,
					       cd->in_scratch, cd->out_left, cd->out_right,
					       cd->out_channels, frames);
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else {
		process_direct_path(cd, cd->in_scratch, cd->out_left, frames);
		memcpy(cd->in_scratch, cd->out_left, frames * sizeof(float));
		process_binaural(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	}
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

	if (cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND && cd->channels > 1) {
		for (int ch = 0; ch < cd->channels && ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
			for (uint32_t i = 0; i < frames; i++)
				cd->in_channels[ch][i] = (float)src[i * cd->channels + ch] * (1.0f / 32768.0f);
		}
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = cd->in_channels[0][i];
	} else {
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = (float)src[i * cd->channels] * (1.0f / 32768.0f);
	}

	steamaudio_dsp_render(cd, frames);

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

#if CONFIG_FORMAT_S24LE
static int steamaudio_process_s24(struct processing_module *mod,
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

	/* S24_4LE: 24-bit audio in 32-bit container, sign-extend Q1.23 to float */
	if (cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND && cd->channels > 1) {
		for (int ch = 0; ch < cd->channels && ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
			for (uint32_t i = 0; i < frames; i++) {
				int32_t val = (src[i * cd->channels + ch] << 8) >> 8;
				cd->in_channels[ch][i] = (float)val * (1.0f / 8388608.0f);
			}
		}
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = cd->in_channels[0][i];
	} else {
		for (uint32_t i = 0; i < frames; i++) {
			int32_t val = (src[i * cd->channels] << 8) >> 8;
			cd->in_scratch[i] = (float)val * (1.0f / 8388608.0f);
		}
	}

	steamaudio_dsp_render(cd, frames);

	for (uint32_t i = 0; i < frames; i++) {
		float l = sat_clamp(cd->out_left[i] * 8388608.0f, -8388608.0f, 8388607.0f);
		float r = sat_clamp(cd->out_right[i] * 8388608.0f, -8388608.0f, 8388607.0f);
		dst[2 * i] = (int32_t)l;
		dst[2 * i + 1] = (int32_t)r;
	}

	source_release_data(source, in_bytes);
	sink_commit_buffer(sink, out_bytes);
	return 0;
}
#endif

#if CONFIG_FORMAT_S32LE
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

	if (cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND && cd->channels > 1) {
		for (int ch = 0; ch < cd->channels && ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
			for (uint32_t i = 0; i < frames; i++)
				cd->in_channels[ch][i] = (float)src[i * cd->channels + ch] * (1.0f / 2147483648.0f);
		}
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = cd->in_channels[0][i];
	} else {
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = (float)src[i * cd->channels] * (1.0f / 2147483648.0f);
	}

	steamaudio_dsp_render(cd, frames);

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
		return steamaudio_process_s24;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		return steamaudio_process_s32;
#endif
	default:
		return NULL;
	}
}
