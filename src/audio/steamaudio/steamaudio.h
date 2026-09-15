/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Steam Audio DSP Offload Engine for SOF
 */

#ifndef __SOF_AUDIO_STEAMAUDIO_H__
#define __SOF_AUDIO_STEAMAUDIO_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <stdbool.h>
#include <stdint.h>

#define STEAMAUDIO_SOF_SYNC_WORD            0x53544541  /* 'STEA' */
#define STEAMAUDIO_SOF_PROTOCOL_VERSION     0x00010000

#define STEAMAUDIO_NUM_EQ_BANDS             3
#define STEAMAUDIO_NUM_FDN_LINES            8
#define STEAMAUDIO_MAX_HRIR_TAPS            64
#define STEAMAUDIO_DELAY_LINE_SIZE          256
#define STEAMAUDIO_FDN_MAX_DELAY            2048
#define STEAMAUDIO_MAX_SPEAKERS             8
#define STEAMAUDIO_MAX_HOA_CHANNELS         16

#define STEAMAUDIO_PARAM_DIRECT_CONFIG      0x1001
#define STEAMAUDIO_PARAM_BINAURAL_CONFIG    0x1002
#define STEAMAUDIO_PARAM_AMBISONICS_CONFIG  0x1003
#define STEAMAUDIO_PARAM_REVERB_CONFIG      0x1004
#define STEAMAUDIO_PARAM_BVH_QUERY          0x1005
#define STEAMAUDIO_PARAM_BITSTREAM_MODE     0x1006
#define STEAMAUDIO_PARAM_PANNING_CONFIG     0x1007
#define STEAMAUDIO_PARAM_VIRTUAL_SURROUND_CONFIG 0x1008
#define STEAMAUDIO_PARAM_OUTPUT_MODE        0x1009

enum steamaudio_speaker_layout {
	STEAMAUDIO_SPEAKER_LAYOUT_STEREO = 0,
	STEAMAUDIO_SPEAKER_LAYOUT_QUAD = 1,
	STEAMAUDIO_SPEAKER_LAYOUT_5_1 = 2,
	STEAMAUDIO_SPEAKER_LAYOUT_7_1 = 3,
};

enum steamaudio_output_mode {
	STEAMAUDIO_OUTPUT_BINAURAL = 0,
	STEAMAUDIO_OUTPUT_SURROUND_PANNING = 1,
	STEAMAUDIO_OUTPUT_VIRTUAL_SURROUND = 2,
	STEAMAUDIO_OUTPUT_AMBISONICS = 3,
};

/* Bitstream Encapsulation Header */
struct __attribute__((packed)) steamaudio_bitstream_header {
	uint32_t sync_word;
	uint32_t protocol_version;
	uint32_t frame_seq_id;
	uint16_t num_samples;
	uint16_t num_channels;
	uint32_t payload_bytes;

	/* Direct Path */
	uint32_t direct_flags;
	uint32_t transmission_type;
	float distance_attenuation;
	float directivity;
	float occlusion;
	float air_absorption[STEAMAUDIO_NUM_EQ_BANDS];
	float transmission[STEAMAUDIO_NUM_EQ_BANDS];

	/* Spatial & Binaural */
	float direction[3];
	uint32_t hrtf_interpolation;
	float spatial_blend;

	/* Reverb */
	float reverb_times[STEAMAUDIO_NUM_EQ_BANDS];
	float reverb_eq[STEAMAUDIO_NUM_EQ_BANDS];
	uint32_t reverb_delay_samples;
	float reverb_wet_gain;
};

/* SOF IPC / ALSA kcontrol TLV structs */
struct __attribute__((packed)) sof_steamaudio_direct_config {
	uint32_t comp_type;
	uint32_t flags;
	uint32_t transmission_type;
	float distance_attenuation;
	float directivity;
	float occlusion;
	float air_absorption[STEAMAUDIO_NUM_EQ_BANDS];
	float transmission[STEAMAUDIO_NUM_EQ_BANDS];
};

struct __attribute__((packed)) sof_steamaudio_binaural_config {
	uint32_t comp_type;
	float direction[3];
	uint32_t interpolation;
	float spatial_blend;
	uint32_t hrtf_slot_id;
};

struct __attribute__((packed)) sof_steamaudio_reverb_config {
	uint32_t comp_type;
	float reverb_times[STEAMAUDIO_NUM_EQ_BANDS];
	float eq_gains[STEAMAUDIO_NUM_EQ_BANDS];
	uint32_t delay_samples;
	float wet_gain;
};

struct __attribute__((packed)) sof_steamaudio_ambisonics_config {
	uint32_t comp_type;
	uint32_t order;
	float direction[3];
	float listener_rotation[3][3];
};

struct __attribute__((packed)) sof_steamaudio_panning_config {
	uint32_t comp_type;
	uint32_t layout_type; /* enum steamaudio_speaker_layout */
	float direction[3];
};

struct __attribute__((packed)) sof_steamaudio_virtual_surround_config {
	uint32_t comp_type;
	uint32_t layout_type; /* enum steamaudio_speaker_layout (5.1 or 7.1) */
	float hrtf_blend;
};

struct __attribute__((packed)) sof_steamaudio_output_mode_config {
	uint32_t comp_type;
	uint32_t mode; /* enum steamaudio_output_mode */
};

struct __attribute__((packed)) sof_steamaudio_bvh_query {
	uint32_t comp_type;
	float source[3];
	float listener[3];
	float occlusion_result;
	uint32_t has_line_of_sight;
};

/* Internal DSP Sub-Engine States */
struct steamaudio_direct_state {
	float coeffs[2][STEAMAUDIO_NUM_EQ_BANDS][5]; /* b0, b1, b2, a1, a2 */
	float states[2][STEAMAUDIO_NUM_EQ_BANDS][2]; /* w1, w2 */
	int active_slot;
	float current_gain;
	float target_gain;
	float gain_step;
	bool needs_crossfade;
	int crossfade_remaining;
};

struct steamaudio_binaural_state {
	float delay_line[STEAMAUDIO_DELAY_LINE_SIZE];
	int write_idx;
	float hrir_left[STEAMAUDIO_MAX_HRIR_TAPS];
	float hrir_right[STEAMAUDIO_MAX_HRIR_TAPS];
	float itd_samples[2];
	float ild_gains[2];
	float spatial_blend;
	float direction[3];
};

struct steamaudio_reverb_state {
	float delay_buffers[STEAMAUDIO_NUM_FDN_LINES][STEAMAUDIO_FDN_MAX_DELAY];
	int delay_lengths[STEAMAUDIO_NUM_FDN_LINES];
	int delay_indices[STEAMAUDIO_NUM_FDN_LINES];
	float absorption[STEAMAUDIO_NUM_FDN_LINES];
	float damp_states[STEAMAUDIO_NUM_FDN_LINES];
	float wet_gain;
};

struct steamaudio_ambisonics_state {
	uint32_t order; /* 1, 2, or 3 */
	int num_channels; /* (order + 1)^2: 4, 9, or 16 */
	float direction[3];
	float rotation[3][3];
	float virtual_speaker_angles[8][2]; /* az, el for 8 cube vertices */
};

struct steamaudio_panning_state {
	uint32_t layout_type;
	int num_speakers;
	float direction[3];
	float prev_direction[3];
	float current_weights[STEAMAUDIO_MAX_SPEAKERS];
	float target_weights[STEAMAUDIO_MAX_SPEAKERS];
};

struct steamaudio_virtual_surround_state {
	uint32_t layout_type;
	int num_speakers;
	float hrtf_blend;
	float delay_lines[STEAMAUDIO_MAX_SPEAKERS][STEAMAUDIO_DELAY_LINE_SIZE];
	int write_idx[STEAMAUDIO_MAX_SPEAKERS];
	float itd_samples[STEAMAUDIO_MAX_SPEAKERS][2];
	float ild_gains[STEAMAUDIO_MAX_SPEAKERS][2];
};

/* BVH scene forward declaration */
#define STEAMAUDIO_MAX_DSP_TRIANGLES 32
#define STEAMAUDIO_MAX_DSP_BVH_NODES 64

struct dsp_vec3 {
	float x, y, z;
};

struct dsp_ray {
	struct dsp_vec3 origin;
	struct dsp_vec3 direction;
	float min_distance;
	float max_distance;
};

struct dsp_hit {
	float distance;
	struct dsp_vec3 normal;
	int triangle_index;
	bool has_hit;
};

struct dsp_triangle {
	struct dsp_vec3 v0, v1, v2;
	struct dsp_vec3 normal;
	float absorption[3];
};

struct dsp_aabb {
	struct dsp_vec3 min;
	struct dsp_vec3 max;
};

struct dsp_bvh_node {
	struct dsp_aabb bounds;
	int left_child;
	int right_child;
};

struct dsp_scene {
	uint32_t num_triangles;
	struct dsp_triangle triangles[STEAMAUDIO_MAX_DSP_TRIANGLES];
	uint32_t num_nodes;
	struct dsp_bvh_node nodes[STEAMAUDIO_MAX_DSP_BVH_NODES];
};

/* Forward declare processing function pointer */
struct steamaudio_comp_data;
typedef int (*steamaudio_func)(struct processing_module *mod,
			       struct sof_source *source,
			       struct sof_sink *sink,
			       uint32_t frames);

struct steamaudio_comp_data {
	steamaudio_func proc_func;
	int source_format;
	int frame_bytes;
	int channels;
	uint32_t sample_rate;
	bool enable;
	bool bitstream_mode;
	uint32_t output_mode; /* enum steamaudio_output_mode */

	/* DSP Subsystem contexts */
	struct steamaudio_direct_state direct;
	struct steamaudio_binaural_state binaural;
	struct steamaudio_reverb_state reverb;
	struct steamaudio_ambisonics_state ambisonics;
	struct steamaudio_panning_state panning;
	struct steamaudio_virtual_surround_state virtual_surround;
	struct dsp_scene scene;

	/* Temporary scratch buffers for processing frames */
	float in_scratch[256];
	float in_channels[STEAMAUDIO_MAX_HOA_CHANNELS][256];
	float out_channels[STEAMAUDIO_MAX_SPEAKERS][256];
	float out_left[256];
	float out_right[256];
};

/* Public component API */
steamaudio_func steamaudio_find_proc_func(enum sof_ipc_frame src_fmt);
void steamaudio_dsp_init(struct steamaudio_comp_data *cd, uint32_t sample_rate);

/* Extended DSP Sub-Engines API */
void steamaudio_dsp_panning_init(struct steamaudio_panning_state *pan, uint32_t layout_type);
void steamaudio_dsp_panning_set_direction(struct steamaudio_panning_state *pan, const float dir[3]);
void steamaudio_dsp_panning_process(struct steamaudio_panning_state *pan, const float *in,
				    float out_ch[STEAMAUDIO_MAX_SPEAKERS][256], uint32_t frames);

void steamaudio_dsp_virtual_surround_init(struct steamaudio_virtual_surround_state *vsurr,
					  uint32_t layout_type, uint32_t sample_rate);
void steamaudio_dsp_virtual_surround_process(struct steamaudio_virtual_surround_state *vsurr,
					     const float in_ch[STEAMAUDIO_MAX_SPEAKERS][256],
					     float *out_l, float *out_r, uint32_t frames);

void steamaudio_dsp_ambisonics_init(struct steamaudio_ambisonics_state *ambi, uint32_t order);
void steamaudio_dsp_ambisonics_encode(uint32_t order, const float dir[3], const float *in,
				      float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256], uint32_t frames);
void steamaudio_dsp_ambisonics_decode_binaural(struct steamaudio_ambisonics_state *ambi,
					       const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					       float *out_l, float *out_r, uint32_t frames);

/* IPC4 control callbacks */
int steamaudio_set_config(struct processing_module *mod,
			  uint32_t param_id,
			  enum module_cfg_fragment_position pos,
			  uint32_t data_offset_size,
			  const uint8_t *fragment,
			  size_t fragment_size,
			  uint8_t *response,
			  size_t response_size);

int steamaudio_get_config(struct processing_module *mod,
			  uint32_t config_id, uint32_t *data_offset_size,
			  uint8_t *fragment, size_t fragment_size);

/* BVH ray tracer API */
void steamaudio_dsp_scene_init_box_room(struct dsp_scene *scene, float width, float length, float height);
bool steamaudio_dsp_trace_closest_hit(const struct dsp_scene *scene, const struct dsp_ray *ray, struct dsp_hit *hit);
float steamaudio_dsp_test_occlusion(const struct dsp_scene *scene, struct dsp_vec3 source, struct dsp_vec3 listener);

#endif /* __SOF_AUDIO_STEAMAUDIO_H__ */
