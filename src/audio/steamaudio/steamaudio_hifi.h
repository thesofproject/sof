// SPDX-License-Identifier: Apache-2.0
//
// Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
// Copyright (c) 2026 Intel Corporation. All rights reserved.

#ifndef __SOF_AUDIO_STEAMAUDIO_HIFI_H__
#define __SOF_AUDIO_STEAMAUDIO_HIFI_H__

#include <sof/common.h>
#include <stdint.h>
#include <stdbool.h>

#if SOF_USE_HIFI(4, STEAMAUDIO) || SOF_USE_HIFI(5, STEAMAUDIO)
#if defined(__clang__)
#include <xtensahifiintrin.h>
#elif SOF_USE_HIFI(5, STEAMAUDIO)
#include <xtensa/tie/xt_hifi5.h>
#elif SOF_USE_HIFI(4, STEAMAUDIO)
#include <xtensa/tie/xt_hifi4.h>
#endif
#endif

/* Forward declarations */
struct processing_module;
struct input_stream_buffer;
struct output_stream_buffer;
struct dsp_room_modes_state;
struct dsp_sofa_hrir_state;
struct steamaudio_ambisonics_state;
struct steamaudio_comp_data;
struct dsp_ray;
struct dsp_aabb;
struct dsp_triangle;
struct dsp_hit;
struct dsp_early_reflections_state;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ============================================================================
 * HiFi 4 Accelerated Primitives (Intel CAVS 2.5 / Spider, ACE 1.5 / Dragon Fly)
 * ============================================================================
 */
#if SOF_USE_HIFI(4, STEAMAUDIO)

void steamaudio_dsp_direct_eq_process_hifi4(struct steamaudio_comp_data *cd,
					     const float *in,
					     float *out,
					     uint32_t frames);

void steamaudio_dsp_room_modes_process_hifi4(struct dsp_room_modes_state *rms,
					     const float *in,
					     float *out,
					     uint32_t frames,
					     uint32_t num_channels,
					     bool muted);

void steamaudio_dsp_sofa_hrir_process_hifi4(struct dsp_sofa_hrir_state *hrir,
					    const float *in,
					    float *out_l, float *out_r,
					    uint32_t frames,
					    bool muted);

void steamaudio_dsp_ambisonics_rotate_hifi4(uint32_t order, const float rot[3][3],
					    const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					    float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					    uint32_t frames);

void steamaudio_dsp_ambisonics_decode_binaural_hifi4(struct steamaudio_ambisonics_state *ambi,
						     const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
						     float *out_left,
						     float *out_right,
						     uint32_t frames);

void steamaudio_dsp_reverb_process_hifi4(struct steamaudio_comp_data *cd,
					 const float *in,
					 float *out_l, float *out_r,
					 uint32_t frames);

void steamaudio_dsp_early_reflections_process_hifi4(struct dsp_early_reflections_state *ers,
						    const float *in,
						    float out[STEAMAUDIO_MAX_MIXER_CHANNELS][256],
						    uint32_t frames,
						    bool muted);

bool steamaudio_bvh_intersect_ray_aabb_hifi4(const struct dsp_ray *ray,
					     const struct dsp_aabb *box);

bool steamaudio_bvh_intersect_ray_triangle_hifi4(const struct dsp_ray *ray,
						 const struct dsp_triangle *tri,
						 struct dsp_hit *hit);

#endif /* SOF_USE_HIFI(4, STEAMAUDIO) */

/*
 * ============================================================================
 * HiFi 5 Accelerated Primitives (Intel ACE 3.0 / Aphid / Panther Lake)
 * ============================================================================
 */
#if SOF_USE_HIFI(5, STEAMAUDIO)

void steamaudio_dsp_direct_eq_process_hifi5(struct steamaudio_comp_data *cd,
					     const float *in,
					     float *out,
					     uint32_t frames);

void steamaudio_dsp_room_modes_process_hifi5(struct dsp_room_modes_state *rms,
					     const float *in,
					     float *out,
					     uint32_t frames,
					     uint32_t num_channels,
					     bool muted);

void steamaudio_dsp_sofa_hrir_process_hifi5(struct dsp_sofa_hrir_state *hrir,
					    const float *in,
					    float *out_l, float *out_r,
					    uint32_t frames,
					    bool muted);

void steamaudio_dsp_ambisonics_rotate_hifi5(uint32_t order, const float rot[3][3],
					    const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					    float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					    uint32_t frames);

void steamaudio_dsp_ambisonics_decode_binaural_hifi5(struct steamaudio_ambisonics_state *ambi,
						     const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
						     float *out_left,
						     float *out_right,
						     uint32_t frames);

void steamaudio_dsp_reverb_process_hifi5(struct steamaudio_comp_data *cd,
					 const float *in,
					 float *out_l, float *out_r,
					 uint32_t frames);

void steamaudio_dsp_early_reflections_process_hifi5(struct dsp_early_reflections_state *ers,
						    const float *in,
						    float out[STEAMAUDIO_MAX_MIXER_CHANNELS][256],
						    uint32_t frames,
						    bool muted);

bool steamaudio_bvh_intersect_ray_aabb_hifi5(const struct dsp_ray *ray,
					     const struct dsp_aabb *box);

bool steamaudio_bvh_intersect_ray_triangle_hifi5(const struct dsp_ray *ray,
						 const struct dsp_triangle *tri,
						 struct dsp_hit *hit);

#endif /* SOF_USE_HIFI(5, STEAMAUDIO) */

#ifdef __cplusplus
}
#endif

#endif /* __SOF_AUDIO_STEAMAUDIO_HIFI_H__ */
