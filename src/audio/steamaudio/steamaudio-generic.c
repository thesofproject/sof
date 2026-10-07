// SPDX-License-Identifier: Apache-2.0
//
// Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
// Copyright (c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio Spatial Processing Core for SOF

#include "steamaudio.h"
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/sink_source_utils.h>
#include <rtos/string.h>

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

void steamaudio_dsp_update_direct_eq(struct steamaudio_comp_data *cd)
{
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float freq = (b == 0) ? 800.0f : ((b == 1) ? 2500.0f : 8000.0f);
		float eq_gain = 1.0f;
		if (!(cd->mute_mask & (1u << STEAMAUDIO_STEP_ATMOSPHERE))) {
			if (cd->direct.flags & (1 << 1))
				eq_gain *= cd->direct.air_absorption[b];
		}
		if (!(cd->mute_mask & (1u << STEAMAUDIO_STEP_MATERIAL_TRANSMISSION))) {
			if (cd->direct.flags & (1 << 3))
				eq_gain *= cd->direct.transmission[b];
		}
		calc_biquad_coeffs(eq_gain, freq, (float)cd->sample_rate, b, cd->direct.coeffs[0][b]);
		calc_biquad_coeffs(eq_gain, freq, (float)cd->sample_rate, b, cd->direct.coeffs[1][b]);
	}
}

void steamaudio_dsp_init(struct steamaudio_comp_data *cd, uint32_t sample_rate)
{
	cd->sample_rate = sample_rate ? sample_rate : 48000;
	cd->enable = true;
	cd->bitstream_mode = false;
	cd->mute_mask = 0;

	/* Initialize Direct Path */
	cd->direct.active_slot = 0;
	cd->direct.current_gain = 1.0f;
	cd->direct.target_gain = 1.0f;
	cd->direct.gain_step = 0.0f;
	cd->direct.needs_crossfade = false;
	cd->direct.crossfade_remaining = 0;
	cd->direct.flags = 0;
	cd->direct.transmission_type = 0;
	cd->direct.distance_attenuation = 1.0f;
	cd->direct.directivity = 1.0f;
	cd->direct.occlusion = 0.0f;
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		cd->direct.air_absorption[b] = 1.0f;
		cd->direct.transmission[b] = 1.0f;
	}

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
	cd->binaural.interpolation = 0;
	cd->binaural.hrtf_slot_id = 0;

	/* Initialize FDN Reverb with prime delays and clear delay buffers */
	memset(cd->reverb.delay_buffers, 0, sizeof(cd->reverb.delay_buffers));
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
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		cd->reverb.reverb_times[b] = 1.0f;
		cd->reverb.eq_gains[b] = 1.0f;
	}

	/* Initialize Ambisonics */
	steamaudio_dsp_ambisonics_init(&cd->ambisonics, 1);

	/* Initialize Multi-Channel Surround Panning */
	steamaudio_dsp_panning_init(&cd->panning, STEAMAUDIO_SPEAKER_LAYOUT_STEREO);

	/* Initialize Virtual Surround Sound */
	steamaudio_dsp_virtual_surround_init(&cd->virtual_surround, STEAMAUDIO_SPEAKER_LAYOUT_5_1, cd->sample_rate);

	/* Initialize Acoustic Pathing */
	steamaudio_dsp_pathing_init(&cd->pathing, 1, cd->sample_rate);

	/* Initialize Hybrid Reverb */
	steamaudio_dsp_hybrid_reverb_init(&cd->hybrid);

	/* Initialize Measured SOFA / HRIR */
	steamaudio_dsp_sofa_hrir_init(&cd->sofa_hrir);

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
	steamaudio_dsp_dynamic_geom_init(&cd->dynamic_geom);
	steamaudio_dsp_directivity_init(&cd->directivity);
	steamaudio_dsp_atmosphere_init(&cd->atmosphere);
	steamaudio_dsp_diffraction_init(&cd->diffraction);
	steamaudio_dsp_probe_batch_init(&cd->probe_batch);
	steamaudio_dsp_graph_search_init(&cd->graph_search);
	steamaudio_dsp_reflection_mixer_init(&cd->reflection_mixer);
	steamaudio_dsp_instanced_mesh_init(&cd->instanced_mesh);
	steamaudio_dsp_ray_tracer_init(&cd->ray_tracer);
	steamaudio_dsp_reverb_estimator_init(&cd->reverb_estimator);
	steamaudio_dsp_early_reflections_init(&cd->early_reflections);
	steamaudio_dsp_material_transmission_init(&cd->material_transmission);
	steamaudio_dsp_acoustic_portals_init(&cd->acoustic_portals);
	steamaudio_dsp_volumetric_source_init(&cd->volumetric_source);
	steamaudio_dsp_source_prioritization_init(&cd->source_prioritization);
	steamaudio_dsp_ground_reflection_init(&cd->ground_reflection);
	steamaudio_dsp_true_peak_limiter_init(&cd->true_peak_limiter);
	steamaudio_dsp_room_modes_init(&cd->room_modes);
	steamaudio_dsp_atmospheric_turbulence_init(&cd->atmospheric_turbulence);
	steamaudio_dsp_surface_scattering_init(&cd->surface_scattering);
	steamaudio_dsp_sound_barrier_init(&cd->sound_barrier);
	steamaudio_dsp_near_field_init(&cd->near_field);
	steamaudio_dsp_nonlinear_wave_init(&cd->nonlinear_wave);
	steamaudio_dsp_battle_bleed_init(&cd->battle_bleed, cd->sample_rate);
	steamaudio_dsp_voice_lod_init(&cd->voice_lod, cd->sample_rate);
	steamaudio_dsp_upmix_init(&cd->upmix, STEAMAUDIO_SPEAKER_LAYOUT_7_1, cd->sample_rate);
}

static inline void process_direct_path(struct steamaudio_comp_data *cd, const float *in, float *out, uint32_t frames)
{
	if (cd->mute_mask & (1u << STEAMAUDIO_STEP_DIRECT)) {
		memcpy(out, in, frames * sizeof(float));
		return;
	}

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
	if (cd->mute_mask & (1u << STEAMAUDIO_STEP_BINAURAL)) {
		memcpy(out_l, in, frames * sizeof(float));
		memcpy(out_r, in, frames * sizeof(float));
		return;
	}

	float x = cd->binaural.direction[0];
	float y = cd->binaural.direction[1];
	float z = cd->binaural.direction[2];

	float azimuth = fast_atan2(x, z);
	float sin_az = fast_sin(azimuth);
	float horiz_sq = x * x + z * z;
	float horiz_dist = (horiz_sq > 1e-9f) ? (1.0f / fast_inv_sqrt(horiz_sq)) : 0.0f;
	float elevation = fast_atan2(y, (horiz_dist > 1e-6f) ? horiz_dist : 1e-6f);
	float cos_el = fast_cos(elevation);

	/* ITD: Woodworth spherical model (~0.66ms max delay = ~32 samples at 48kHz) */
	float max_itd_samples = 32.0f * (float)cd->sample_rate / 48000.0f;
	float itd_l = (sin_az < 0.0f) ? 0.0f : (sin_az * cos_el * max_itd_samples);
	float itd_r = (sin_az > 0.0f) ? 0.0f : (-sin_az * cos_el * max_itd_samples);

	/* ILD: head-shadow attenuation with elevation spectral shading */
	float el_shading = 1.0f + 0.15f * y;
	float ild_l = ((sin_az > 0.0f) ? (1.0f - 0.5f * sin_az) : 1.0f) * el_shading;
	float ild_r = ((sin_az < 0.0f) ? (1.0f + 0.5f * sin_az) : 1.0f) * el_shading;

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
	if (cd->mute_mask & ((1u << STEAMAUDIO_STEP_REVERB) | (1u << STEAMAUDIO_STEP_CONVOLUTION)))
		return;

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

void steamaudio_dsp_hybrid_reverb_init(struct steamaudio_hybrid_reverb_state *hybrid)
{
	memset(hybrid->transition_delay_buffer, 0, sizeof(hybrid->transition_delay_buffer));
	hybrid->delay_write_ptr = 0;
	hybrid->delay_samples = 0;
	for (int i = 0; i < STEAMAUDIO_NUM_EQ_BANDS; i++) {
		hybrid->eq_coeffs[i] = 1.0f;
		hybrid->eq_states[i][0] = 0.0f;
		hybrid->eq_states[i][1] = 0.0f;
	}
	hybrid->wet_gain = 0.25f;
	hybrid->active = true;
}

void steamaudio_dsp_hybrid_reverb_process(struct steamaudio_comp_data *cd,
					 const float *in,
					 const float in_early[2][256],
					 float *out_l, float *out_r,
					 uint32_t frames)
{
	/* Zero-action bypass if either Reverb or Convolution step is muted */
	if (cd->mute_mask & ((1u << STEAMAUDIO_STEP_REVERB) | (1u << STEAMAUDIO_STEP_CONVOLUTION))) {
		for (uint32_t i = 0; i < frames; i++) {
			out_l[i] = in[i];
			out_r[i] = in[i];
		}
		return;
	}

	struct steamaudio_hybrid_reverb_state *hyb = &cd->hybrid;
	float delayed_in[256];
	int delay = hyb->delay_samples;
	if (delay < 0) delay = 0;
	if (delay >= STEAMAUDIO_MAX_HYBRID_DELAY) delay = STEAMAUDIO_MAX_HYBRID_DELAY - 1;

	/* 1. Ring buffer delay */
	for (uint32_t i = 0; i < frames; i++) {
		hyb->transition_delay_buffer[hyb->delay_write_ptr] = in[i];
		int read_ptr = hyb->delay_write_ptr - delay;
		if (read_ptr < 0) read_ptr += STEAMAUDIO_MAX_HYBRID_DELAY;
		delayed_in[i] = hyb->transition_delay_buffer[read_ptr];
		hyb->delay_write_ptr = (hyb->delay_write_ptr + 1) % STEAMAUDIO_MAX_HYBRID_DELAY;
	}

	/* 2. 3-band EQ weighting (Low, Mid, High) */
	float eq_low = hyb->eq_coeffs[0];
	float eq_mid = hyb->eq_coeffs[1];
	float eq_high = hyb->eq_coeffs[2];
	float avg_eq = (eq_low + eq_mid + eq_high) * 0.333333f;
	for (uint32_t i = 0; i < frames; i++) {
		delayed_in[i] *= avg_eq;
	}

	/* 3. Add early reflections if provided */
	if (in_early) {
		for (uint32_t i = 0; i < frames; i++) {
			out_l[i] += in_early[0][i];
			out_r[i] += in_early[1][i];
		}
	}

	/* 4. Process late FDN reverb on delayed & EQ-weighted input */
	process_reverb(cd, delayed_in, out_l, out_r, frames);
}


static void check_inband_bitstream(struct steamaudio_comp_data *cd, const void *src_ptr, uint32_t avail_bytes)
{
	if (!cd->bitstream_mode)
		return;

	if (avail_bytes < sizeof(struct steamaudio_bitstream_header))
		return;

	const struct steamaudio_bitstream_header *hdr = (const struct steamaudio_bitstream_header *)src_ptr;
	if (hdr->sync_word != STEAMAUDIO_SOF_SYNC_WORD)
		return;

	if (hdr->protocol_version != STEAMAUDIO_SOF_PROTOCOL_VERSION)
		return;

	if (hdr->num_samples == 0 || hdr->num_samples > 4096)
		return;

	if (hdr->payload_bytes > avail_bytes)
		return;

	/* Valid synchronized bitstream frame detected */
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

void steamaudio_dsp_ambisonics_rotate(uint32_t order, const float rot[3][3],
				      const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
				      float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256], uint32_t frames)
{
	int num_ch = (order + 1) * (order + 1);
	if (num_ch > STEAMAUDIO_MAX_HOA_CHANNELS)
		num_ch = STEAMAUDIO_MAX_HOA_CHANNELS;

	/* Channel 0: W (omni invariant to rotation) */
	for (uint32_t i = 0; i < frames; i++)
		out_ch[0][i] = in_ch[0][i];

	if (num_ch >= 4) {
		/* Order 1: Y, Z, X dipole rotation */
		for (uint32_t i = 0; i < frames; i++) {
			float y = in_ch[1][i];
			float z = in_ch[2][i];
			float x = in_ch[3][i];

			out_ch[1][i] = y * rot[0][0] + z * rot[0][1] + x * rot[0][2];
			out_ch[2][i] = y * rot[1][0] + z * rot[1][1] + x * rot[1][2];
			out_ch[3][i] = y * rot[2][0] + z * rot[2][1] + x * rot[2][2];
		}
	}

	for (int ch = 4; ch < num_ch; ch++) {
		for (uint32_t i = 0; i < frames; i++)
			out_ch[ch][i] = in_ch[ch][i];
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

void steamaudio_dsp_path_sim_eval(const float source[3], const float listener[3],
				  const float (*virtual_sources)[3], const float *path_weights,
				  const float *deviations, uint32_t num_paths, uint32_t order,
				  float eq_out[STEAMAUDIO_NUM_EQ_BANDS],
				  float sh_out[STEAMAUDIO_MAX_HOA_CHANNELS],
				  float avg_dir_out[3], float *dist_ratio_out, float *tot_dev_out)
{
	int num_sh = (order <= 3) ? (int)((order + 1) * (order + 1)) : 4;
	if (num_sh > STEAMAUDIO_MAX_HOA_CHANNELS)
		num_sh = STEAMAUDIO_MAX_HOA_CHANNELS;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
		eq_out[b] = 0.0f;
	for (int c = 0; c < STEAMAUDIO_MAX_HOA_CHANNELS; c++)
		sh_out[c] = 0.0f;
	if (avg_dir_out) {
		avg_dir_out[0] = 0.0f;
		avg_dir_out[1] = 0.0f;
		avg_dir_out[2] = 0.0f;
	}
	if (dist_ratio_out) *dist_ratio_out = 0.0f;
	if (tot_dev_out) *tot_dev_out = 0.0f;

	if (num_paths == 0 || !virtual_sources || !path_weights) {
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) eq_out[b] = 1.0f;
		float dir[3] = { source[0] - listener[0], source[1] - listener[1], source[2] - listener[2] };
		float len2 = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
		float inv_len = (len2 > 1e-6f) ? fast_inv_sqrt(len2) : 1.0f;
		float dx = dir[0] * inv_len, dy = dir[1] * inv_len, dz = dir[2] * inv_len;
		if (avg_dir_out) { avg_dir_out[0] = dx; avg_dir_out[1] = dy; avg_dir_out[2] = dz; }
		eval_sh_basis(dx, dy, dz, (int)order, sh_out);
		if (dist_ratio_out) *dist_ratio_out = 1.0f;
		if (tot_dev_out) *tot_dev_out = 0.0f;
		return;
	}

	float dir_acc[3] = { 0.0f, 0.0f, 0.0f };
	float ratio_acc = 0.0f;
	float dev_acc = 0.0f;

	float d_direct = sqrtf((source[0] - listener[0])*(source[0] - listener[0]) +
			       (source[1] - listener[1])*(source[1] - listener[1]) +
			       (source[2] - listener[2])*(source[2] - listener[2]));

	for (uint32_t p = 0; p < num_paths; p++) {
		float w = path_weights[p];
		if (w <= 0.0f) continue;

		float vs[3] = { virtual_sources[p][0], virtual_sources[p][1], virtual_sources[p][2] };
		float pdir[3] = { vs[0] - listener[0], vs[1] - listener[1], vs[2] - listener[2] };
		float dist = sqrtf(pdir[0]*pdir[0] + pdir[1]*pdir[1] + pdir[2]*pdir[2]);
		float inv_dist = (dist > 1e-4f) ? 1.0f / dist : 1.0f;
		float un[3] = { pdir[0] * inv_dist, pdir[1] * inv_dist, pdir[2] * inv_dist };

		float atten = 1.0f / (dist > 1.0f ? dist : 1.0f);
		float gain = w * atten;

		float sh[16];
		eval_sh_basis(un[0], un[1], un[2], (int)order, sh);
		for (int c = 0; c < num_sh; c++)
			sh_out[c] += gain * sh[c];

		dir_acc[0] += gain * un[0];
		dir_acc[1] += gain * un[1];
		dir_acc[2] += gain * un[2];

		float dev = (deviations) ? deviations[p] : 0.0f;
		dev_acc += w * dev;

		float eq0 = expf(-0.35f * dev);
		float eq1 = expf(-0.75f * dev);
		float eq2 = expf(-1.50f * dev);
		eq_out[0] += w * eq0;
		eq_out[1] += w * eq1;
		eq_out[2] += w * eq2;

		float pratio = (dist > 1.0f && d_direct > 1.0f) ? (d_direct / dist) : 1.0f;
		ratio_acc += w * pratio;
	}

	if (avg_dir_out) {
		float dlen = sqrtf(dir_acc[0]*dir_acc[0] + dir_acc[1]*dir_acc[1] + dir_acc[2]*dir_acc[2]);
		if (dlen > 1e-6f) {
			avg_dir_out[0] = dir_acc[0] / dlen;
			avg_dir_out[1] = dir_acc[1] / dlen;
			avg_dir_out[2] = dir_acc[2] / dlen;
		} else {
			avg_dir_out[0] = 0.0f; avg_dir_out[1] = 0.0f; avg_dir_out[2] = -1.0f;
		}
	}
	if (dist_ratio_out) *dist_ratio_out = (ratio_acc > 0.0f) ? ratio_acc : 1.0f;
	if (tot_dev_out) *tot_dev_out = dev_acc;
}

void steamaudio_dsp_probe_weights(const float point[3], const float (*probe_centers)[3],
				  uint32_t num_probes, float *weights_out)
{
	if (!weights_out || num_probes == 0) return;

	float sum = 0.0f;
	for (uint32_t i = 0; i < num_probes; i++) {
		float dx = probe_centers[i][0] - point[0];
		float dy = probe_centers[i][1] - point[1];
		float dz = probe_centers[i][2] - point[2];
		float dist = sqrtf(dx*dx + dy*dy + dz*dz) + 1e-4f;
		weights_out[i] = 1.0f / dist;
		sum += weights_out[i];
	}
	if (sum > 0.0f) {
		float inv_sum = 1.0f / sum;
		for (uint32_t i = 0; i < num_probes; i++)
			weights_out[i] *= inv_sum;
	}
}

void steamaudio_dsp_energy_field_simulate(const float source[3], const float listener[3],
					  uint32_t num_rays, uint32_t num_bounces,
					  float duration, uint32_t order,
					  float irradiance_min_distance,
					  const float room_dimensions[3],
					  uint32_t num_channels, uint32_t num_bands, uint32_t num_bins,
					  float *out_data)
{
	if (!out_data || num_channels == 0 || num_bands == 0 || num_bins == 0)
		return;

	uint32_t total_size = num_channels * num_bands * num_bins;
	memset(out_data, 0, total_size * sizeof(float));

	if (num_rays == 0) num_rays = 512;
	if (num_bounces == 0) num_bounces = 2;
	if (irradiance_min_distance < 0.1f) irradiance_min_distance = 1.0f;

	float w = (room_dimensions && room_dimensions[0] > 0.0f) ? room_dimensions[0] : 8.0f;
	float l = (room_dimensions && room_dimensions[1] > 0.0f) ? room_dimensions[1] : 10.0f;
	float h = (room_dimensions && room_dimensions[2] > 0.0f) ? room_dimensions[2] : 3.5f;

	float min_x = fminf(-w * 0.5f, fminf(source[0], listener[0]) - 0.5f);
	float max_x = fmaxf( w * 0.5f, fmaxf(source[0], listener[0]) + 0.5f);
	float min_y = fminf(-h * 0.5f, fminf(source[1], listener[1]) - 0.5f);
	float max_y = fmaxf( h * 0.5f, fmaxf(source[1], listener[1]) + 0.5f);
	float min_z = fminf(-l * 0.5f, fminf(source[2], listener[2]) - 0.5f);
	float max_z = fmaxf( l * 0.5f, fmaxf(source[2], listener[2]) + 0.5f);

	// 1. Direct path contribution
	float dir_vec[3] = { listener[0] - source[0], listener[1] - source[1], listener[2] - source[2] };
	float dir_dist = sqrtf(dir_vec[0]*dir_vec[0] + dir_vec[1]*dir_vec[1] + dir_vec[2]*dir_vec[2]);
	float dir_time = dir_dist / 343.0f;
	int dir_bin = (int)floorf(dir_time / 0.01f);
	if (dir_bin >= 0 && (uint32_t)dir_bin < num_bins) {
		float inv_dd = (dir_dist > 1e-4f) ? 1.0f / dir_dist : 1.0f;
		float un[3] = { dir_vec[0] * inv_dd, dir_vec[1] * inv_dd, dir_vec[2] * inv_dd };
		float atten = 1.0f / fmaxf(dir_dist, irradiance_min_distance);

		float sh[16];
		eval_sh_basis(un[0], un[1], un[2], (int)order, sh);

		for (uint32_t c = 0; c < num_channels && c < 16; c++) {
			for (uint32_t b = 0; b < num_bands; b++) {
				uint32_t idx = (c * num_bands + b) * num_bins + dir_bin;
				out_data[idx] += atten * sh[c];
			}
		}
	}

	// 2. Multi-bounce radiance ray marching
	float inv_rays = 1.0f / (float)num_rays;
	const float golden_ratio = 2.399963229728653f; // pi * (3 - sqrt(5))

	for (uint32_t r = 0; r < num_rays; r++) {
		// Spherical Fibonacci ray direction
		float z = 1.0f - (2.0f * (float)r + 1.0f) * inv_rays;
		float radius = sqrtf(fmaxf(0.0f, 1.0f - z * z));
		float phi = (float)r * golden_ratio;
		float dx = radius * cosf(phi);
		float dy = radius * sinf(phi);
		float dz = z;

		float p[3] = { source[0], source[1], source[2] };
		float d[3] = { dx, dy, dz };
		float accum_dist = 0.0f;
		float energy[3] = { 1.0f, 1.0f, 1.0f };

		for (uint32_t bounce = 0; bounce < num_bounces; bounce++) {
			// Ray-box intersection: find closest boundary hit
			float t_hit = 1e9f;
			float normal[3] = { 0.0f, 0.0f, 0.0f };

			if (d[0] > 1e-5f) {
				float t = (max_x - p[0]) / d[0];
				if (t > 1e-4f && t < t_hit) { t_hit = t; normal[0] = -1.0f; normal[1] = 0.0f; normal[2] = 0.0f; }
			} else if (d[0] < -1e-5f) {
				float t = (min_x - p[0]) / d[0];
				if (t > 1e-4f && t < t_hit) { t_hit = t; normal[0] = 1.0f; normal[1] = 0.0f; normal[2] = 0.0f; }
			}

			if (d[1] > 1e-5f) {
				float t = (max_y - p[1]) / d[1];
				if (t > 1e-4f && t < t_hit) { t_hit = t; normal[0] = 0.0f; normal[1] = -1.0f; normal[2] = 0.0f; }
			} else if (d[1] < -1e-5f) {
				float t = (min_y - p[1]) / d[1];
				if (t > 1e-4f && t < t_hit) { t_hit = t; normal[0] = 0.0f; normal[1] = 1.0f; normal[2] = 0.0f; }
			}

			if (d[2] > 1e-5f) {
				float t = (max_z - p[2]) / d[2];
				if (t > 1e-4f && t < t_hit) { t_hit = t; normal[0] = 0.0f; normal[1] = 0.0f; normal[2] = -1.0f; }
			} else if (d[2] < -1e-5f) {
				float t = (min_z - p[2]) / d[2];
				if (t > 1e-4f && t < t_hit) { t_hit = t; normal[0] = 0.0f; normal[1] = 0.0f; normal[2] = 1.0f; }
			}

			if (t_hit > 1e8f) break;

			p[0] += t_hit * d[0];
			p[1] += t_hit * d[1];
			p[2] += t_hit * d[2];
			accum_dist += t_hit;

			// Boundary material absorption: Low: 8%, Mid: 18%, High: 35%
			energy[0] *= 0.92f;
			energy[1] *= 0.82f;
			energy[2] *= 0.65f;

			// Radiance arrival at listener
			float to_lis[3] = { listener[0] - p[0], listener[1] - p[1], listener[2] - p[2] };
			float lis_dist = sqrtf(to_lis[0]*to_lis[0] + to_lis[1]*to_lis[1] + to_lis[2]*to_lis[2]);
			float total_time = (accum_dist + lis_dist) / 343.0f;
			int bin = (int)floorf(total_time / 0.01f);

			if (bin >= 0 && (uint32_t)bin < num_bins) {
				float inv_ld = (lis_dist > 1e-4f) ? 1.0f / lis_dist : 1.0f;
				float arr_dir[3] = { to_lis[0] * inv_ld, to_lis[1] * inv_ld, to_lis[2] * inv_ld };
				float atten = 1.0f / fmaxf(lis_dist, irradiance_min_distance);

				float sh[16];
				eval_sh_basis(arr_dir[0], arr_dir[1], arr_dir[2], (int)order, sh);

				for (uint32_t c = 0; c < num_channels && c < 16; c++) {
					for (uint32_t b = 0; b < num_bands; b++) {
						uint32_t idx = (c * num_bands + b) * num_bins + bin;
						out_data[idx] += inv_rays * atten * energy[b] * sh[c];
					}
				}
			}

			// Specular reflection for next bounce
			float dot_nd = d[0] * normal[0] + d[1] * normal[1] + d[2] * normal[2];
			d[0] -= 2.0f * dot_nd * normal[0];
			d[1] -= 2.0f * dot_nd * normal[1];
			d[2] -= 2.0f * dot_nd * normal[2];

			p[0] += 1e-3f * normal[0];
			p[1] += 1e-3f * normal[1];
			p[2] += 1e-3f * normal[2];
		}
	}
}

void steamaudio_dsp_energy_field_scale(const float *in, float scalar, float *out, uint32_t total_size)
{
	if (!in || !out || total_size == 0) return;
	for (uint32_t i = 0; i < total_size; i++)
		out[i] = in[i] * scalar;
}

void steamaudio_dsp_energy_field_add(const float *in1, const float *in2, float *out, uint32_t total_size)
{
	if (!in1 || !in2 || !out || total_size == 0) return;
	for (uint32_t i = 0; i < total_size; i++)
		out[i] = in1[i] + in2[i];
}

void steamaudio_dsp_energy_field_scale_accum(const float *in, float scalar, float *out, uint32_t total_size)
{
	if (!in || !out || total_size == 0) return;
	for (uint32_t i = 0; i < total_size; i++)
		out[i] += in[i] * scalar;
}

int steamaudio_dsp_unpack_metadata_packet(const uint8_t *data, uint32_t size,
					 struct steamaudio_compressed_metadata_packet *out_header,
					 struct steamaudio_voice_meta *out_voices, uint32_t max_voices)
{
	if (!data || size < sizeof(struct steamaudio_compressed_metadata_packet))
		return -1;

	const struct steamaudio_compressed_metadata_packet *pkt =
		(const struct steamaudio_compressed_metadata_packet *)data;

	if (pkt->sync_word != STEAMAUDIO_META_SYNC_WORD)
		return -2;

	uint32_t expected_bytes = sizeof(struct steamaudio_compressed_metadata_packet) +
				  pkt->num_active_voices * sizeof(struct steamaudio_voice_meta);
	if (size < expected_bytes || pkt->total_packet_bytes != expected_bytes)
		return -3;

	if (out_header)
		memcpy(out_header, pkt, sizeof(struct steamaudio_compressed_metadata_packet));

	if (out_voices && max_voices > 0) {
		uint32_t to_copy = (pkt->num_active_voices < max_voices) ? pkt->num_active_voices : max_voices;
		for (uint32_t i = 0; i < to_copy; i++)
			out_voices[i] = pkt->active_voices[i];
	}

	return (int)pkt->num_active_voices;
}

void steamaudio_dsp_sync_metadata_pts(struct steamaudio_comp_data *cd, uint64_t audio_pts)
{
	if (!cd) return;
	if (cd->frame_count > 0 && audio_pts > 0) {
		int64_t drift = (int64_t)audio_pts - (int64_t)cd->frame_count;
		if (drift > 128 || drift < -128) {
			cd->frame_count = audio_pts;
		}
	}
}

struct reconstruct_biquad {
	float b0, b1, b2, a1, a2;
};

static void reconstruct_calc_lowpass(float fc, float fs, struct reconstruct_biquad *c)
{
	float w0 = 2.0f * 3.14159265f * fc / fs;
	float alpha = sinf(w0) / (2.0f * 0.7071f);
	float cosw = cosf(w0);
	float a0 = 1.0f + alpha;
	c->b0 = ((1.0f - cosw) * 0.5f) / a0;
	c->b1 = (1.0f - cosw) / a0;
	c->b2 = ((1.0f - cosw) * 0.5f) / a0;
	c->a1 = (-2.0f * cosw) / a0;
	c->a2 = (1.0f - alpha) / a0;
}

static void reconstruct_calc_highpass(float fc, float fs, struct reconstruct_biquad *c)
{
	float w0 = 2.0f * 3.14159265f * fc / fs;
	float alpha = sinf(w0) / (2.0f * 0.7071f);
	float cosw = cosf(w0);
	float a0 = 1.0f + alpha;
	c->b0 = ((1.0f + cosw) * 0.5f) / a0;
	c->b1 = -(1.0f + cosw) / a0;
	c->b2 = ((1.0f + cosw) * 0.5f) / a0;
	c->a1 = (-2.0f * cosw) / a0;
	c->a2 = (1.0f - alpha) / a0;
}

static void reconstruct_calc_bandpass(float f1, float f2, float fs, struct reconstruct_biquad *c)
{
	float f0 = sqrtf(f1 * f2);
	float w0 = 2.0f * 3.14159265f * f0 / fs;
	float q = 0.5f;
	float alpha = sinf(w0) / (2.0f * q);
	float cosw = cosf(w0);
	float a0 = 1.0f + alpha;
	c->b0 = alpha / a0;
	c->b1 = 0.0f;
	c->b2 = -alpha / a0;
	c->a1 = (-2.0f * cosw) / a0;
	c->a2 = (1.0f - alpha) / a0;
}

static inline float reconstruct_fast_noise(uint32_t *seed)
{
	*seed = (*seed) * 1664525u + 1013904223u;
	return ((float)((int32_t)(*seed))) * (1.0f / 2147483648.0f);
}

static inline float reconstruct_biquad_step(const struct reconstruct_biquad *c, float state[2], float in)
{
	float out = c->b0 * in + state[0];
	state[0] = c->b1 * in - c->a1 * out + state[1];
	state[1] = c->b2 * in - c->a2 * out;
	return out;
}

void steamaudio_dsp_reconstruct_ir(const float *energy_field,
				  uint32_t num_channels,
				  uint32_t num_bands,
				  uint32_t num_bins,
				  uint32_t sampling_rate,
				  uint32_t reconstruction_type,
				  const float *air_absorption,
				  const float *distance_correction,
				  float *out_ir,
				  uint32_t num_ir_samples)
{
	if (!energy_field || !out_ir || num_channels == 0 || num_ir_samples == 0)
		return;

	const float kEnergyThreshold = 1e-7f;
	const float sqrt_4pi = 3.544907701811032f;
	uint32_t num_samples_per_bin = (sampling_rate * 10) / 1000;
	if (num_samples_per_bin == 0) num_samples_per_bin = 480;

	struct reconstruct_biquad biquads[3];
	reconstruct_calc_lowpass(800.0f, (float)sampling_rate, &biquads[0]);
	reconstruct_calc_bandpass(800.0f, 8000.0f, (float)sampling_rate, &biquads[1]);
	reconstruct_calc_highpass(8000.0f, (float)sampling_rate, &biquads[2]);

	for (uint32_t ch = 0; ch < num_channels; ch++) {
		float biquad_state[3][2] = { {0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f} };
		uint32_t noise_seed[3] = {
			0x12345678u + ch * 17u + 0u,
			0x87654321u + ch * 17u + 1u,
			0xdeadbeefu + ch * 17u + 2u
		};

		for (uint32_t n = 0; n < num_ir_samples; n++) {
			uint32_t bin = n / num_samples_per_bin;
			uint32_t bin_sample = n % num_samples_per_bin;

			if (bin >= num_bins) {
				out_ir[ch * num_ir_samples + n] = 0.0f;
				continue;
			}

			float sample_out = 0.0f;

			for (uint32_t b = 0; b < 3 && b < num_bands; b++) {
				uint32_t idx_curr = (ch * num_bands + b) * num_bins + bin;
				uint32_t idx_monopole = (0 * num_bands + b) * num_bins + bin;
				float e_curr = energy_field[idx_curr];
				float e_mono = energy_field[idx_monopole];

				float norm_energy = 0.0f;
				if (fabsf(e_curr) >= kEnergyThreshold && fabsf(e_mono) >= kEnergyThreshold) {
					norm_energy = e_curr / sqrtf(e_mono * sqrt_4pi);
				}

				float noise = reconstruct_fast_noise(&noise_seed[b]);
				float sample_in = 0.0f;

				if (reconstruction_type == 1) {
					/* Linear interpolation */
					float prev_energy = norm_energy;
					if (bin > 0) {
						uint32_t idx_prev = (ch * num_bands + b) * num_bins + (bin - 1);
						uint32_t idx_mono_prev = (0 * num_bands + b) * num_bins + (bin - 1);
						float ep = energy_field[idx_prev];
						float emp = energy_field[idx_mono_prev];
						if (fabsf(ep) >= kEnergyThreshold && fabsf(emp) >= kEnergyThreshold) {
							prev_energy = ep / sqrtf(emp * sqrt_4pi);
						}
					}
					float w = (float)bin_sample / (float)num_samples_per_bin;
					float interpolated_e = (1.0f - w) * prev_energy + w * norm_energy;
					sample_in = interpolated_e * noise;
				} else {
					/* Gaussian mode */
					if (fabsf(e_curr) >= kEnergyThreshold && fabsf(e_mono) >= kEnergyThreshold) {
						float t_mean = ((float)bin + 0.5f) * (float)num_samples_per_bin / (float)sampling_rate;
						float t = (float)n / (float)sampling_rate;
						float diff = t - t_mean;
						float g = expf(-(diff * diff) / (2.0f * 1e-5f));
						sample_in = g * noise * norm_energy;
					}
				}

				/* Air absorption */
				if (air_absorption) {
					float dist = 0.5f * 343.0f * ((float)bin + 0.5f) * (float)num_samples_per_bin / (float)sampling_rate;
					sample_in *= expf(-air_absorption[b] * dist);
				}

				/* Step biquad */
				float y_b = reconstruct_biquad_step(&biquads[b], biquad_state[b], sample_in);
				sample_out += y_b;
			}

			if (distance_correction) {
				sample_out *= distance_correction[n];
			}

			out_ir[ch * num_ir_samples + n] = sample_out;
		}
	}
}

void steamaudio_dsp_process_delay(const struct sof_steamaudio_delay_config *config,
				  float *ring_buffer,
				  uint32_t *write_cursor,
				  const float *in,
				  float *out,
				  uint32_t num_samples)
{
	if (!config || !ring_buffer || !write_cursor || !in || !out || num_samples == 0)
		return;

	uint32_t ring_size = config->max_delay_samples;
	if (ring_size == 0)
		ring_size = 2048;

	uint32_t mask = ring_size - 1;
	bool is_power_of_two = (ring_size & (ring_size - 1)) == 0;

	float cur_delay = config->delay_samples;
	float target_delay = config->target_delay_samples;
	float step_delay = (target_delay - cur_delay) / (float)num_samples;
	float doppler_rate = 1.0f - config->doppler_ratio;
	uint32_t wpos = *write_cursor;

	for (uint32_t n = 0; n < num_samples; n++) {
		if (is_power_of_two)
			ring_buffer[wpos & mask] = in[n];
		else
			ring_buffer[wpos % ring_size] = in[n];

		float rpos_f = (float)wpos - cur_delay;
		int32_t rpos_i = (int32_t)floorf(rpos_f);
		float frac = rpos_f - (float)rpos_i;

		float sample_out = 0.0f;

		if (config->interpolation_type == 1) {
			/* 4-point Cubic Hermite Interpolation (C^1 continuous) */
			int32_t idx_m1 = rpos_i - 1;
			int32_t idx_0  = rpos_i;
			int32_t idx_p1 = rpos_i + 1;
			int32_t idx_p2 = rpos_i + 2;

			if (is_power_of_two) {
				idx_m1 &= mask;
				idx_0  &= mask;
				idx_p1 &= mask;
				idx_p2 &= mask;
			} else {
				idx_m1 = (idx_m1 % (int32_t)ring_size + ring_size) % ring_size;
				idx_0  = (idx_0  % (int32_t)ring_size + ring_size) % ring_size;
				idx_p1 = (idx_p1 % (int32_t)ring_size + ring_size) % ring_size;
				idx_p2 = (idx_p2 % (int32_t)ring_size + ring_size) % ring_size;
			}

			float xm1 = ring_buffer[idx_m1];
			float x0  = ring_buffer[idx_0];
			float xp1 = ring_buffer[idx_p1];
			float xp2 = ring_buffer[idx_p2];

			float c0 = x0;
			float c1 = 0.5f * (xp1 - xm1);
			float c2 = xm1 - 2.5f * x0 + 2.0f * xp1 - 0.5f * xp2;
			float c3 = 0.5f * (xp2 - xm1) + 1.5f * (x0 - xp1);

			sample_out = ((c3 * frac + c2) * frac + c1) * frac + c0;
		} else {
			/* 2-point Linear Interpolation */
			int32_t idx_0 = rpos_i;
			int32_t idx_1 = rpos_i + 1;

			if (is_power_of_two) {
				idx_0 &= mask;
				idx_1 &= mask;
			} else {
				idx_0 = (idx_0 % (int32_t)ring_size + ring_size) % ring_size;
				idx_1 = (idx_1 % (int32_t)ring_size + ring_size) % ring_size;
			}

			sample_out = (1.0f - frac) * ring_buffer[idx_0] + frac * ring_buffer[idx_1];
		}

		out[n] = sample_out;

		wpos++;
		if (is_power_of_two)
			wpos &= mask;
		else if (wpos >= ring_size)
			wpos -= ring_size;

		cur_delay += step_delay + doppler_rate;
		if (cur_delay < 0.0f)
			cur_delay = 0.0f;
		if (cur_delay > (float)(ring_size - 4))
			cur_delay = (float)(ring_size - 4);
	}

	*write_cursor = wpos;
}

void steamaudio_dsp_process_voip(const struct sof_steamaudio_voip_config *config,
				 struct sof_steamaudio_voip_state *state,
				 const float *in,
				 float *out_left,
				 float *out_right,
				 uint32_t num_samples)
{
	if (!config || !state || !in || !out_left || !out_right || num_samples == 0)
		return;

	if (!config->enabled) {
		memset(out_left, 0, num_samples * sizeof(float));
		memset(out_right, 0, num_samples * sizeof(float));
		state->is_speaking = 0;
		return;
	}

	float fs = config->sample_rate > 0.0f ? config->sample_rate : 48000.0f;

	/* 1. 80 Hz 2nd-order Butterworth Highpass Filter */
	float w0 = 2.0f * PI * 80.0f / fs;
	float cos_w0 = fast_cos(w0);
	float sin_w0 = fast_sin(w0);
	float alpha_hp = sin_w0 / (2.0f * 0.7071f);

	float a0 = 1.0f + alpha_hp;
	float inv_a0 = 1.0f / a0;
	float hp_b0 = ((1.0f + cos_w0) * 0.5f) * inv_a0;
	float hp_b1 = (-(1.0f + cos_w0)) * inv_a0;
	float hp_b2 = ((1.0f + cos_w0) * 0.5f) * inv_a0;
	float hp_a1 = (-2.0f * cos_w0) * inv_a0;
	float hp_a2 = (1.0f - alpha_hp) * inv_a0;

	/* Envelope Follower attack/release coefficients */
	float attack_ms = config->attack_time_ms > 0.1f ? config->attack_time_ms : 5.0f;
	float release_ms = config->release_time_ms > 1.0f ? config->release_time_ms : 100.0f;
	float ga = expf(-1.0f / (attack_ms * 0.001f * fs));
	float gr = expf(-1.0f / (release_ms * 0.001f * fs));

	/* Gate threshold and AGC targets in linear amplitude */
	float gate_thresh_linear = powf(10.0f, config->gate_threshold_db / 20.0f);
	float agc_target_linear = powf(10.0f, config->agc_target_db / 20.0f);
	float agc_max_gain = powf(10.0f, config->agc_max_gain_db / 20.0f);
	if (agc_max_gain < 1.0f) agc_max_gain = 1.0f;

	/* 3D Positional Geometry */
	float rel_x = config->source_position[0] - config->listener_position[0];
	float rel_y = config->source_position[1] - config->listener_position[1];
	float rel_z = config->source_position[2] - config->listener_position[2];
	float dist_sq = rel_x * rel_x + rel_y * rel_y + rel_z * rel_z;
	float dist = fast_sqrt(dist_sq);
	if (dist < 0.1f) dist = 0.1f;

	/* Distance Attenuation: 1.0 / max(dist, 1.0) */
	float dist_gain = 1.0f / (dist > 1.0f ? dist : 1.0f);

	/* Listener local frame coordinates */
	float ah_x = config->listener_ahead[0], ah_y = config->listener_ahead[1], ah_z = config->listener_ahead[2];
	float up_x = config->listener_up[0], up_y = config->listener_up[1], up_z = config->listener_up[2];

	/* right = ahead x up */
	float rt_x = ah_y * up_z - ah_z * up_y;
	float rt_y = ah_z * up_x - ah_x * up_z;
	float rt_z = ah_x * up_y - ah_y * up_x;

	float inv_rt = fast_inv_sqrt(rt_x * rt_x + rt_y * rt_y + rt_z * rt_z + 1e-9f);
	rt_x *= inv_rt; rt_y *= inv_rt; rt_z *= inv_rt;

	float inv_ah = fast_inv_sqrt(ah_x * ah_x + ah_y * ah_y + ah_z * ah_z + 1e-9f);
	ah_x *= inv_ah; ah_y *= inv_ah; ah_z *= inv_ah;

	/* Unit direction vector towards source */
	float inv_d = 1.0f / dist;
	float dir_x = rel_x * inv_d;
	float dir_y = rel_y * inv_d;
	float dir_z = rel_z * inv_d;

	float local_right = dir_x * rt_x + dir_y * rt_y + dir_z * rt_z;
	float local_ahead = dir_x * ah_x + dir_y * ah_y + dir_z * ah_z;

	float dir_gain = 1.0f;
	if (config->directivity_weight > 0.0f) {
		float forward_factor = (local_ahead + 1.0f) * 0.5f;
		dir_gain = (1.0f - config->directivity_weight) + config->directivity_weight * forward_factor;
	}

	float pan_left = fast_sqrt(sat_clamp(0.5f * (1.0f - local_right), 0.0f, 1.0f));
	float pan_right = fast_sqrt(sat_clamp(0.5f * (1.0f + local_right), 0.0f, 1.0f));

	float spatial_scale = dist_gain * dir_gain;
	float gain_l = pan_left * spatial_scale;
	float gain_r = pan_right * spatial_scale;

	if (config->spatial_mode == 0) {
		gain_l = dist_gain * dir_gain;
		gain_r = dist_gain * dir_gain;
	}

	float env = state->env_level;
	float agc = state->agc_gain <= 0.001f ? 1.0f : state->agc_gain;
	float hp_x1 = state->hp_x1, hp_x2 = state->hp_x2;
	float hp_y1 = state->hp_y1, hp_y2 = state->hp_y2;

	uint32_t active_samples = 0;

	for (uint32_t n = 0; n < num_samples; n++) {
		float x = in[n];

		/* 1. Highpass filter */
		float y = hp_b0 * x + hp_b1 * hp_x1 + hp_b2 * hp_x2 - hp_a1 * hp_y1 - hp_a2 * hp_y2;
		hp_x2 = hp_x1; hp_x1 = x;
		hp_y2 = hp_y1; hp_y1 = y;

		/* 2. Envelope follower */
		float abs_y = fabsf(y);
		if (abs_y > env)
			env = ga * env + (1.0f - ga) * abs_y;
		else
			env = gr * env + (1.0f - gr) * abs_y;

		/* 3. Noise gate decision */
		float gate_gain = 1.0f;
		if (env < gate_thresh_linear) {
			float ratio = env / (gate_thresh_linear + 1e-9f);
			gate_gain = ratio * ratio;
		} else {
			active_samples++;
		}

		/* 4. AGC adaptation */
		if (env > gate_thresh_linear * 0.5f) {
			float target_gain = agc_target_linear / (env + 1e-4f);
			if (target_gain > agc_max_gain) target_gain = agc_max_gain;
			if (target_gain < 0.1f) target_gain = 0.1f;
			agc = 0.9995f * agc + 0.0005f * target_gain;
		}

		float voice = y * gate_gain * agc;

		/* 5. Positional rendering */
		out_left[n]  = voice * gain_l;
		out_right[n] = voice * gain_r;
	}

	state->hp_x1 = hp_x1; state->hp_x2 = hp_x2;
	state->hp_y1 = hp_y1; state->hp_y2 = hp_y2;
	state->env_level = env;
	state->agc_gain = agc;
	state->is_speaking = (active_samples > (num_samples / 4)) ? 1 : 0;
}

void steamaudio_dsp_process_multilistener(const struct sof_steamaudio_multilistener_config *config,
					  const float *in,
					  float out_channels[STEAMAUDIO_MAX_LISTENERS][STEAMAUDIO_MAX_SPEAKERS][256],
					  uint32_t frames)
{
	if (!config || !in || !out_channels || frames == 0)
		return;

	uint32_t num_listeners = config->num_listeners;
	if (num_listeners > STEAMAUDIO_MAX_LISTENERS)
		num_listeners = STEAMAUDIO_MAX_LISTENERS;

	for (uint32_t k = 0; k < num_listeners; k++) {
		const struct sof_steamaudio_listener_endpoint *lis = &config->listeners[k];

		/* Clear channels for this listener */
		for (int ch = 0; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
			for (uint32_t i = 0; i < frames; i++)
				out_channels[k][ch][i] = 0.0f;
		}

		if (lis->muted)
			continue;

		/* Relative source vector */
		float rel_x = config->source_position[0] - lis->position[0];
		float rel_y = config->source_position[1] - lis->position[1];
		float rel_z = config->source_position[2] - lis->position[2];
		float dist_sq = rel_x * rel_x + rel_y * rel_y + rel_z * rel_z;
		float dist = fast_sqrt(dist_sq);
		if (dist < 0.01f) dist = 0.01f;

		/* Distance attenuation */
		float dist_gain = 1.0f;
		if (config->distance_attenuation)
			dist_gain = 1.0f / (dist > 1.0f ? dist : 1.0f);

		float endpoint_gain = (lis->gain > 0.0f) ? lis->gain : 1.0f;
		float total_gain = dist_gain * endpoint_gain;

		/* Orthonormal basis for listener orientation */
		float ah_x = lis->ahead[0], ah_y = lis->ahead[1], ah_z = lis->ahead[2];
		float up_x = lis->up[0], up_y = lis->up[1], up_z = lis->up[2];

		/* Normalize ahead and up */
		float inv_ah = fast_inv_sqrt(ah_x * ah_x + ah_y * ah_y + ah_z * ah_z + 1e-9f);
		ah_x *= inv_ah; ah_y *= inv_ah; ah_z *= inv_ah;

		float inv_up = fast_inv_sqrt(up_x * up_x + up_y * up_y + up_z * up_z + 1e-9f);
		up_x *= inv_up; up_y *= inv_up; up_z *= inv_up;

		/* right = ahead x up */
		float rt_x = ah_y * up_z - ah_z * up_y;
		float rt_y = ah_z * up_x - ah_x * up_z;
		float rt_z = ah_x * up_y - ah_y * up_x;
		float inv_rt = fast_inv_sqrt(rt_x * rt_x + rt_y * rt_y + rt_z * rt_z + 1e-9f);
		rt_x *= inv_rt; rt_y *= inv_rt; rt_z *= inv_rt;

		/* Unit direction vector towards source */
		float inv_d = 1.0f / dist;
		float dir_x = rel_x * inv_d;
		float dir_y = rel_y * inv_d;
		float dir_z = rel_z * inv_d;

		/* Project into local coordinates: right, up, ahead */
		float loc_right = dir_x * rt_x + dir_y * rt_y + dir_z * rt_z;
		float loc_up    = dir_x * up_x + dir_y * up_y + dir_z * up_z;
		float loc_ahead = dir_x * ah_x + dir_y * ah_y + dir_z * ah_z;

		if (lis->endpoint_type == STEAMAUDIO_ENDPOINT_HEADPHONES) {
			/* Binaural Headphone Endpoint (Stereo: L=0, R=1) */
			float pan_l = fast_sqrt(sat_clamp(0.5f * (1.0f - loc_right), 0.0f, 1.0f));
			float pan_r = fast_sqrt(sat_clamp(0.5f * (1.0f + loc_right), 0.0f, 1.0f));

			/* Head shadow contralateral attenuation */
			if (loc_right > 0.0f)
				pan_l *= (1.0f - 0.25f * loc_right);
			else
				pan_r *= (1.0f + 0.25f * loc_right);

			float g_l = pan_l * total_gain;
			float g_r = pan_r * total_gain;

			for (uint32_t i = 0; i < frames; i++) {
				out_channels[k][0][i] = in[i] * g_l;
				out_channels[k][1][i] = in[i] * g_r;
			}
		} else {
			/* Surround Speaker Layout Endpoint */
			uint32_t layout = lis->speaker_layout;
			int num_speakers = lis->num_channels;
			if (num_speakers > STEAMAUDIO_MAX_SPEAKERS)
				num_speakers = STEAMAUDIO_MAX_SPEAKERS;

			struct steamaudio_panning_state pan;
			memset(&pan, 0, sizeof(pan));
			steamaudio_dsp_panning_init(&pan, layout);

			/* In panning state, direction x is right, y is up, z is -ahead */
			float pan_dir[3] = { loc_right, loc_up, -loc_ahead };
			steamaudio_dsp_panning_set_direction(&pan, pan_dir);

			for (int ch = 0; ch < num_speakers; ch++) {
				float w = pan.target_weights[ch] * total_gain;
				for (uint32_t i = 0; i < frames; i++)
					out_channels[k][ch][i] = in[i] * w;
			}
		}
	}

	/* Clear remaining inactive listener slots */
	for (uint32_t k = num_listeners; k < STEAMAUDIO_MAX_LISTENERS; k++) {
		for (int ch = 0; ch < STEAMAUDIO_MAX_SPEAKERS; ch++) {
			for (uint32_t i = 0; i < frames; i++)
				out_channels[k][ch][i] = 0.0f;
		}
	}
}

static inline void steamaudio_dsp_render(struct steamaudio_comp_data *cd, uint32_t frames)
{
	steamaudio_dsp_update_cycle_governor(cd);

	if (cd->output_mode == STEAMAUDIO_OUTPUT_SURROUND_PANNING) {
		process_direct_path(cd, cd->in_scratch, cd->out_left, frames);
		memcpy(cd->in_scratch, cd->out_left, frames * sizeof(float));

		if (cd->mute_mask & (1u << STEAMAUDIO_STEP_PANNING)) {
			memcpy(cd->out_channels[0], cd->in_scratch, frames * sizeof(float));
			memcpy(cd->out_channels[1], cd->in_scratch, frames * sizeof(float));
			for (int c = 2; c < STEAMAUDIO_MAX_SPEAKERS; c++)
				memset(cd->out_channels[c], 0, frames * sizeof(float));
		} else {
			steamaudio_dsp_panning_process(&cd->panning, cd->in_scratch, cd->out_channels, frames);
		}

		memcpy(cd->out_left, cd->out_channels[0], frames * sizeof(float));
		memcpy(cd->out_right, cd->out_channels[1], frames * sizeof(float));
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND) {
		if (cd->mute_mask & (1u << STEAMAUDIO_STEP_VIRTUAL_SURROUND)) {
			memcpy(cd->out_left, cd->in_channels[0], frames * sizeof(float));
			memcpy(cd->out_right, cd->in_channels[1], frames * sizeof(float));
		} else {
			steamaudio_dsp_virtual_surround_process(&cd->virtual_surround,
								(const float (*)[256])cd->in_channels,
								cd->out_left, cd->out_right, frames);
		}
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_AMBISONICS) {
		if (cd->mute_mask & (1u << STEAMAUDIO_STEP_AMBISONICS)) {
			if (cd->channels > 1) {
				memcpy(cd->out_left, cd->in_channels[0], frames * sizeof(float));
				memcpy(cd->out_right, cd->in_channels[1], frames * sizeof(float));
			} else {
				memcpy(cd->out_left, cd->in_scratch, frames * sizeof(float));
				memcpy(cd->out_right, cd->in_scratch, frames * sizeof(float));
			}
		} else {
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
		}
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_PATHING) {
		if (cd->mute_mask & (1u << STEAMAUDIO_STEP_PATHING)) {
			memcpy(cd->out_left, cd->in_scratch, frames * sizeof(float));
			memcpy(cd->out_right, cd->in_scratch, frames * sizeof(float));
			memcpy(cd->out_channels[0], cd->in_scratch, frames * sizeof(float));
			memcpy(cd->out_channels[1], cd->in_scratch, frames * sizeof(float));
			for (int c = 2; c < STEAMAUDIO_MAX_SPEAKERS; c++)
				memset(cd->out_channels[c], 0, frames * sizeof(float));
		} else {
			steamaudio_dsp_pathing_process(&cd->pathing, &cd->ambisonics, &cd->panning,
						       cd->in_scratch, cd->out_left, cd->out_right,
						       cd->out_channels, frames);
		}
		process_reverb(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
	} else if (cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) {
		bool muted = (cd->mute_mask & (1u << STEAMAUDIO_STEP_SCENE_UPMIX)) != 0;
		steamaudio_dsp_upmix_process(cd, cd->in_channels[0], cd->in_channels[1],
					     cd->out_channels, frames, muted);
		memcpy(cd->out_left, cd->out_channels[0], frames * sizeof(float));
		memcpy(cd->out_right, cd->out_channels[1], frames * sizeof(float));
	} else {
		process_direct_path(cd, cd->in_scratch, cd->out_left, frames);
		memcpy(cd->in_scratch, cd->out_left, frames * sizeof(float));
		if (cd->sofa_hrir.enabled && !(cd->mute_mask & (1u << STEAMAUDIO_STEP_SOFA_HRIR))) {
			steamaudio_dsp_sofa_hrir_process(&cd->sofa_hrir, cd->in_scratch,
							 cd->out_left, cd->out_right, frames, false);
		} else {
			process_binaural(cd, cd->in_scratch, cd->out_left, cd->out_right, frames);
		}
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
	uint32_t sink_ch = sink_get_channels(sink);
	if (sink_ch == 0)
		sink_ch = 2;
	uint32_t in_bytes = frames * cd->channels * sizeof(int16_t);
	uint32_t out_bytes = frames * sink_ch * sizeof(int16_t);
	int ret;

	ret = source_get_data_s16(source, in_bytes, &src, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, out_bytes, &dst, &y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	check_inband_bitstream(cd, src, in_bytes);

	const int16_t *x_end = x_start + x_size;
	int16_t *y_end = y_start + y_size;
	const int16_t *r_ptr = src;
	int16_t *w_ptr = dst;

	if ((cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND ||
	     cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) && cd->channels > 1) {
		for (uint32_t i = 0; i < frames; i++) {
			for (int ch = 0; ch < cd->channels; ch++) {
				if (r_ptr >= x_end)
					r_ptr -= x_size;
				if (ch < STEAMAUDIO_MAX_SPEAKERS)
					cd->in_channels[ch][i] = (float)*r_ptr * (1.0f / 32768.0f);
				r_ptr++;
			}
		}
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = cd->in_channels[0][i];
	} else {
		for (uint32_t i = 0; i < frames; i++) {
			for (int ch = 0; ch < cd->channels; ch++) {
				if (r_ptr >= x_end)
					r_ptr -= x_size;
				if (ch == 0)
					cd->in_scratch[i] = (float)*r_ptr * (1.0f / 32768.0f);
				r_ptr++;
			}
		}
	}

	steamaudio_dsp_render(cd, frames);

	for (uint32_t i = 0; i < frames; i++) {
		if (sink_ch > 2 && cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) {
			for (uint32_t ch = 0; ch < sink_ch; ch++) {
				float val = (ch < STEAMAUDIO_MAX_SPEAKERS) ? cd->out_channels[ch][i] : 0.0f;
				float s = sat_clamp(val * 32767.0f, -32768.0f, 32767.0f);
				if (w_ptr >= y_end)
					w_ptr -= y_size;
				*w_ptr++ = (int16_t)s;
			}
		} else {
			float l = sat_clamp(cd->out_left[i] * 32767.0f, -32768.0f, 32767.0f);
			float r = sat_clamp(cd->out_right[i] * 32767.0f, -32768.0f, 32767.0f);
			if (w_ptr >= y_end)
				w_ptr -= y_size;
			*w_ptr++ = (int16_t)l;
			if (w_ptr >= y_end)
				w_ptr -= y_size;
			*w_ptr++ = (int16_t)r;
		}
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
	uint32_t sink_ch = sink_get_channels(sink);
	if (sink_ch == 0)
		sink_ch = 2;
	uint32_t in_bytes = frames * cd->channels * sizeof(int32_t);
	uint32_t out_bytes = frames * sink_ch * sizeof(int32_t);
	int ret;

	ret = source_get_data_s32(source, in_bytes, &src, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, out_bytes, &dst, &y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	check_inband_bitstream(cd, src, in_bytes);

	const int32_t *x_end = x_start + x_size;
	int32_t *y_end = y_start + y_size;
	const int32_t *r_ptr = src;
	int32_t *w_ptr = dst;

	/* S24_4LE: 24-bit audio in 32-bit container, sign-extend Q1.23 to float */
	if ((cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND ||
	     cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) && cd->channels > 1) {
		for (uint32_t i = 0; i < frames; i++) {
			for (int ch = 0; ch < cd->channels; ch++) {
				if (r_ptr >= x_end)
					r_ptr -= x_size;
				if (ch < STEAMAUDIO_MAX_SPEAKERS) {
					int32_t val = (*r_ptr << 8) >> 8;
					cd->in_channels[ch][i] = (float)val * (1.0f / 8388608.0f);
				}
				r_ptr++;
			}
		}
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = cd->in_channels[0][i];
	} else {
		for (uint32_t i = 0; i < frames; i++) {
			for (int ch = 0; ch < cd->channels; ch++) {
				if (r_ptr >= x_end)
					r_ptr -= x_size;
				if (ch == 0) {
					int32_t val = (*r_ptr << 8) >> 8;
					cd->in_scratch[i] = (float)val * (1.0f / 8388608.0f);
				}
				r_ptr++;
			}
		}
	}

	steamaudio_dsp_render(cd, frames);

	for (uint32_t i = 0; i < frames; i++) {
		if (sink_ch > 2 && cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) {
			for (uint32_t ch = 0; ch < sink_ch; ch++) {
				float val = (ch < STEAMAUDIO_MAX_SPEAKERS) ? cd->out_channels[ch][i] : 0.0f;
				float s = sat_clamp(val * 8388608.0f, -8388608.0f, 8388607.0f);
				if (w_ptr >= y_end)
					w_ptr -= y_size;
				*w_ptr++ = (int32_t)s;
			}
		} else {
			float l = sat_clamp(cd->out_left[i] * 8388608.0f, -8388608.0f, 8388607.0f);
			float r = sat_clamp(cd->out_right[i] * 8388608.0f, -8388608.0f, 8388607.0f);
			if (w_ptr >= y_end)
				w_ptr -= y_size;
			*w_ptr++ = (int32_t)l;
			if (w_ptr >= y_end)
				w_ptr -= y_size;
			*w_ptr++ = (int32_t)r;
		}
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
	uint32_t sink_ch = sink_get_channels(sink);
	if (sink_ch == 0)
		sink_ch = 2;
	uint32_t in_bytes = frames * cd->channels * sizeof(int32_t);
	uint32_t out_bytes = frames * sink_ch * sizeof(int32_t);
	int ret;

	ret = source_get_data_s32(source, in_bytes, &src, &x_start, &x_size);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, out_bytes, &dst, &y_start, &y_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	check_inband_bitstream(cd, src, in_bytes);

	const int32_t *x_end = x_start + x_size;
	int32_t *y_end = y_start + y_size;
	const int32_t *r_ptr = src;
	int32_t *w_ptr = dst;

	if ((cd->output_mode == STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND ||
	     cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) && cd->channels > 1) {
		for (uint32_t i = 0; i < frames; i++) {
			for (int ch = 0; ch < cd->channels; ch++) {
				if (r_ptr >= x_end)
					r_ptr -= x_size;
				if (ch < STEAMAUDIO_MAX_SPEAKERS)
					cd->in_channels[ch][i] = (float)*r_ptr * (1.0f / 2147483648.0f);
				r_ptr++;
			}
		}
		for (uint32_t i = 0; i < frames; i++)
			cd->in_scratch[i] = cd->in_channels[0][i];
	} else {
		for (uint32_t i = 0; i < frames; i++) {
			for (int ch = 0; ch < cd->channels; ch++) {
				if (r_ptr >= x_end)
					r_ptr -= x_size;
				if (ch == 0)
					cd->in_scratch[i] = (float)*r_ptr * (1.0f / 2147483648.0f);
				r_ptr++;
			}
		}
	}

	steamaudio_dsp_render(cd, frames);

	for (uint32_t i = 0; i < frames; i++) {
		if (sink_ch > 2 && cd->output_mode == STEAMAUDIO_OUTPUT_SCENE_UPMIX) {
			for (uint32_t ch = 0; ch < sink_ch; ch++) {
				float val = (ch < STEAMAUDIO_MAX_SPEAKERS) ? cd->out_channels[ch][i] : 0.0f;
				float s = sat_clamp(val * 2147483647.0f, -2147483648.0f, 2147483647.0f);
				if (w_ptr >= y_end)
					w_ptr -= y_size;
				*w_ptr++ = (int32_t)s;
			}
		} else {
			float l = sat_clamp(cd->out_left[i] * 2147483647.0f, -2147483648.0f, 2147483647.0f);
			float r = sat_clamp(cd->out_right[i] * 2147483647.0f, -2147483648.0f, 2147483647.0f);
			if (w_ptr >= y_end)
				w_ptr -= y_size;
			*w_ptr++ = (int32_t)l;
			if (w_ptr >= y_end)
				w_ptr -= y_size;
			*w_ptr++ = (int32_t)r;
		}
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

/* Directional Sound Radiation & Source Directivity Patterns Implementation */
void steamaudio_dsp_directivity_init(struct dsp_directivity_state *dir)
{
	if (!dir)
		return;

	memset(dir, 0, sizeof(*dir));
	dir->source_ahead[2] = -1.0f; /* default forward = -Z */
	dir->source_up[1] = 1.0f;    /* default up = +Y */
	dir->listener_pos[2] = -1.0f; /* 1m ahead */
	dir->dipole_weight = 0.0f;   /* omnidirectional by default */
	dir->dipole_power = 1.0f;
	dir->calculated_gain = 1.0f;
	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
		dir->calculated_eq[b] = 1.0f;
	dir->current_gain = 1.0f;
}

float steamaudio_dsp_calculate_directivity(const float source_pos[3],
					   const float source_ahead[3],
					   const float listener_pos[3],
					   float dipole_weight,
					   float dipole_power)
{
	if (dipole_weight <= 0.0f)
		return 1.0f;

	float dx = listener_pos[0] - source_pos[0];
	float dy = listener_pos[1] - source_pos[1];
	float dz = listener_pos[2] - source_pos[2];
	float dist_sq = dx * dx + dy * dy + dz * dz;

	if (dist_sq < 1e-12f)
		return 1.0f;

	float inv_dist = 1.0f / fast_sqrt(dist_sq);
	float dir_x = dx * inv_dist;
	float dir_y = dy * inv_dist;
	float dir_z = dz * inv_dist;

	/* Dot product: cos(theta) = ahead . (listener - source) / dist */
	float cosine = source_ahead[0] * dir_x + source_ahead[1] * dir_y + source_ahead[2] * dir_z;
	cosine = sat_clamp(cosine, -1.0f, 1.0f);

	/* D(theta) = |(1 - w) + w * cos(theta)|^p */
	float base = fabsf((1.0f - dipole_weight) + dipole_weight * cosine);
	float gain = (dipole_power == 1.0f) ? base : powf(base, dipole_power);
	return sat_clamp(gain, 0.0f, 1.0f);
}

void steamaudio_dsp_calculate_directivity_3band(const float source_pos[3],
						const float source_ahead[3],
						const float listener_pos[3],
						float dipole_weight,
						float dipole_power,
						float out_gains[STEAMAUDIO_NUM_EQ_BANDS])
{
	if (!out_gains)
		return;

	if (dipole_weight <= 0.0f) {
		out_gains[0] = 1.0f;
		out_gains[1] = 1.0f;
		out_gains[2] = 1.0f;
		return;
	}

	float dx = listener_pos[0] - source_pos[0];
	float dy = listener_pos[1] - source_pos[1];
	float dz = listener_pos[2] - source_pos[2];
	float dist_sq = dx * dx + dy * dy + dz * dz;

	if (dist_sq < 1e-12f) {
		out_gains[0] = 1.0f;
		out_gains[1] = 1.0f;
		out_gains[2] = 1.0f;
		return;
	}

	float inv_dist = 1.0f / fast_sqrt(dist_sq);
	float dir_x = dx * inv_dist;
	float dir_y = dy * inv_dist;
	float dir_z = dz * inv_dist;

	float cosine = source_ahead[0] * dir_x + source_ahead[1] * dir_y + source_ahead[2] * dir_z;
	cosine = sat_clamp(cosine, -1.0f, 1.0f);

	/* Band 0 (Low, 400 Hz): Acoustic diffraction wraps around source; mostly omnidirectional */
	float w_low = dipole_weight * 0.25f;
	float base_low = fabsf((1.0f - w_low) + w_low * cosine);
	out_gains[0] = sat_clamp(base_low, 0.0f, 1.0f);

	/* Band 1 (Mid, 2.5 kHz): Standard nominal directivity */
	float base_mid = fabsf((1.0f - dipole_weight) + dipole_weight * cosine);
	out_gains[1] = sat_clamp((dipole_power == 1.0f) ? base_mid : powf(base_mid, dipole_power), 0.0f, 1.0f);

	/* Band 2 (High, 15 kHz): Strong directional horn/focus; sharp acoustic shadow behind emitter */
	float p_high = dipole_power * 1.5f;
	float base_high = fabsf((1.0f - dipole_weight) + dipole_weight * cosine);
	out_gains[2] = sat_clamp(powf(base_high, p_high), 0.0f, 1.0f);
}

void steamaudio_dsp_atmosphere_init(struct dsp_atmosphere_state *atm)
{
	if (!atm)
		return;

	atm->temperature_c = 20.0f;
	atm->relative_humidity = 0.5f;
	atm->pressure_kpa = 101.325f;
	atm->speed_of_sound = 343.85f;
	/* Standard Steam Audio / ISO 9613-1 defaults */
	atm->absorption_coefficients[0] = 0.0002f;
	atm->absorption_coefficients[1] = 0.0017f;
	atm->absorption_coefficients[2] = 0.0182f;
	atm->enabled = false;
	atm->flags = 0;
}

void steamaudio_dsp_calculate_atmosphere(float temp_c, float rel_hum, float pressure_kpa,
					 float *speed_of_sound, float *abs_coeffs_3band)
{
	/* Clamp inputs to valid atmospheric ranges */
	float t_c = sat_clamp(temp_c, -50.0f, 60.0f);
	float h_r = sat_clamp(rel_hum * 100.0f, 0.01f, 100.0f); /* in % */
	float p_a = (pressure_kpa > 10.0f) ? pressure_kpa : 101.325f;

	float t_k = t_c + 273.15f;
	float t0 = 293.15f;
	float t01 = 273.16f;
	float p0 = 101.325f;
	float p_r = p_a / p0;
	float t_r = t_k / t0;

	/* Saturation vapor pressure ratio per ISO 9613-1:
	 * log10(p_sat / p0) = -6.8346 * (T01 / T_K)^1.261 + 4.6151
	 */
	float log_psat = -6.8346f * powf(t01 / t_k, 1.261f) + 4.6151f;
	float psat_p0 = powf(10.0f, log_psat);
	float h = h_r * psat_p0 / p_r; /* Molar concentration of water vapor in % */

	/* Oxygen relaxation frequency:
	 * frO = p_r * (24 + 4.04e4 * h * (0.02 + h) / (0.391 + h))
	 */
	float fro = p_r * (24.0f + 4.04e4f * h * (0.02f + h) / (0.391f + h));

	/* Nitrogen relaxation frequency:
	 * frN = p_r * (T_K / T0)^(-1/2) * (9 + 280 * h * exp(-4.170 * ((T_K / T0)^(-1/3) - 1)))
	 */
	float tr_neg_half = 1.0f / fast_sqrt(t_r);
	float tr_neg_third = powf(t_r, -0.33333333f);
	float frn = p_r * tr_neg_half * (9.0f + 280.0f * h * expf(-4.170f * (tr_neg_third - 1.0f)));

	/* Speed of sound in m/s */
	if (speed_of_sound) {
		*speed_of_sound = 331.3f * fast_sqrt(t_k / 273.15f) * (1.0f + 0.16f * (h / 100.0f));
	}

	/* 3-Band attenuation coefficients (Low: 400 Hz, Mid: 2500 Hz, High: 15000 Hz) */
	if (abs_coeffs_3band) {
		static const float freqs[3] = { 400.0f, 2500.0f, 15000.0f };
		float classical = 1.84e-11f * (1.0f / p_r) * fast_sqrt(t_r);
		float tr_neg_2_5 = powf(t_r, -2.5f);
		float o2_exp = 0.01275f * expf(-2239.1f / t_k);
		float n2_exp = 0.1068f * expf(-3352.0f / t_k);

		for (int b = 0; b < 3; b++) {
			float f = freqs[b];
			float f_sq = f * f;
			float o2_term = o2_exp * fro / (fro * fro + f_sq);
			float n2_term = n2_exp * frn / (frn * frn + f_sq);
			/* Linear amplitude coefficient in Np/m */
			float alpha_lin = f_sq * (classical + tr_neg_2_5 * (o2_term + n2_term));
			abs_coeffs_3band[b] = alpha_lin;
		}
	}
}

void steamaudio_dsp_calculate_air_absorption(float distance, const float *abs_coeffs,
					     float *out_gains)
{
	if (!out_gains)
		return;

	if (!abs_coeffs || distance <= 0.0f) {
		out_gains[0] = 1.0f;
		out_gains[1] = 1.0f;
		out_gains[2] = 1.0f;
		return;
	}

	for (int b = 0; b < 3; b++) {
		float gain = expf(-abs_coeffs[b] * distance);
		out_gains[b] = sat_clamp(gain, 0.0f, 1.0f);
	}
}

/* ==============================================================================
 * Acoustic Edge Diffraction & Obstacle Pathing (BTM / UTD) DSP Implementation
 * ============================================================================== */

struct dsp_cplx {
	float r;
	float i;
};

static inline struct dsp_cplx dsp_cplx_add(struct dsp_cplx a, struct dsp_cplx b)
{
	return (struct dsp_cplx){ a.r + b.r, a.i + b.i };
}

static inline struct dsp_cplx dsp_cplx_mul(struct dsp_cplx a, struct dsp_cplx b)
{
	return (struct dsp_cplx){ a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r };
}

static inline struct dsp_cplx dsp_cplx_scale(struct dsp_cplx a, float s)
{
	return (struct dsp_cplx){ a.r * s, a.i * s };
}

static inline float dsp_cplx_mag(struct dsp_cplx a)
{
	return fast_sqrt(a.r * a.r + a.i * a.i);
}

static inline struct dsp_cplx dsp_utd_F(float x)
{
	float angle = 0.25f * (float)M_PI * fast_sqrt(x / (x + 1.4f));
	struct dsp_cplx e = { cosf(angle), sinf(angle) };

	if (x < 0.8f) {
		float term1 = fast_sqrt((float)M_PI * x);
		float term2 = 1.0f - (fast_sqrt(x) / (0.7f * fast_sqrt(x) + 1.2f));
		return dsp_cplx_scale(e, term1 * term2);
	} else {
		float term1 = 1.0f - (0.8f / ((x + 1.25f) * (x + 1.25f)));
		return dsp_cplx_scale(e, term1);
	}
}

static inline float dsp_utd_cotf(float theta)
{
	float s = fast_sin(theta);
	if (fabsf(s) < 1e-7f)
		return (s >= 0.0f) ? 1e7f : -1e7f;
	return fast_cos(theta) / s;
}

static inline float dsp_utd_N_plus(float n, float x)
{
	return (x <= (float)M_PI * (n - 1.0f)) ? 0.0f : 1.0f;
}

static inline float dsp_utd_N_minus(float n, float x)
{
	if (x < (float)M_PI * (1.0f - n))
		return -1.0f;
	else if ((float)M_PI * (1.0f - n) <= x && x <= (float)M_PI * (1.0f + n))
		return 0.0f;
	else
		return 1.0f;
}

static inline float dsp_utd_a(float n, float beta, float N)
{
	float cosine = cosf(((float)M_PI * n * N) - (0.5f * beta));
	return 2.0f * cosine * cosine;
}

static float dsp_evaluate_utd_term(float angle, float freq, float n, float L, float c)
{
	float l = c / freq;
	float k = (2.0f * (float)M_PI) / l;

	/* e = exp(-j * pi / 4) = cos(-pi/4) + j * sin(-pi/4) = sqrt(2)/2 * (1 - j) */
	struct dsp_cplx e = { 0.70710678f, -0.70710678f };
	float denom = 2.0f * n * fast_sqrt(2.0f * (float)M_PI * k);
	struct dsp_cplx D0 = { e.r / denom, e.i / denom };

	float alpha_i = 0.0f;
	float alpha_d = alpha_i + (float)M_PI + angle;

	float beta1 = alpha_d - alpha_i;
	float beta2 = alpha_d - alpha_i;
	float beta3 = alpha_d + alpha_i;
	float beta4 = alpha_d + alpha_i;

	float t1 = dsp_utd_cotf(((float)M_PI + beta1) / (2.0f * n));
	float t2 = dsp_utd_cotf(((float)M_PI - beta2) / (2.0f * n));
	float t3 = dsp_utd_cotf(((float)M_PI + beta3) / (2.0f * n));
	float t4 = dsp_utd_cotf(((float)M_PI - beta4) / (2.0f * n));

	float N1 = dsp_utd_N_plus(n, beta1);
	float N2 = dsp_utd_N_minus(n, beta2);
	float N3 = dsp_utd_N_plus(n, beta3);
	float N4 = dsp_utd_N_minus(n, beta4);

	float a1 = dsp_utd_a(n, beta1, N1);
	float a2 = dsp_utd_a(n, beta2, N2);
	float a3 = dsp_utd_a(n, beta3, N3);
	float a4 = dsp_utd_a(n, beta4, N4);

	float x1 = k * L * a1;
	float x2 = k * L * a2;
	float x3 = k * L * a3;
	float x4 = k * L * a4;

	struct dsp_cplx F1 = dsp_utd_F(x1);
	struct dsp_cplx F2 = dsp_utd_F(x2);
	struct dsp_cplx F3 = dsp_utd_F(x3);
	struct dsp_cplx F4 = dsp_utd_F(x4);

	struct dsp_cplx D1 = dsp_cplx_scale(F1, t1);
	struct dsp_cplx D2 = dsp_cplx_scale(F2, t2);
	struct dsp_cplx D3 = dsp_cplx_scale(F3, t3);
	struct dsp_cplx D4 = dsp_cplx_scale(F4, t4);

	struct dsp_cplx Dsum = dsp_cplx_add(dsp_cplx_add(D1, D2), dsp_cplx_add(D3, D4));
	struct dsp_cplx total = dsp_cplx_mul(D0, Dsum);
	return dsp_cplx_mag(total);
}

void steamaudio_dsp_diffraction_init(struct dsp_diffraction_state *diff)
{
	if (!diff)
		return;

	memset(diff, 0, sizeof(*diff));
	diff->wedge_angle_rad = 0.0f; /* 0 = Knife-edge thin screen */
	diff->deviation_angle_rad = 0.0f;
	diff->r_source = 5.0f;
	diff->r_receiver = 5.0f;
	diff->speed_of_sound = 343.85f;
	diff->transmission[0] = 0.0f;
	diff->transmission[1] = 0.0f;
	diff->transmission[2] = 0.0f;
	diff->diffraction_coeffs[0] = 1.0f;
	diff->diffraction_coeffs[1] = 1.0f;
	diff->diffraction_coeffs[2] = 1.0f;
	diff->combined_gains[0] = 1.0f;
	diff->combined_gains[1] = 1.0f;
	diff->combined_gains[2] = 1.0f;
	diff->enabled = true;
	diff->flags = 1;
}

void steamaudio_dsp_calculate_edge_diffraction(float wedge_angle_rad, float deviation_angle_rad,
					       float r_source, float r_receiver,
					       float speed_of_sound, float *out_diff_coeffs)
{
	if (!out_diff_coeffs)
		return;

	float theta_w = sat_clamp(wedge_angle_rad, 0.0f, (float)M_PI - 0.01f);
	float n = sat_clamp(2.0f - (theta_w / (float)M_PI), 1.01f, 2.0f);

	float r_s = (r_source > 0.01f) ? r_source : 5.0f;
	float r_r = (r_receiver > 0.01f) ? r_receiver : 5.0f;
	float L = (r_s * r_r) / (r_s + r_r);
	if (L < 0.01f) L = 0.01f;

	float c = (speed_of_sound > 50.0f) ? speed_of_sound : 343.85f;
	float dev = sat_clamp(deviation_angle_rad, 0.0f, (float)M_PI);

	static const float freqs[STEAMAUDIO_NUM_EQ_BANDS] = { 400.0f, 2500.0f, 15000.0f };

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		if (dev <= 1e-6f) {
			out_diff_coeffs[b] = 1.0f;
		} else {
			float val = dsp_evaluate_utd_term(dev, freqs[b], n, L, c);
			float ref = dsp_evaluate_utd_term(1e-6f, freqs[b], n, L, c);
			float gain = (ref > 1e-12f) ? (val / ref) : 1.0f;
			out_diff_coeffs[b] = sat_clamp(gain, 0.0f, 1.0f);
		}
	}
}

void steamaudio_dsp_calculate_obstacle_pathing(const float *diff_coeffs, const float *transmission,
					       float *out_combined_gains)
{
	if (!out_combined_gains)
		return;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
		float d = diff_coeffs ? diff_coeffs[b] : 1.0f;
		float t = transmission ? transmission[b] : 0.0f;
		/* Incoherent energy sum of diffracted wave and through-barrier transmission */
		float combined = fast_sqrt(d * d + t * t);
		out_combined_gains[b] = sat_clamp(combined, 0.0f, 1.0f);
	}
}

void steamaudio_dsp_probe_batch_init(struct dsp_probe_batch_state *pb)
{
	if (!pb)
		return;

	memset(pb, 0, sizeof(*pb));
	pb->num_probes = 8;
	pb->num_queries = 0;
	pb->enabled = true;
	pb->flags = 1;

	/* Initialize default 2x2x2 regular spatial probe lattice for testing/fallback */
	static const float default_coords[8][3] = {
		{ -2.0f, -2.0f, -2.0f },
		{  2.0f, -2.0f, -2.0f },
		{ -2.0f,  2.0f, -2.0f },
		{  2.0f,  2.0f, -2.0f },
		{ -2.0f, -2.0f,  2.0f },
		{  2.0f, -2.0f,  2.0f },
		{ -2.0f,  2.0f,  2.0f },
		{  2.0f,  2.0f,  2.0f }
	};

	for (int i = 0; i < 8; i++) {
		pb->probes[i].center[0] = default_coords[i][0];
		pb->probes[i].center[1] = default_coords[i][1];
		pb->probes[i].center[2] = default_coords[i][2];
		pb->probes[i].radius = 4.0f; /* 4m spherical influence */
		pb->probes[i].sh_reverb[0] = 0.8f - 0.05f * (float)i;
		pb->probes[i].sh_reverb[1] = 0.5f - 0.03f * (float)i;
		pb->probes[i].sh_reverb[2] = 0.2f - 0.01f * (float)i;
		pb->probes[i].flags = 1;
	}

	for (int q = 0; q < STEAMAUDIO_MAX_PROBE_QUERIES; q++) {
		for (int n = 0; n < STEAMAUDIO_MAX_NEIGHBORS; n++) {
			pb->neighbor_indices[q][n] = -1;
			pb->neighbor_weights[q][n] = 0.0f;
		}
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			pb->interpolated_eq[q][b] = 1.0f;
	}
}

void steamaudio_dsp_probe_batch_lookup(const struct dsp_probe *probes, uint32_t num_probes,
				       const float query_pos[3], int32_t *out_indices,
				       uint32_t max_neighbors)
{
	if (!out_indices || max_neighbors == 0)
		return;

	for (uint32_t i = 0; i < max_neighbors; i++)
		out_indices[i] = -1;

	if (!probes || !query_pos || num_probes == 0)
		return;

	/* Find all probes that contain the query position in their spherical influence */
	/* We also track distance squared for proximity ranking */
	float found_dist_sq[STEAMAUDIO_MAX_NEIGHBORS];
	uint32_t count = 0;

	for (uint32_t i = 0; i < num_probes && i < STEAMAUDIO_MAX_PROBES; i++) {
		float dx = query_pos[0] - probes[i].center[0];
		float dy = query_pos[1] - probes[i].center[1];
		float dz = query_pos[2] - probes[i].center[2];
		float dist_sq = dx * dx + dy * dy + dz * dz;
		float r = probes[i].radius;

		if (dist_sq <= r * r) {
			/* Insertion sort into neighbor array by ascending distance */
			uint32_t insert_pos = count;
			if (insert_pos > max_neighbors)
				insert_pos = max_neighbors;

			for (uint32_t j = 0; j < count && j < max_neighbors; j++) {
				if (dist_sq < found_dist_sq[j]) {
					insert_pos = j;
					break;
				}
			}

			if (insert_pos < max_neighbors) {
				uint32_t shift_limit = (count < max_neighbors) ? count : (max_neighbors - 1);
				for (int32_t s = (int32_t)shift_limit; s > (int32_t)insert_pos; s--) {
					out_indices[s] = out_indices[s - 1];
					found_dist_sq[s] = found_dist_sq[s - 1];
				}
				out_indices[insert_pos] = (int32_t)i;
				found_dist_sq[insert_pos] = dist_sq;
				if (count < max_neighbors)
					count++;
			}
		}
	}
}

void steamaudio_dsp_probe_batch_interpolate(const struct dsp_probe *probes,
					    const int32_t *neighbor_indices, uint32_t num_neighbors,
					    const float query_pos[3], float *out_weights,
					    float *out_interpolated_eq)
{
	if (!out_interpolated_eq)
		return;

	for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
		out_interpolated_eq[b] = 1.0f;

	if (out_weights) {
		for (uint32_t n = 0; n < num_neighbors; n++)
			out_weights[n] = 0.0f;
	}

	if (!probes || !neighbor_indices || !query_pos || num_neighbors == 0)
		return;

	float raw_weights[STEAMAUDIO_MAX_NEIGHBORS];
	float total_weight = 0.0f;
	uint32_t valid_neighbors = 0;

	for (uint32_t n = 0; n < num_neighbors; n++) {
		int32_t idx = neighbor_indices[n];
		if (idx >= 0 && idx < STEAMAUDIO_MAX_PROBES) {
			float dx = query_pos[0] - probes[idx].center[0];
			float dy = query_pos[1] - probes[idx].center[1];
			float dz = query_pos[2] - probes[idx].center[2];
			float dist = fast_sqrt(dx * dx + dy * dy + dz * dz);

			/* Steam Audio standard inverse distance formula: 1.0f / (dist + 1e-4f) */
			raw_weights[n] = 1.0f / (dist + 1e-4f);
			total_weight += raw_weights[n];
			valid_neighbors++;
		} else {
			raw_weights[n] = 0.0f;
		}
	}

	if (total_weight > 1e-9f && valid_neighbors > 0) {
		float inv_total = 1.0f / total_weight;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			out_interpolated_eq[b] = 0.0f;

		for (uint32_t n = 0; n < num_neighbors; n++) {
			int32_t idx = neighbor_indices[n];
			if (idx >= 0 && idx < STEAMAUDIO_MAX_PROBES) {
				float norm_w = raw_weights[n] * inv_total;
				if (out_weights)
					out_weights[n] = norm_w;
				for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
					out_interpolated_eq[b] += norm_w * probes[idx].sh_reverb[b];
			}
		}
	} else {
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			out_interpolated_eq[b] = 1.0f;
	}
}

void steamaudio_dsp_probe_batch_process(struct dsp_probe_batch_state *pb)
{
	if (!pb || !pb->enabled)
		return;

	uint32_t n_queries = pb->num_queries;
	if (n_queries > STEAMAUDIO_MAX_PROBE_QUERIES)
		n_queries = STEAMAUDIO_MAX_PROBE_QUERIES;

	for (uint32_t q = 0; q < n_queries; q++) {
		steamaudio_dsp_probe_batch_lookup(pb->probes, pb->num_probes,
						  pb->query_positions[q],
						  pb->neighbor_indices[q],
						  STEAMAUDIO_MAX_NEIGHBORS);

		steamaudio_dsp_probe_batch_interpolate(pb->probes,
						       pb->neighbor_indices[q],
						       STEAMAUDIO_MAX_NEIGHBORS,
						       pb->query_positions[q],
						       pb->neighbor_weights[q],
						       pb->interpolated_eq[q]);
	}
}

/* ========================================================================= */
/* Measured SOFA / HRIR Direct DSP Convolution API                          */
/* ========================================================================= */

void steamaudio_dsp_sofa_hrir_init(struct dsp_sofa_hrir_state *hrir)
{
	if (!hrir)
		return;

	memset(hrir, 0, sizeof(*hrir));
	hrir->num_taps = 32;
	hrir->spatial_blend = 1.0f;
	hrir->volume = 1.0f;
	hrir->direction[0] = 0.0f;
	hrir->direction[1] = 0.0f;
	hrir->direction[2] = 1.0f;
	hrir->enabled = false;
	hrir->flags = 0;
	hrir->history_idx = 0;

	/* Default front HRIR: impulse at tap 0 */
	hrir->hrir_left[0] = 1.0f;
	hrir->hrir_right[0] = 1.0f;
}

void steamaudio_dsp_sofa_hrir_set_impulse_response(struct dsp_sofa_hrir_state *hrir,
						   const float *left_taps,
						   const float *right_taps,
						   uint32_t num_taps,
						   bool reset_history)
{
	if (!hrir)
		return;

	if (num_taps > STEAMAUDIO_MAX_HRIR_TAPS)
		num_taps = STEAMAUDIO_MAX_HRIR_TAPS;
	if (num_taps == 0)
		num_taps = 1;

	hrir->num_taps = num_taps;

	if (left_taps) {
		for (uint32_t i = 0; i < num_taps; i++)
			hrir->hrir_left[i] = left_taps[i];
		for (uint32_t i = num_taps; i < STEAMAUDIO_MAX_HRIR_TAPS; i++)
			hrir->hrir_left[i] = 0.0f;
	}

	if (right_taps) {
		for (uint32_t i = 0; i < num_taps; i++)
			hrir->hrir_right[i] = right_taps[i];
		for (uint32_t i = num_taps; i < STEAMAUDIO_MAX_HRIR_TAPS; i++)
			hrir->hrir_right[i] = 0.0f;
	}

	if (reset_history) {
		memset(hrir->history, 0, sizeof(hrir->history));
		hrir->history_idx = 0;
	}
}

void steamaudio_dsp_sofa_hrir_process(struct dsp_sofa_hrir_state *hrir,
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

		for (uint32_t k = 0; k < taps; k++) {
			float s;
			int past_idx = (int)i - (int)k;
			if (past_idx >= 0) {
				s = in[past_idx];
			} else {
				/* Access sample from history buffer */
				s = hrir->history[STEAMAUDIO_MAX_HRIR_TAPS + past_idx];
			}
			acc_l += s * hl[k];
			acc_r += s * hr[k];
		}

		/* Apply spatial blend & volume factor */
		out_l[i] = ((1.0f - blend) * in_val + blend * acc_l) * vol;
		out_r[i] = ((1.0f - blend) * in_val + blend * acc_r) * vol;
	}

	/* Update history buffer with recent samples from this frame */
	if (frames >= STEAMAUDIO_MAX_HRIR_TAPS) {
		memcpy(hrir->history, &in[frames - STEAMAUDIO_MAX_HRIR_TAPS],
		       STEAMAUDIO_MAX_HRIR_TAPS * sizeof(float));
	} else {
		uint32_t keep = STEAMAUDIO_MAX_HRIR_TAPS - frames;
		memmove(hrir->history, &hrir->history[frames], keep * sizeof(float));
		memcpy(&hrir->history[keep], in, frames * sizeof(float));
	}
}

void steamaudio_dsp_graph_search_init(struct dsp_graph_search_state *gs)
{
	if (!gs)
		return;

	memset(gs, 0, sizeof(*gs));
	gs->enabled = true;
	gs->num_nodes = 0;
	gs->max_range = 100.0f;
}

void steamaudio_dsp_graph_search_set_graph(struct dsp_graph_search_state *gs,
					   uint16_t num_nodes,
					   const struct dsp_graph_node *nodes)
{
	if (!gs)
		return;

	if (num_nodes > STEAMAUDIO_MAX_GRAPH_NODES)
		num_nodes = STEAMAUDIO_MAX_GRAPH_NODES;

	gs->num_nodes = num_nodes;
	if (nodes && num_nodes > 0) {
		for (uint16_t i = 0; i < num_nodes; i++) {
			gs->nodes[i] = nodes[i];
		}
	}
}

void steamaudio_dsp_graph_search_find_path(struct dsp_graph_search_state *gs,
					   uint16_t start,
					   uint16_t target,
					   float max_range,
					   bool muted)
{
	if (!gs)
		return;

	gs->start_node = start;
	gs->target_node = target;
	gs->max_range = max_range;
	gs->num_path_nodes = 0;
	gs->total_cost = 0.0f;
	gs->path_found = false;

	if (muted) {
		/* Step 20 Muted: bypass graph search and return direct single-hop path */
		gs->num_path_nodes = 2;
		gs->path_nodes[0] = start;
		gs->path_nodes[1] = target;
		gs->total_cost = 0.0f;
		gs->path_found = true;
		return;
	}

	if (gs->num_nodes == 0 || start >= gs->num_nodes || target >= gs->num_nodes)
		return;

	if (start == target) {
		gs->num_path_nodes = 1;
		gs->path_nodes[0] = start;
		gs->total_cost = 0.0f;
		gs->path_found = true;
		return;
	}

	float costs[STEAMAUDIO_MAX_GRAPH_NODES];
	int16_t parents[STEAMAUDIO_MAX_GRAPH_NODES];
	bool visited[STEAMAUDIO_MAX_GRAPH_NODES];

	for (uint16_t i = 0; i < gs->num_nodes; i++) {
		costs[i] = 1e30f;
		parents[i] = -1;
		visited[i] = false;
	}
	costs[start] = 0.0f;

	for (uint16_t step = 0; step < gs->num_nodes; step++) {
		int best_u = -1;
		float best_cost = 1e30f;
		for (uint16_t i = 0; i < gs->num_nodes; i++) {
			if (!visited[i] && costs[i] < best_cost) {
				best_cost = costs[i];
				best_u = (int)i;
			}
		}

		if (best_u < 0 || best_cost >= 1e29f)
			break;

		if (best_u == (int)target)
			break;

		visited[best_u] = true;
		const struct dsp_graph_node *u_node = &gs->nodes[best_u];

		for (uint16_t e = 0; e < u_node->num_edges; e++) {
			uint16_t v = u_node->edges[e].node;
			if (v >= gs->num_nodes || visited[v])
				continue;

			float new_cost = costs[best_u] + u_node->edges[e].cost;
			if (max_range > 0.0f && new_cost > max_range)
				continue;

			if (new_cost < costs[v]) {
				costs[v] = new_cost;
				parents[v] = (int16_t)best_u;
			}
		}
	}

	if (parents[target] < 0) {
		/* Target not reachable */
		gs->path_found = false;
		return;
	}

	/* Backtrack path from target to start */
	uint16_t rev_path[STEAMAUDIO_MAX_PATH_NODES];
	uint16_t count = 0;
	int curr = target;

	while (curr >= 0 && count < STEAMAUDIO_MAX_PATH_NODES) {
		rev_path[count++] = (uint16_t)curr;
		if (curr == (int)start)
			break;
		curr = parents[curr];
	}

	if (count > 0 && rev_path[count - 1] == start) {
		gs->num_path_nodes = count;
		for (uint16_t i = 0; i < count; i++) {
			gs->path_nodes[i] = rev_path[count - 1 - i];
		}
		gs->total_cost = costs[target];
		gs->path_found = true;
	} else {
		gs->path_found = false;
	}
}

/* Multi-Source Reflection Mixer Coalescence Implementation */
void steamaudio_dsp_reflection_mixer_init(struct dsp_reflection_mixer_state *rm)
{
	if (!rm)
		return;

	memset(rm, 0, sizeof(*rm));
	rm->num_sources = 0;
	rm->num_channels = 2;
	rm->frames = 256;
	rm->enabled = true;
	rm->flags = 1;
	for (uint32_t s = 0; s < STEAMAUDIO_MAX_MIXER_SOURCES; s++) {
		rm->source_gains[s] = 1.0f;
	}
}

void steamaudio_dsp_reflection_mixer_reset(struct dsp_reflection_mixer_state *rm)
{
	if (!rm)
		return;

	rm->num_sources = 0;
	memset(rm->accum_buffer, 0, sizeof(rm->accum_buffer));
}

void steamaudio_dsp_reflection_mixer_accumulate(struct dsp_reflection_mixer_state *rm,
					       uint32_t source_idx,
					       const float *in,
					       uint32_t channel,
					       uint32_t frames,
					       float gain)
{
	if (!rm || !in)
		return;

	if (channel >= STEAMAUDIO_MAX_MIXER_CHANNELS)
		return;

	if (frames > STEAMAUDIO_MAX_MIXER_FRAMES)
		frames = STEAMAUDIO_MAX_MIXER_FRAMES;

	if (source_idx < STEAMAUDIO_MAX_MIXER_SOURCES) {
		gain *= rm->source_gains[source_idx];
		if (source_idx >= rm->num_sources)
			rm->num_sources = source_idx + 1;
	}

	for (uint32_t i = 0; i < frames; i++) {
		rm->accum_buffer[channel][i] += in[i] * gain;
	}

	if (channel >= rm->num_channels)
		rm->num_channels = channel + 1;
	if (frames > rm->frames)
		rm->frames = frames;
}

void steamaudio_dsp_reflection_mixer_process(struct dsp_reflection_mixer_state *rm,
					    struct sof_steamaudio_reflection_mixer_config *cfg,
					    bool muted)
{
	if (!rm || !cfg)
		return;

	uint32_t channels = rm->num_channels;
	if (channels > STEAMAUDIO_MAX_MIXER_CHANNELS)
		channels = STEAMAUDIO_MAX_MIXER_CHANNELS;
	if (channels == 0)
		channels = 2;

	uint32_t frames = rm->frames;
	if (frames > STEAMAUDIO_MAX_MIXER_FRAMES)
		frames = STEAMAUDIO_MAX_MIXER_FRAMES;
	if (frames == 0)
		frames = 256;

	if (muted) {
		/* Step 21 Muted: bit-exact mute bypass (silence / clear output) */
		for (uint32_t ch = 0; ch < channels; ch++) {
			for (uint32_t i = 0; i < frames; i++)
				cfg->mixed_output[ch][i] = 0.0f;
		}
		steamaudio_dsp_reflection_mixer_reset(rm);
		return;
	}

	for (uint32_t ch = 0; ch < channels; ch++) {
		for (uint32_t i = 0; i < frames; i++) {
			cfg->mixed_output[ch][i] = rm->accum_buffer[ch][i];
		}
	}

	/* If auto-reset after coalescence is configured, clear buffer */
	if (rm->flags & (1 << 1)) {
		steamaudio_dsp_reflection_mixer_reset(rm);
	}
}

