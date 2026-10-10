/* SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
 * Copyright (c) 2026 Intel Corporation. All rights reserved.
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
#include "steamaudio_math.h"

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
#define STEAMAUDIO_PARAM_PATHING_CONFIG     0x100A
#define STEAMAUDIO_PARAM_MUTE_CONFIG        0x100B
#define STEAMAUDIO_PARAM_RECONSTRUCTION_CONFIG 0x100E
#define STEAMAUDIO_PARAM_DELAY_CONFIG       0x100F
#define STEAMAUDIO_PARAM_VOIP_CONFIG        0x1010
#define STEAMAUDIO_PARAM_MULTILISTENER_CONFIG 0x1011
#define STEAMAUDIO_PARAM_GEOMETRY_STREAM    0x1012
#define STEAMAUDIO_PARAM_DIRECTIVITY        0x1013
#define STEAMAUDIO_PARAM_ATMOSPHERE         0x1014
#define STEAMAUDIO_PARAM_DIFFRACTION        0x1015
#define STEAMAUDIO_PARAM_PROBEBATCH         0x1016
#define STEAMAUDIO_PARAM_SOFA_HRIR          0x1017
#define STEAMAUDIO_PARAM_GRAPH_SEARCH       0x1018
#define STEAMAUDIO_PARAM_REFLECTION_MIXER   0x1019
#define STEAMAUDIO_PARAM_INSTANCED_MESH     0x101A
#define STEAMAUDIO_PARAM_RAY_TRACER         0x101B
#define STEAMAUDIO_PARAM_REVERB_ESTIMATOR   0x101C
#define STEAMAUDIO_PARAM_EARLY_REFLECTIONS  0x101D
#define STEAMAUDIO_PARAM_MATERIAL_TRANSMISSION 0x101E
#define STEAMAUDIO_PARAM_ACOUSTIC_PORTALS   0x101F
#define STEAMAUDIO_PARAM_VOLUMETRIC_SOURCE  0x1020
#define STEAMAUDIO_PARAM_SOURCE_PRIORITIZATION 0x1021
#define STEAMAUDIO_PARAM_GROUND_REFLECTION  0x1022
#define STEAMAUDIO_PARAM_TRUE_PEAK_LIMITER  0x1023
#define STEAMAUDIO_PARAM_ROOM_MODES         0x1024
#define STEAMAUDIO_PARAM_ATMOSPHERIC_TURBULENCE 0x1025
#define STEAMAUDIO_PARAM_SURFACE_SCATTERING 0x1026
#define STEAMAUDIO_PARAM_SOUND_BARRIER      0x1027
#define STEAMAUDIO_PARAM_NEAR_FIELD         0x1028
#define STEAMAUDIO_PARAM_NONLINEAR_WAVE     0x1029
#define STEAMAUDIO_PARAM_RAW_SCENE          0x1030
#define STEAMAUDIO_PARAM_BATTLE_BLEED       0x1031
#define STEAMAUDIO_PARAM_VOICE_LOD          0x1032
#define STEAMAUDIO_PARAM_SCENE_UPMIX        0x1033

#define STEAMAUDIO_MAX_PROBES               64
#define STEAMAUDIO_MAX_PROBE_QUERIES        8
#define STEAMAUDIO_MAX_NEIGHBORS            8

#define STEAMAUDIO_MAX_MATERIAL_LAYERS      8
#define STEAMAUDIO_MAX_PORTALS              16
#define STEAMAUDIO_MAX_VOLUMETRIC_SOURCES   16
#define STEAMAUDIO_MAX_PRIORITY_SOURCES     32
#define STEAMAUDIO_MAX_ACTIVE_VOICES        16
#define STEAMAUDIO_MAX_LOD_SOURCES          256

#define STEAMAUDIO_MAX_GRAPH_NODES          64
#define STEAMAUDIO_MAX_GRAPH_EDGES_PER_NODE 8
#define STEAMAUDIO_MAX_PATH_NODES           16

#define STEAMAUDIO_MAX_MIXER_SOURCES        16
#define STEAMAUDIO_MAX_MIXER_CHANNELS       8
#define STEAMAUDIO_MAX_MIXER_FRAMES         256

#define STEAMAUDIO_MAX_INSTANCES            16
#define STEAMAUDIO_MAX_BLAS_PROTOTYPES      8
#define STEAMAUDIO_MAX_BLAS_TRIANGLES       32

#define STEAMAUDIO_RAY_TRACER_MAX_RAYS      16
#define STEAMAUDIO_RAY_TRACER_MAX_BOUNCES   8

#define STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS 100

#define STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS 16
#define STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE 4096

enum steamaudio_step {
	STEAMAUDIO_STEP_DIRECT = 0,
	STEAMAUDIO_STEP_BINAURAL = 1,
	STEAMAUDIO_STEP_PATHING = 2,
	STEAMAUDIO_STEP_REVERB = 3,
	STEAMAUDIO_STEP_CONVOLUTION = 4,
	STEAMAUDIO_STEP_AMBISONICS = 5,
	STEAMAUDIO_STEP_VIRTUAL_SURROUND = 6,
	STEAMAUDIO_STEP_PANNING = 7,
	STEAMAUDIO_STEP_OCCLUSION = 8,
	STEAMAUDIO_STEP_SIMULATION = 9,
	STEAMAUDIO_STEP_RECONSTRUCTION = 10,
	STEAMAUDIO_STEP_DELAY = 11,
	STEAMAUDIO_STEP_VOIP = 12,
	STEAMAUDIO_STEP_MULTILISTENER = 13,
	STEAMAUDIO_STEP_GEOMETRY = 14,
	STEAMAUDIO_STEP_DIRECTIVITY = 15,
	STEAMAUDIO_STEP_ATMOSPHERE = 16,
	STEAMAUDIO_STEP_DIFFRACTION = 17,
	STEAMAUDIO_STEP_PROBEBATCH = 18,
	STEAMAUDIO_STEP_SOFA_HRIR = 19,
	STEAMAUDIO_STEP_GRAPH_SEARCH = 20,
	STEAMAUDIO_STEP_REFLECTION_MIXER = 21,
	STEAMAUDIO_STEP_INSTANCED_MESH = 22,
	STEAMAUDIO_STEP_RAY_TRACER = 23,
	STEAMAUDIO_STEP_REVERB_ESTIMATOR = 24,
	STEAMAUDIO_STEP_EARLY_REFLECTIONS = 25,
	STEAMAUDIO_STEP_MATERIAL_TRANSMISSION = 26,
	STEAMAUDIO_STEP_ACOUSTIC_PORTALS = 27,
	STEAMAUDIO_STEP_VOLUMETRIC_SOURCE = 28,
	STEAMAUDIO_STEP_SOURCE_PRIORITIZATION = 29,
	STEAMAUDIO_STEP_GROUND_REFLECTION = 30,
	STEAMAUDIO_STEP_TRUE_PEAK_LIMITER = 31,
	STEAMAUDIO_STEP_ROOM_MODES = 32,
	STEAMAUDIO_STEP_ATMOSPHERIC_TURBULENCE = 33,
	STEAMAUDIO_STEP_SURFACE_SCATTERING = 34,
	STEAMAUDIO_STEP_SOUND_BARRIER = 35,
	STEAMAUDIO_STEP_NEAR_FIELD = 36,
	STEAMAUDIO_STEP_NONLINEAR_WAVE = 37,
	STEAMAUDIO_STEP_BATTLE_BLEED = 38,
	STEAMAUDIO_STEP_VOICE_LOD = 39,
	STEAMAUDIO_STEP_SCENE_UPMIX = 40,
	STEAMAUDIO_NUM_STEPS = 41,
};

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
	STEAMAUDIO_OUTPUT_PATHING = 4,
	STEAMAUDIO_OUTPUT_SCENE_UPMIX = 5,
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

/**
 * struct sof_steamaudio_direct_config - Direct sound path configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_DIRECT_CONFIG).
 * @flags: Bitmask enabling direct sound modeling features.
 * @transmission_type: Transmission frequency-dependent filtering model.
 * @distance_attenuation: Linear scalar distance attenuation gain.
 * @directivity: Linear source directivity radiation attenuation factor.
 * @occlusion: Geometric occlusion factor [0.0 = clear, 1.0 = fully blocked].
 * @air_absorption: 3-band air absorption EQ gain factors.
 * @transmission: 3-band material transmission EQ gain factors.
 */
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

/**
 * struct sof_steamaudio_binaural_config - Binaural HRTF spatialization configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_BINAURAL_CONFIG).
 * @direction: 3D unit vector pointing from listener to audio source (x, y, z).
 * @interpolation: HRTF interpolation algorithm (nearest or bilinear).
 * @spatial_blend: Blend factor between unprocessed audio and spatialized audio [0.0 - 1.0].
 * @hrtf_slot_id: Loaded HRTF profile index / slot identifier.
 */
struct __attribute__((packed)) sof_steamaudio_binaural_config {
	uint32_t comp_type;
	float direction[3];
	uint32_t interpolation;
	float spatial_blend;
	uint32_t hrtf_slot_id;
};

/**
 * struct sof_steamaudio_reverb_config - Room reverberation configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_REVERB_CONFIG).
 * @reverb_times: Reverberation decay time (T60) across 3 frequency bands in seconds.
 * @eq_gains: Equalization gains across 3 frequency bands for reverb tail.
 * @delay_samples: Predelay buffer length in samples.
 * @wet_gain: Linear wet mix gain scalar.
 */
struct __attribute__((packed)) sof_steamaudio_reverb_config {
	uint32_t comp_type;
	float reverb_times[STEAMAUDIO_NUM_EQ_BANDS];
	float eq_gains[STEAMAUDIO_NUM_EQ_BANDS];
	uint32_t delay_samples;
	float wet_gain;
};

/**
 * struct sof_steamaudio_ambisonics_config - Higher-Order Ambisonics soundfield configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_AMBISONICS_CONFIG).
 * @order: Spherical harmonics Ambisonics order (1 to 3).
 * @direction: Source incidence vector.
 * @listener_rotation: 3x3 listener coordinate rotation matrix.
 */
struct __attribute__((packed)) sof_steamaudio_ambisonics_config {
	uint32_t comp_type;
	uint32_t order;
	float direction[3];
	float listener_rotation[3][3];
};

/**
 * struct sof_steamaudio_panning_config - Multi-channel surround panning configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_PANNING_CONFIG).
 * @layout_type: Target loudspeaker layout (stereo, quad, 5.1, or 7.1).
 * @direction: Source panning direction vector.
 */
struct __attribute__((packed)) sof_steamaudio_panning_config {
	uint32_t comp_type;
	uint32_t layout_type; /* enum steamaudio_speaker_layout */
	float direction[3];
};

/**
 * struct sof_steamaudio_virtual_surround_config - Virtual surround sound configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_VIRTUAL_SURROUND_CONFIG).
 * @layout_type: Virtualized multi-channel layout (5.1 or 7.1).
 * @hrtf_blend: Cross-bleed blend factor for headphone virtualization.
 */
struct __attribute__((packed)) sof_steamaudio_virtual_surround_config {
	uint32_t comp_type;
	uint32_t layout_type; /* enum steamaudio_speaker_layout (5.1 or 7.1) */
	float hrtf_blend;
};

/**
 * struct sof_steamaudio_output_mode_config - Output rendering mode selector
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_OUTPUT_MODE).
 * @mode: Active output rendering pipeline mode.
 */
struct __attribute__((packed)) sof_steamaudio_output_mode_config {
	uint32_t comp_type;
	uint32_t mode; /* enum steamaudio_output_mode */
};

/**
 * struct sof_steamaudio_bvh_query - Line-of-sight ray tracing occlusion query
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_BVH_QUERY).
 * @source: 3D origin coordinates of source (x, y, z).
 * @listener: 3D destination coordinates of listener (x, y, z).
 * @occlusion_result: Evaluated occlusion factor [0.0 = visible, 1.0 = occluded].
 * @has_line_of_sight: Binary flag indicating uninterrupted line-of-sight (1) or obstruction (0).
 */
struct __attribute__((packed)) sof_steamaudio_bvh_query {
	uint32_t comp_type;
	float source[3];
	float listener[3];
	float occlusion_result;
	uint32_t has_line_of_sight;
};

/**
 * struct sof_steamaudio_pathing_config - Diffraction and pathing configuration
 * @comp_type: Component type / parameter ID (STEAMAUDIO_PARAM_PATHING_CONFIG).
 * @eq_coeffs: 3-band diffraction transmission EQ attenuation coefficients.
 * @sh_coeffs: Spherical harmonics coefficients representing arrival direction.
 * @order: Ambisonics encoding order.
 * @binaural: Flag enabling binaural decoding to stereo headphones.
 * @listener_rotation: 3x3 listener coordinate space rotation.
 */
struct __attribute__((packed)) sof_steamaudio_pathing_config {
	uint32_t comp_type;
	float eq_coeffs[STEAMAUDIO_NUM_EQ_BANDS]; /* 3-band diffraction transmission EQ */
	float sh_coeffs[STEAMAUDIO_MAX_HOA_CHANNELS]; /* Ambisonics SH arrival soundfield */
	uint32_t order; /* Ambisonics order (0, 1, 2, or 3) */
	uint32_t binaural; /* 1: Binaural headphone decode, 0: Panning decode */
	float listener_rotation[3][3]; /* Listener coordinate space rotation */
};

struct __attribute__((packed)) sof_steamaudio_path_sim_config {
	uint32_t comp_type;
	float source[3];
	float listener[3];
	uint32_t num_paths;
	uint32_t order;
	float eq_gains[STEAMAUDIO_NUM_EQ_BANDS];
	float sh_coeffs[STEAMAUDIO_MAX_HOA_CHANNELS];
	float avg_direction[3];
	float distance_ratio;
	float total_deviation;
};

struct __attribute__((packed)) sof_steamaudio_energy_field_config {
	uint32_t comp_type;
	float source[3];
	float listener[3];
	uint32_t num_rays;
	uint32_t num_bounces;
	float duration;
	uint32_t order;
	float irradiance_min_distance;
	float room_dimensions[3];
	uint32_t num_channels;
	uint32_t num_bands;
	uint32_t num_bins;
};

struct __attribute__((packed)) sof_steamaudio_mute_config {
	uint32_t comp_type;
	uint32_t mute_mask;
	uint32_t mute_mask_hi;
};

struct __attribute__((packed)) sof_steamaudio_reconstruction_config {
	uint32_t comp_type;
	uint32_t reconstruction_type; /* 0: Gaussian, 1: Linear */
	uint32_t duration_samples;
	uint32_t order;
	uint32_t num_channels;
	uint32_t num_bands;
	uint32_t num_bins;
	float air_absorption[STEAMAUDIO_NUM_EQ_BANDS];
};

#define STEAMAUDIO_META_SYNC_WORD           0x534D5441  /* 'SMTA' */
#define STEAMAUDIO_MAX_VOICES               64

enum steamaudio_voice_flags {
	STEAMAUDIO_VOICE_FLAG_ACTIVE     = (1 << 0),
	STEAMAUDIO_VOICE_FLAG_OCCLUDED   = (1 << 1),
	STEAMAUDIO_VOICE_FLAG_RAMP_RESET = (1 << 2),
	STEAMAUDIO_VOICE_FLAG_DOPPLER    = (1 << 3),
};

struct __attribute__((packed)) steamaudio_voice_meta {
	uint8_t  voice_slot;
	uint8_t  flags;
	uint16_t reserved;
	float    position[3];
	float    distance_gain;
	float    occlusion_factor;
	float    air_absorption[STEAMAUDIO_NUM_EQ_BANDS];
	float    reverb_send;
};

struct __attribute__((packed)) steamaudio_compressed_metadata_packet {
	uint32_t sync_word;
	uint32_t packet_seq_id;
	uint32_t total_packet_bytes;
	uint32_t frame_samples;
	uint64_t presentation_pts;
	float listener_rotation[3][3];
	float listener_velocity[3];
	uint64_t active_sources_mask;
	uint32_t num_active_voices;
	uint32_t flags;
	struct steamaudio_voice_meta active_voices[];
};

/* Raw Autonomous Scene State Packet (Thin Host Shim -> Autonomous DSP) */
#define STEAMAUDIO_RAW_SCENE_MAGIC 0x534D5441 /* 'SMTA' */
#define STEAMAUDIO_RAW_SCENE_VERSION 0x00010001

struct __attribute__((packed, aligned(128))) raw_emitter_descriptor {
	uint32_t source_id;
	uint32_t flags;
	float    pos[3];
	float    velocity[3];
	float    ahead[3];
	float    up[3];
	float    source_spl_db;
	float    directivity_weight;
	float    directivity_power;
	float    min_distance;
	float    max_distance;
	float    volumetric_radius;
	float    occlusion_factor;
	float    transmission_low;
	float    transmission_mid;
	float    transmission_high;
	uint32_t reserved[8];
};

struct __attribute__((packed, aligned(128))) raw_scene_packet {
	uint32_t sync_word;
	uint32_t version;
	uint32_t seq_id;
	uint32_t num_emitters;
	uint32_t frame_index;
	uint32_t num_samples;
	uint32_t header_bytes;
	uint32_t emitter_stride;
	float    listener_pos[3];
	float    listener_rotation[3][3];
	float    listener_velocity[3];
	float    ambient_temp_c;
	float    ambient_humidity;
	float    room_dimensions[3];
	uint32_t flags;
	uint32_t pcm_offset;
	uint32_t pcm_bytes;
	uint32_t total_frame_bytes;
	struct raw_emitter_descriptor emitters[];
};

STATIC_ASSERT(sizeof(struct raw_emitter_descriptor) == 128, raw_emitter_descriptor_must_be_128_bytes);
STATIC_ASSERT(sizeof(struct raw_scene_packet) == 128, raw_scene_packet_header_must_be_128_bytes);

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
	uint32_t flags;
	uint32_t transmission_type;
	float distance_attenuation;
	float directivity;
	float occlusion;
	float air_absorption[STEAMAUDIO_NUM_EQ_BANDS];
	float transmission[STEAMAUDIO_NUM_EQ_BANDS];
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
	uint32_t interpolation;
	uint32_t hrtf_slot_id;
};

struct steamaudio_reverb_state {
	float delay_buffers[STEAMAUDIO_NUM_FDN_LINES][STEAMAUDIO_FDN_MAX_DELAY];
	int delay_lengths[STEAMAUDIO_NUM_FDN_LINES];
	int delay_indices[STEAMAUDIO_NUM_FDN_LINES];
	float absorption[STEAMAUDIO_NUM_FDN_LINES];
	float damp_states[STEAMAUDIO_NUM_FDN_LINES];
	float wet_gain;
	float reverb_times[STEAMAUDIO_NUM_EQ_BANDS];
	float eq_gains[STEAMAUDIO_NUM_EQ_BANDS];
};

#define STEAMAUDIO_MAX_HYBRID_DELAY 4096

struct steamaudio_hybrid_reverb_state {
	float transition_delay_buffer[STEAMAUDIO_MAX_HYBRID_DELAY];
	int delay_write_ptr;
	int delay_samples;
	float eq_coeffs[STEAMAUDIO_NUM_EQ_BANDS];
	float eq_states[STEAMAUDIO_NUM_EQ_BANDS][2];
	float wet_gain;
	bool active;
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

struct steamaudio_pathing_state {
	float eq_coeffs[STEAMAUDIO_NUM_EQ_BANDS];
	float sh_coeffs[STEAMAUDIO_MAX_HOA_CHANNELS];
	uint32_t order;
	int num_channels;
	bool binaural;
	float rotation[3][3];
	float filter_coeffs[STEAMAUDIO_NUM_EQ_BANDS][5];
	float filter_states[STEAMAUDIO_NUM_EQ_BANDS][2];
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

#define STEAMAUDIO_MAX_DYNAMIC_TRIANGLES 64

struct dsp_dynamic_geometry {
	uint32_t num_triangles;
	struct dsp_triangle triangles[STEAMAUDIO_MAX_DYNAMIC_TRIANGLES];
	float transmission[STEAMAUDIO_MAX_DYNAMIC_TRIANGLES][3];
	uint32_t mesh_ids[STEAMAUDIO_MAX_DYNAMIC_TRIANGLES];
	uint32_t ring_write_seq;
};

struct dsp_directivity_state {
	float source_pos[3];
	float source_ahead[3];
	float source_up[3];
	float listener_pos[3];
	float dipole_weight;
	float dipole_power;
	uint32_t freq_dependent;
	float band_weights[3];
	float band_powers[3];
	float calculated_gain;
	float calculated_eq[STEAMAUDIO_NUM_EQ_BANDS];
	float current_gain;
};

struct dsp_atmosphere_state {
	float temperature_c;     /* -50.0 to +60.0 C, default 20.0 */
	float relative_humidity; /* 0.0 to 1.0, default 0.5 (50%) */
	float pressure_kpa;      /* default 101.325 kPa */
	float speed_of_sound;    /* calculated m/s, default ~343.85 */
	float absorption_coefficients[STEAMAUDIO_NUM_EQ_BANDS]; /* calculated Np/m */
	bool enabled;
	uint32_t flags;
};

struct dsp_diffraction_state {
	float wedge_angle_rad;     /* Wedge interior angle [0, pi), 0 = knife-edge */
	float deviation_angle_rad; /* Total deviation angle behind edge [0, pi] */
	float r_source;            /* Distance from source to edge apex (m) */
	float r_receiver;          /* Distance from receiver to edge apex (m) */
	float speed_of_sound;      /* Dynamic speed of sound (m/s) */
	float transmission[STEAMAUDIO_NUM_EQ_BANDS]; /* Material transmission loss [0, 1] */
	float diffraction_coeffs[STEAMAUDIO_NUM_EQ_BANDS]; /* 3-band diffraction gains */
	float combined_gains[STEAMAUDIO_NUM_EQ_BANDS];     /* Combined diffraction + transmission */
	bool enabled;
	uint32_t flags;
};

struct dsp_probe {
	float center[3];
	float radius;
	float sh_reverb[STEAMAUDIO_NUM_EQ_BANDS]; /* 3-band baked acoustic property (reverb / EQ / energy) */
	uint32_t flags;
};

struct dsp_probe_batch_state {
	uint32_t num_probes;
	struct dsp_probe probes[STEAMAUDIO_MAX_PROBES];
	/* Multi-source query input & output */
	uint32_t num_queries;
	float query_positions[STEAMAUDIO_MAX_PROBE_QUERIES][3];
	int32_t neighbor_indices[STEAMAUDIO_MAX_PROBE_QUERIES][STEAMAUDIO_MAX_NEIGHBORS];
	float neighbor_weights[STEAMAUDIO_MAX_PROBE_QUERIES][STEAMAUDIO_MAX_NEIGHBORS];
	float interpolated_eq[STEAMAUDIO_MAX_PROBE_QUERIES][STEAMAUDIO_NUM_EQ_BANDS];
	bool enabled;
	uint32_t flags;
};

struct dsp_sofa_hrir_state {
	uint32_t num_taps;
	float hrir_left[STEAMAUDIO_MAX_HRIR_TAPS];
	float hrir_right[STEAMAUDIO_MAX_HRIR_TAPS];
	float history[STEAMAUDIO_MAX_HRIR_TAPS];
	uint32_t history_idx;
	float direction[3];
	float spatial_blend;
	float volume;
	bool enabled;
	uint32_t flags;
};

struct dsp_graph_edge {
	uint16_t node;
	float cost;
};

struct dsp_graph_node {
	uint16_t num_edges;
	struct dsp_graph_edge edges[STEAMAUDIO_MAX_GRAPH_EDGES_PER_NODE];
	float pos[3];
};

struct dsp_graph_search_state {
	uint16_t num_nodes;
	struct dsp_graph_node nodes[STEAMAUDIO_MAX_GRAPH_NODES];
	uint16_t start_node;
	uint16_t target_node;
	float max_range;
	uint16_t num_path_nodes;
	uint16_t path_nodes[STEAMAUDIO_MAX_PATH_NODES];
	float total_cost;
	bool path_found;
	bool enabled;
	uint32_t flags;
};

struct dsp_reflection_mixer_state {
	uint32_t num_sources;
	uint32_t num_channels;
	uint32_t frames;
	float source_gains[STEAMAUDIO_MAX_MIXER_SOURCES];
	float accum_buffer[STEAMAUDIO_MAX_MIXER_CHANNELS][STEAMAUDIO_MAX_MIXER_FRAMES];
	bool enabled;
	uint32_t flags;
};

struct dsp_mat4 {
	float m[16]; /* Row-major: m[0..3]=row0, m[4..7]=row1, m[8..11]=row2, m[12..15]=row3 */
};

struct dsp_instanced_mesh_instance {
	uint32_t instance_id;
	uint32_t prototype_id;
	struct dsp_mat4 transform;
	struct dsp_mat4 inv_transform;
	float transmission[3];
	bool enabled;
};

struct dsp_blas_prototype {
	uint32_t prototype_id;
	uint32_t num_triangles;
	struct dsp_triangle triangles[STEAMAUDIO_MAX_BLAS_TRIANGLES];
	float transmission[STEAMAUDIO_MAX_BLAS_TRIANGLES][3];
};

struct dsp_instanced_mesh_state {
	uint32_t num_instances;
	uint32_t num_prototypes;
	struct dsp_instanced_mesh_instance instances[STEAMAUDIO_MAX_INSTANCES];
	struct dsp_blas_prototype prototypes[STEAMAUDIO_MAX_BLAS_PROTOTYPES];
	bool enabled;
	uint32_t flags;
};

struct dsp_ray_bounce_hit {
	struct dsp_vec3 hit_point;
	struct dsp_vec3 normal;
	float distance;
	float absorption[STEAMAUDIO_NUM_EQ_BANDS];
	uint32_t surface_type; /* 0: static scene, 1: dynamic geom, 2: instanced mesh */
};

struct dsp_acoustic_ray_path {
	uint32_t ray_index;
	uint32_t num_bounces;
	float total_distance;
	float delay_ms;
	float energy[STEAMAUDIO_NUM_EQ_BANDS];
	struct dsp_vec3 arrival_dir;
	uint32_t reached_listener;
	struct dsp_ray_bounce_hit bounces[STEAMAUDIO_RAY_TRACER_MAX_BOUNCES];
};

struct dsp_ray_tracer_state {
	uint32_t max_bounces;
	float speed_of_sound;
	float irradiance_min_distance;
	float listener_radius;
	float default_material_absorption[STEAMAUDIO_NUM_EQ_BANDS];
	float scattering;
	uint32_t num_simulated_paths;
	struct dsp_acoustic_ray_path paths[STEAMAUDIO_RAY_TRACER_MAX_RAYS];
	bool enabled;
	uint32_t flags;
};

struct dsp_energy_histogram {
	uint32_t num_bins;
	float bin_duration_s;
	float bins[STEAMAUDIO_NUM_EQ_BANDS][STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS];
};

struct dsp_reverb_estimation_results {
	float rt60[STEAMAUDIO_NUM_EQ_BANDS];
	float early_energy[STEAMAUDIO_NUM_EQ_BANDS];
	float late_energy[STEAMAUDIO_NUM_EQ_BANDS];
	float total_energy[STEAMAUDIO_NUM_EQ_BANDS];
	float direct_delay_ms;
	float late_delay_ms;
	float edc[STEAMAUDIO_NUM_EQ_BANDS][STEAMAUDIO_REVERB_ESTIMATOR_MAX_BINS];
};

struct dsp_reverb_estimator_state {
	uint32_t num_bins;
	float bin_duration_s;
	float early_cutoff_s;
	float scattering;
	struct dsp_energy_histogram histogram;
	struct dsp_reverb_estimation_results results;
	bool enabled;
	uint32_t flags;
};

struct dsp_early_reflection_tap {
	float delay_ms;
	float gain[STEAMAUDIO_NUM_EQ_BANDS];
	float direction[3];
	uint32_t active;
};

struct dsp_early_reflections_state {
	float delay_line[STEAMAUDIO_EARLY_REFLECTIONS_DELAY_LINE_SIZE];
	uint32_t write_pos;
	uint32_t num_taps;
	float sample_rate;
	float speed_of_sound;
	struct dsp_early_reflection_tap taps[STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS];
	uint32_t num_channels;
	bool enabled;
	uint32_t flags;
};

enum steamaudio_material_preset {
	STEAMAUDIO_MATERIAL_CUSTOM = 0,
	STEAMAUDIO_MATERIAL_DRYWALL = 1,
	STEAMAUDIO_MATERIAL_WOOD = 2,
	STEAMAUDIO_MATERIAL_GLASS = 3,
	STEAMAUDIO_MATERIAL_CONCRETE = 4,
	STEAMAUDIO_MATERIAL_METAL = 5,
	STEAMAUDIO_MATERIAL_FABRIC = 6,
};

struct dsp_material_properties {
	float surface_density; /* kg/m^2 */
	float thickness;       /* meters */
	float transmission[STEAMAUDIO_NUM_EQ_BANDS]; /* 3-band transmission [0.0, 1.0] */
	float absorption[STEAMAUDIO_NUM_EQ_BANDS];   /* 3-band absorption [0.0, 1.0] */
	float scattering;
	uint32_t preset;      /* enum steamaudio_material_preset */
};

struct dsp_material_transmission_state {
	uint32_t num_layers;
	struct dsp_material_properties layers[STEAMAUDIO_MAX_MATERIAL_LAYERS];
	float composite_transmission[STEAMAUDIO_NUM_EQ_BANDS];
	float filter_states[2]; /* 2-pole crossover state: [0] = lowpass state, [1] = highpass state */
	float prev_input;       /* Previous input sample for high-pass differentiator */
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

struct dsp_acoustic_portal {
	float center[3];
	float normal[3];
	float dimensions[2]; /* width, height (m) */
	float area;          /* m^2 */
	float openness;      /* [0.0, 1.0] */
	int32_t room_id_front;
	int32_t room_id_back;
	uint32_t enabled;
};

struct dsp_portal_coupling_result {
	int32_t active_portal_idx;
	float direct_distance;
	float path_distance;
	float diffraction_angle_rad;
	float transmission[STEAMAUDIO_NUM_EQ_BANDS];
	float arrival_dir[3];
	float coupling_energy;
};

struct dsp_acoustic_portals_state {
	uint32_t num_portals;
	struct dsp_acoustic_portal portals[STEAMAUDIO_MAX_PORTALS];
	float filter_states[2]; /* 2-pole crossover state: [0] = lowpass state, [1] = highpass state */
	float prev_input;       /* Previous input sample for high-pass differentiator */
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

enum steamaudio_volumetric_shape {
	STEAMAUDIO_VOLUMETRIC_SHAPE_POINT = 0,
	STEAMAUDIO_VOLUMETRIC_SHAPE_SPHERE = 1,
	STEAMAUDIO_VOLUMETRIC_SHAPE_BOX = 2,
	STEAMAUDIO_VOLUMETRIC_SHAPE_CAPSULE = 3,
};

struct dsp_volumetric_source {
	uint32_t shape_type;
	float center[3];
	float params[4]; /* sphere: [0]=radius; box: [0]=hx, [1]=hy, [2]=hz; capsule: [0..2]=endpoint_b, [3]=radius */
	float energy_distribution;
	uint32_t enabled;
};

struct dsp_volumetric_spread_result {
	float closest_point[3];
	float apparent_center[3];
	float direct_distance;
	float center_distance;
	float effective_distance;
	float spread_angle_rad;
	float spread_factor;
	float direct_gain;
	float diffuse_gain;
	float speaker_weights[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_volumetric_source_state {
	uint32_t num_sources;
	struct dsp_volumetric_source sources[STEAMAUDIO_MAX_VOLUMETRIC_SOURCES];
	struct dsp_volumetric_spread_result results[STEAMAUDIO_MAX_VOLUMETRIC_SOURCES];
	float listener_pos[3];
	float listener_ahead[3];
	float listener_up[3];
	uint32_t speaker_layout;
	uint32_t num_speakers;
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

struct dsp_source_priority_input {
	uint32_t source_id;
	float position[3];
	float base_priority;       /* [0.0, 1.0] e.g. player weapon=1.0, footsteps=0.8, ambient=0.3 */
	float volume;              /* linear volume [0.0, 1.0] */
	float direct_fraction;     /* occlusion/transmission factor [0.0, 1.0] */
	uint32_t flags;            /* Bit 0: audible, Bit 1: focus/target, Bit 2: virtual */
	uint32_t enabled;
};

struct dsp_source_voice_allocation {
	uint32_t source_id;
	float calculated_priority;
	float distance;
	float fov_dot;
	float voice_gain;          /* smoothed/crossfade gain [0.0, 1.0] */
	int32_t hardware_voice_idx;/* [0..max_voices-1], or -1 if culled */
	uint32_t is_active;
	uint32_t was_active;
};

struct dsp_source_prioritization_state {
	uint32_t num_sources;
	uint32_t max_voices;       /* [1..STEAMAUDIO_MAX_ACTIVE_VOICES] */
	struct dsp_source_priority_input sources[STEAMAUDIO_MAX_PRIORITY_SOURCES];
	struct dsp_source_voice_allocation allocations[STEAMAUDIO_MAX_PRIORITY_SOURCES];
	float listener_pos[3];
	float listener_ahead[3];
	float min_audible_threshold;
	float distance_reference;
	float distance_max;
	float fov_attenuation_bias;
	float hysteresis_margin;
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

enum steamaudio_ground_material {
	STEAMAUDIO_GROUND_MATERIAL_CONCRETE = 0,
	STEAMAUDIO_GROUND_MATERIAL_SOIL = 1,
	STEAMAUDIO_GROUND_MATERIAL_GRASS = 2,
	STEAMAUDIO_GROUND_MATERIAL_WATER = 3,
	STEAMAUDIO_GROUND_MATERIAL_WOOD = 4,
	STEAMAUDIO_GROUND_MATERIAL_CARPET = 5,
};

struct dsp_ground_reflection_config {
	float ground_plane_pos[3];
	float ground_plane_normal[3];
	uint32_t material_preset;
	float reflection_coeffs[STEAMAUDIO_NUM_EQ_BANDS];
	float sound_speed;
	float source_pos[3];
	float listener_pos[3];
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

struct dsp_ground_reflection_result {
	float image_source_pos[3];
	float direct_distance;
	float reflected_distance;
	float path_difference;
	float delay_seconds;
	uint32_t delay_samples;
	float grazing_angle_rad;
	float specular_bounce_point[3];
	float interference_gains[STEAMAUDIO_NUM_EQ_BANDS];
	float composite_gain;
};

struct dsp_ground_reflection_state {
	struct dsp_ground_reflection_config config;
	struct dsp_ground_reflection_result result;
	float delay_buffer[256];
	uint32_t write_pos;
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

struct dsp_limiter_config {
	float threshold_db;
	float ceiling_db;
	float knee_width_db;
	float ratio;
	float attack_time_ms;
	float release_time_ms;
	float makeup_gain_db;
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

struct dsp_limiter_result {
	float current_gain;
	float current_gain_db;
	float max_true_peak;
	float max_true_peak_db;
	float gain_reduction_db;
};

struct dsp_limiter_state {
	struct dsp_limiter_config config;
	struct dsp_limiter_result result;
	float envelope_gain;
	float prev_peak;
	float alpha_attack;
	float alpha_release;
	float makeup_gain_linear;
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

#define STEAMAUDIO_MAX_ROOM_MODES 16

struct dsp_room_mode {
	float freq;          /* modal resonance frequency in Hz */
	float q_factor;      /* Q factor of resonance */
	float gain_db;       /* peak excitation amplitude in dB */
	float gain_linear;   /* linear gain scalar */
	uint8_t nx, ny, nz;  /* mode order indices */
	uint8_t type;        /* 0: axial, 1: tangential, 2: oblique */
};

struct __attribute__((packed)) sof_steamaudio_room_modes_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_ROOM_MODES */
	float room_dimensions[3]; /* Lx, Ly, Lz in meters */
	float wall_absorption;    /* average alpha [0.01, 0.99] */
	float source_pos[3];      /* source position in room coords */
	float listener_pos[3];    /* listener position in room coords */
	uint32_t num_modes;       /* max modes to synthesize (up to 16) */
	uint32_t sample_rate;
	uint32_t flags;           /* Bit 0: enabled, Bit 1: axial only, Bit 2: auto-tune */
};

struct dsp_room_mode_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_room_modes_result {
	float schroeder_freq;
	float t60;
	float room_volume;
	uint32_t num_active_modes;
	float peak_resonance_freq;
	float peak_resonance_db;
};

struct dsp_room_modes_state {
	struct sof_steamaudio_room_modes_config config;
	struct dsp_room_modes_result result;
	struct dsp_room_mode modes[STEAMAUDIO_MAX_ROOM_MODES];
	struct dsp_room_mode_biquad filters[STEAMAUDIO_MAX_ROOM_MODES];
	uint32_t num_active_modes;
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

/* Atmospheric Turbulence & Wind Advection Structures */
struct __attribute__((packed)) sof_steamaudio_atmospheric_turbulence_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_ATMOSPHERIC_TURBULENCE */
	float wind_velocity[3];   /* wx, wy, wz in m/s */
	float source_pos[3];      /* source position (x, y, z) in meters */
	float listener_pos[3];    /* listener position (x, y, z) in meters */
	float turbulence_intensity; /* 0.0 (calm) to 1.0 (severe storm) */
	float reference_height;   /* boundary layer height (default 10.0m) */
	float temperature_c;      /* ambient temp (default 20.0C) */
	uint32_t sample_rate;     /* sampling rate (default 48000) */
	uint32_t flags;           /* Bit 0: enabled, Bit 1: refraction only, Bit 2: turbulence only */
};

struct dsp_atmospheric_turbulence_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_atmospheric_turbulence_result {
	float effective_sound_speed; /* m/s along ray */
	float delay_delta_ms;        /* propagation delay change relative to c0 */
	float shadow_attenuation_db[STEAMAUDIO_NUM_EQ_BANDS]; /* attenuation per band */
	float scintillation_index;   /* high-freq turbulence variance */
	uint32_t is_upwind_shadow;   /* 1 if upwind shadow zone active */
};

struct dsp_atmospheric_turbulence_state {
	struct sof_steamaudio_atmospheric_turbulence_config config;
	struct dsp_atmospheric_turbulence_result result;
	struct dsp_atmospheric_turbulence_biquad eq_filter[STEAMAUDIO_NUM_EQ_BANDS];
	float lfo_phase;             /* turbulence eddy phase accumulator */
	float lfo_step;              /* phase increment per sample */
	float current_scintillation; /* smoothed amplitude modulation factor */
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

/* Surface Acoustic Scattering & Rough Boundary Diffuse Dispersion Structures */
#define STEAMAUDIO_SCATTERING_ALLPASS_STAGES 4
#define STEAMAUDIO_SCATTERING_MAX_DELAY 72

struct dsp_surface_scattering_allpass {
	float buffer[STEAMAUDIO_SCATTERING_MAX_DELAY];
	uint32_t index;
	uint32_t delay;
	float gain;
};

struct dsp_surface_scattering_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct __attribute__((packed)) sof_steamaudio_surface_scattering_config {
	uint32_t comp_type;              /* STEAMAUDIO_PARAM_SURFACE_SCATTERING */
	float roughness_rms;             /* RMS roughness height sigma_h (meters) */
	float correlation_length;        /* Correlation length lc (meters) */
	float incident_angle_rad;        /* Angle of incidence theta_i relative to normal */
	float material_absorption[STEAMAUDIO_NUM_EQ_BANDS]; /* Low, Mid, High absorption */
	float diffuse_fraction;          /* Base diffuse scattering weight [0.0, 1.0] */
	float dispersion_depth;          /* Allpass dispersion intensity [0.0, 1.0] */
	float surface_area;              /* Reflector area (m^2) */
	float distance_to_listener;      /* Distance to listener (meters) */
	uint32_t sample_rate;            /* Default 48000 */
	uint32_t flags;                  /* Bit 0: enabled, Bit 1: dispersion enabled */
};

struct dsp_surface_scattering_result {
	float scattering_coeff[STEAMAUDIO_NUM_EQ_BANDS]; /* s(f) for Low, Mid, High */
	float specular_gain[STEAMAUDIO_NUM_EQ_BANDS];    /* sqrt((1 - alpha)(1 - s)) */
	float diffuse_gain[STEAMAUDIO_NUM_EQ_BANDS];     /* sqrt((1 - alpha) * s) */
	float dispersion_delay_ms;                       /* micro-temporal spread */
	float total_reflected_energy;                    /* total energy conserved */
};

struct dsp_surface_scattering_state {
	struct sof_steamaudio_surface_scattering_config config;
	struct dsp_surface_scattering_result result;
	struct dsp_surface_scattering_biquad specular_filter[STEAMAUDIO_NUM_EQ_BANDS];
	struct dsp_surface_scattering_biquad diffuse_filter[STEAMAUDIO_NUM_EQ_BANDS];
	struct dsp_surface_scattering_allpass allpass[STEAMAUDIO_MAX_SPEAKERS][STEAMAUDIO_SCATTERING_ALLPASS_STAGES];
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

/* Sound Barrier Edge Diffraction & Maekawa Shadowing Structures */
struct __attribute__((packed)) sof_steamaudio_sound_barrier_config {
	uint32_t comp_type;             /* STEAMAUDIO_PARAM_SOUND_BARRIER */
	uint32_t barrier_type;          /* 0 = Single Knife Edge, 1 = Double Edge / Wide Berm */
	float source_pos[3];            /* Source 3D position (m) */
	float listener_pos[3];          /* Listener 3D position (m) */
	float edge_pt0[3];              /* Primary edge start point (m) */
	float edge_pt1[3];              /* Primary edge end point (m) */
	float edge2_pt0[3];             /* Secondary edge start point for double-edge (m) */
	float edge2_pt1[3];             /* Secondary edge end point for double-edge (m) */
	float barrier_height;           /* Effective barrier height (m) */
	float barrier_transmission[STEAMAUDIO_NUM_EQ_BANDS]; /* Low, Mid, High material transmission */
	float flanking_limit_db;        /* Max shadow attenuation clamp (dB, default 25.0) */
	uint32_t sample_rate;           /* Default 48000 */
	uint32_t flags;                 /* Bit 0: enabled, Bit 1: double-edge */
};

struct dsp_sound_barrier_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_sound_barrier_result {
	float path_difference_m;        /* delta path difference in meters */
	float fresnel_number[STEAMAUDIO_NUM_EQ_BANDS]; /* N(f) for Low, Mid, High */
	float barrier_attenuation_db[STEAMAUDIO_NUM_EQ_BANDS]; /* Maekawa attenuation in dB */
	float combined_gain[STEAMAUDIO_NUM_EQ_BANDS]; /* combined diffraction + transmission linear gain */
	uint32_t is_in_shadow;          /* 1 if in acoustic shadow, 0 if in bright zone */
};

struct dsp_sound_barrier_state {
	struct sof_steamaudio_sound_barrier_config config;
	struct dsp_sound_barrier_result result;
	struct dsp_sound_barrier_biquad filter[STEAMAUDIO_NUM_EQ_BANDS];
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

/* Near-Field HRIR Parallax & Proximity Effect Bass Boost Structures */
struct __attribute__((packed)) sof_steamaudio_near_field_config {
	uint32_t comp_type;             /* STEAMAUDIO_PARAM_NEAR_FIELD */
	float source_pos[3];            /* Source 3D position (m) */
	float listener_pos[3];          /* Listener 3D position (m) */
	float head_radius;              /* Listener head radius (m, default 0.0875) */
	float reference_distance;       /* Reference far-field distance (m, default 1.0) */
	float bass_boost_limit_db;      /* Maximum bass boost clamp in dB (default 18.0) */
	uint32_t sample_rate;           /* Default 48000 */
	uint32_t flags;                 /* Bit 0: enabled, Bit 1: parallax enabled, Bit 2: bass boost enabled */
};

struct dsp_near_field_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_near_field_result {
	float distance_m;               /* Nominal Euclidean distance */
	float distance_left_m;          /* Left ear distance with parallax */
	float distance_right_m;         /* Right ear distance with parallax */
	float ild_boost_db;             /* Extra near-field ILD in dB */
	float bass_boost_db;            /* Proximity low-frequency boost in dB */
	float bass_boost_gain;          /* Linear bass boost amplitude gain */
	uint32_t is_near_field;         /* 1 if r < reference_distance, else 0 */
};

struct dsp_near_field_state {
	struct sof_steamaudio_near_field_config config;
	struct dsp_near_field_result result;
	struct dsp_near_field_biquad filter; /* Low-shelf proximity filter */
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

/* Nonlinear Acoustic Propagation & Shock Wave Crest Distortion Structures */
struct __attribute__((packed)) sof_steamaudio_nonlinear_wave_config {
	uint32_t comp_type;                 /* STEAMAUDIO_PARAM_NONLINEAR_WAVE */
	float source_spl_db;                /* Peak SPL at source (dB, e.g. 140.0) */
	float distance_m;                   /* Propagation distance (m) */
	float nonlinearity_parameter_beta;  /* Parameter of nonlinearity (default 1.20 for air) */
	float shock_threshold_spl_db;       /* Threshold where nonlinear effects activate (dB, default 115.0) */
	float max_shock_dissipation_db;     /* Max thermoviscous shock dissipation clamp (dB, default 12.0) */
	uint32_t sample_rate;               /* Default 48000 */
	uint32_t flags;                     /* Bit 0: enabled, Bit 1: steepening, Bit 2: thermoviscous dissipation */
};

struct dsp_nonlinear_wave_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_nonlinear_wave_result {
	float effective_spl_db;             /* Distance-attenuated SPL at listener */
	float shock_distance_m;             /* Blackstock shock formation distance x_bar */
	float distortion_index_sigma;       /* sigma = x / x_bar */
	float thd_percent;                  /* Generated harmonic distortion percentage */
	float shock_dissipation_db;         /* Thermoviscous dissipation loss in dB */
	uint32_t has_shock_formed;          /* 1 if sigma >= 1.0, else 0 */
};

struct dsp_nonlinear_wave_state {
	struct sof_steamaudio_nonlinear_wave_config config;
	struct dsp_nonlinear_wave_result result;
	struct dsp_nonlinear_wave_biquad filter; /* High-cut dissipation filter */
	uint32_t sample_rate;
	bool enabled;
	uint32_t flags;
};

/* Playback-to-Capture Loopback & Battle Bleed Mixer Structures */
struct __attribute__((packed)) sof_steamaudio_battle_bleed_config {
	uint32_t comp_type;                 /* STEAMAUDIO_PARAM_BATTLE_BLEED */
	float bleed_volume;                 /* 0.0 to 1.0 (default 0.15 = 15%) */
	float ducking_depth_db;             /* Ducking attenuation in dB, e.g. 12.0 */
	float ducking_threshold_db;         /* Voice detection threshold in dB, e.g. -30.0 */
	float attack_time_ms;               /* Ducking attack time, e.g. 5.0 ms */
	float release_time_ms;              /* Ducking release time, e.g. 150.0 ms */
	uint32_t sample_rate;               /* Sample rate in Hz, e.g. 48000 */
	uint32_t flags;                     /* Bit 0: enabled, Bit 1: helmet acoustic filter */
	uint32_t reserved[24];              /* Pad to exact 128 bytes alignment */
};

struct dsp_battle_bleed_biquad {
	float b0, b1, b2;
	float a1, a2;
	float x1[STEAMAUDIO_MAX_SPEAKERS];
	float x2[STEAMAUDIO_MAX_SPEAKERS];
	float y1[STEAMAUDIO_MAX_SPEAKERS];
	float y2[STEAMAUDIO_MAX_SPEAKERS];
};

struct dsp_battle_bleed_result {
	float current_ducking_gain;
	float current_ducking_db;
	float mic_envelope_db;
	uint32_t is_speaking;
};

struct dsp_battle_bleed_state {
	struct sof_steamaudio_battle_bleed_config config;
	struct dsp_battle_bleed_result result;
	struct dsp_battle_bleed_biquad hp_filter; /* 150 Hz HPF */
	struct dsp_battle_bleed_biquad lp_filter; /* 4000 Hz LPF */
	float env_mic;
	float duck_gain_current;
	float alpha_attack;
	float alpha_release;
	float ducking_min_gain;
	float ducking_threshold_lin;
	uint32_t sample_rate;
	bool enabled;
	bool helmet_filter_enabled;
};

/* High Polyphony Voice Scaling & DSP 3-Tier Level-of-Detail (LOD) Structures */
enum steamaudio_lod_tier {
	STEAMAUDIO_LOD_TIER_CULLED = 0,
	STEAMAUDIO_LOD_TIER_1 = 1,      /* Near-Field: Full Binaural HRTF + Early Reflections + Air Absorption EQ */
	STEAMAUDIO_LOD_TIER_2 = 2,      /* Mid-Field: 2nd-Order Ambisonics (HOA) Binning + Shared Reverb */
	STEAMAUDIO_LOD_TIER_3 = 3       /* Far-Field: Distance Gain + Diffuse Energy Field Accumulator */
};

struct __attribute__((packed, aligned(128))) sof_steamaudio_voice_lod_config {
	uint32_t comp_type;             /* STEAMAUDIO_PARAM_VOICE_LOD */
	uint32_t enabled;               /* 1 = active, 0 = bypass */
	uint32_t max_tier1_voices;      /* Max Tier 1 HRTF voices (default: 32) */
	uint32_t max_tier2_voices;      /* Max Tier 2 HOA voices (default: 64) */
	uint32_t max_tier3_voices;      /* Max Tier 3 Diffuse voices (default: 160) */
	float    tier1_distance_m;      /* Distance threshold for Tier 1 (default: 15.0f) */
	float    tier2_distance_m;      /* Distance threshold for Tier 2 (default: 50.0f) */
	float    hysteresis_m;          /* Distance hysteresis band (default: 1.5f) */
	float    occlusion_demote_db;   /* Occlusion dB loss threshold to demote to Tier 2 (default: 12.0f) */
	uint32_t hoa_order;             /* HOA order for Tier 2 (default: 2 -> 9 channels) */
	float    crossfade_time_ms;     /* Tier transition crossfade time in ms (default: 10.0f) */
	uint32_t reserved[21];          /* Exact 128-byte cacheline / DMA alignment */
};
STATIC_ASSERT(sizeof(struct sof_steamaudio_voice_lod_config) == 128, sof_steamaudio_voice_lod_config_must_be_128_bytes);

struct dsp_voice_lod_source_state {
	uint32_t source_id;
	uint8_t  current_tier;         /* 1, 2, or 3 */
	uint8_t  target_tier;          /* 1, 2, or 3 */
	uint8_t  previous_tier;        /* 1, 2, or 3 */
	uint8_t  active;               /* 1 if active in current frame */
	float    distance_m;           /* Distance to listener in meters */
	float    priority_score;       /* Computed perceptual priority */
	float    transition_gain;      /* 0.0 to 1.0 crossfade progress */
	float    attenuation;          /* Distance + directivity attenuation */
};

struct dsp_voice_lod_stats {
	uint32_t total_active_emitters;
	uint32_t tier1_voice_count;
	uint32_t tier2_voice_count;
	uint32_t tier3_voice_count;
	uint32_t tier1_demotions;      /* Sources demoted from Tier 1 due to budget */
	uint32_t tier2_demotions;      /* Sources demoted from Tier 2 due to budget */
	float    estimated_dsp_load_pct;
	float    peak_voice_priority;
};

struct dsp_voice_lod_state {
	struct sof_steamaudio_voice_lod_config config;
	struct dsp_voice_lod_stats stats;
	struct dsp_voice_lod_source_state sources[STEAMAUDIO_MAX_LOD_SOURCES];
	float hoa_bed[STEAMAUDIO_MAX_HOA_CHANNELS][256];
	float diffuse_bed[2][256];
	uint32_t num_sources;
	uint32_t sample_rate;
	bool enabled;
};

/* Scene-Aware 5.1 / 7.1 Acoustic Upmixer Structures */
#define STEAMAUDIO_UPMIX_DECORR_DELAY_MAX 1024

struct __attribute__((packed, aligned(128))) sof_steamaudio_upmix_config {
	uint32_t comp_type;             /* STEAMAUDIO_PARAM_SCENE_UPMIX */
	uint32_t layout_type;           /* enum steamaudio_speaker_layout (5.1 or 7.1) */
	float    center_spread;          /* 0.0 (discrete center) to 1.0 (phantom center) */
	float    center_threshold_rad;   /* Azimuth threshold for dialogue anchoring (e.g. 0.2618 rad = 15 deg) */
	float    ambient_decorrelation;  /* 0.0 to 1.0 (all-pass decorrelation depth) */
	float    reverb_surround_mix;    /* Wet gain of FDN reverb in surrounds (default 1.0) */
	float    crossover_freq_hz;      /* Bass management cutoff (e.g. 80.0 Hz) */
	uint32_t enable_bvh_reflections; /* 1: map BVH rays to physical speaker walls; 0: bypass */
	uint32_t enable_bass_management; /* 1: Linkwitz-Riley crossover to LFE; 0: full-range */
	uint32_t reserved[23];          /* Exact 128-byte cacheline / DMA alignment */
};
STATIC_ASSERT(sizeof(struct sof_steamaudio_upmix_config) == 128, sof_steamaudio_upmix_config_must_be_128_bytes);

struct dsp_upmix_state {
	struct sof_steamaudio_upmix_config config;
	uint32_t num_speakers;

	/* Schroeder all-pass decorrelator delay buffers (4 delay lines for Ls, Rs, Rls, Rrs) */
	float    ap_buffers[4][STEAMAUDIO_UPMIX_DECORR_DELAY_MAX];
	uint32_t ap_delays[4];
	uint32_t ap_write_idx[4];

	/* 4th-order Linkwitz-Riley crossover states: 2 cascaded 2nd-order Butterworth biquads per channel */
	/* [channel][stage 0..1][coeff 0..4: b0, b1, b2, a1, a2] */
	float    lpf_coeffs[STEAMAUDIO_MAX_SPEAKERS][2][5];
	float    lpf_states[STEAMAUDIO_MAX_SPEAKERS][2][2];
	float    hpf_coeffs[STEAMAUDIO_MAX_SPEAKERS][2][5];
	float    hpf_states[STEAMAUDIO_MAX_SPEAKERS][2][2];

	uint32_t sample_rate;
	bool     enabled;
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
	uint64_t mute_mask;

	/* DSP Subsystem contexts */
	struct steamaudio_direct_state direct;
	struct steamaudio_binaural_state binaural;
	struct steamaudio_reverb_state reverb;
	struct steamaudio_hybrid_reverb_state hybrid;
	struct steamaudio_ambisonics_state ambisonics;
	struct steamaudio_panning_state panning;
	struct steamaudio_virtual_surround_state virtual_surround;
	struct steamaudio_pathing_state pathing;
	struct dsp_scene scene;
	struct dsp_dynamic_geometry dynamic_geom;
	struct dsp_directivity_state directivity;
	struct dsp_atmosphere_state atmosphere;
	struct dsp_diffraction_state diffraction;
	struct dsp_probe_batch_state probe_batch;
	struct dsp_sofa_hrir_state sofa_hrir;
	struct dsp_graph_search_state graph_search;
	struct dsp_reflection_mixer_state reflection_mixer;
	struct dsp_instanced_mesh_state instanced_mesh;
	struct dsp_ray_tracer_state ray_tracer;
	struct dsp_reverb_estimator_state reverb_estimator;
	struct dsp_early_reflections_state early_reflections;
	struct dsp_material_transmission_state material_transmission;
	struct dsp_acoustic_portals_state acoustic_portals;
	struct dsp_volumetric_source_state volumetric_source;
	struct dsp_source_prioritization_state source_prioritization;
	struct dsp_ground_reflection_state ground_reflection;
	struct dsp_limiter_state true_peak_limiter;
	struct dsp_room_modes_state room_modes;
	struct dsp_atmospheric_turbulence_state atmospheric_turbulence;
	struct dsp_surface_scattering_state surface_scattering;
	struct dsp_sound_barrier_state sound_barrier;
	struct dsp_near_field_state near_field;
	struct dsp_nonlinear_wave_state nonlinear_wave;
	struct dsp_battle_bleed_state battle_bleed;
	struct dsp_voice_lod_state voice_lod;
	struct dsp_upmix_state upmix;

	/* Autonomous DSP-Centric Engine & Hardware Cycle Governor State */
	uint32_t dsp_cycle_start;
	uint32_t dsp_cycle_last_frame;
	uint32_t dsp_cycle_moving_avg;
	uint32_t dsp_shedding_level; /* 0 = Full fidelity, 1 = Light LOD, 2 = Heavy LOD */
	uint32_t num_raw_emitters;
	uint64_t frame_count;

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
void steamaudio_dsp_update_direct_eq(struct steamaudio_comp_data *cd);

/* Hybrid Reverb API */
void steamaudio_dsp_hybrid_reverb_init(struct steamaudio_hybrid_reverb_state *hybrid);
void steamaudio_dsp_hybrid_reverb_process(struct steamaudio_comp_data *cd,
					 const float *in,
					 const float in_early[2][256],
					 float *out_l, float *out_r,
					 uint32_t frames);


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
void steamaudio_dsp_ambisonics_rotate(uint32_t order, const float rot[3][3],
				      const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
				      float out_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256], uint32_t frames);
void steamaudio_dsp_ambisonics_decode_binaural(struct steamaudio_ambisonics_state *ambi,
					       const float in_ch[STEAMAUDIO_MAX_HOA_CHANNELS][256],
					       float *out_l, float *out_r, uint32_t frames);

void steamaudio_dsp_pathing_init(struct steamaudio_pathing_state *pathing, uint32_t order, uint32_t sample_rate);
void steamaudio_dsp_pathing_set_params(struct steamaudio_pathing_state *pathing,
				       const float eq[STEAMAUDIO_NUM_EQ_BANDS],
				       const float sh[STEAMAUDIO_MAX_HOA_CHANNELS],
				       uint32_t order, bool binaural,
				       const float rot[3][3],
				       uint32_t sample_rate);
void steamaudio_dsp_pathing_process(struct steamaudio_pathing_state *pathing,
				    struct steamaudio_ambisonics_state *ambi,
				    struct steamaudio_panning_state *panning,
				    const float *in, float *out_l, float *out_r,
				    float out_ch[STEAMAUDIO_MAX_SPEAKERS][256],
				    uint32_t frames);

/**
 * steamaudio_set_config() - Configure Steam Audio DSP parameters via IPC
 * @mod: Pointer to module data.
 * @param_id: Control parameter identifier.
 * @pos: Fragment position in multi-part message.
 * @data_offset_size: Data offset or total payload size.
 * @fragment: Payload byte buffer containing configuration struct.
 * @fragment_size: Size in bytes of fragment buffer.
 * @response: Optional response payload buffer.
 * @response_size: Maximum size in bytes of response buffer.
 *
 * Return: 0 on success, negative errno on failure.
 */
#if CONFIG_IPC_MAJOR_4
int steamaudio_set_config(struct processing_module *mod,
			  uint32_t param_id,
			  enum module_cfg_fragment_position pos,
			  uint32_t data_offset_size,
			  const uint8_t *fragment,
			  size_t fragment_size,
			  uint8_t *response,
			  size_t response_size);

/**
 * steamaudio_get_config() - Retrieve Steam Audio DSP parameter state via IPC
 * @mod: Pointer to module data.
 * @config_id: Requested configuration parameter identifier.
 * @data_offset_size: Pointer to store returned response size in bytes.
 * @fragment: Destination buffer for configuration data.
 * @fragment_size: Capacity of destination buffer in bytes.
 *
 * Return: 0 on success, negative errno on failure.
 */
int steamaudio_get_config(struct processing_module *mod,
			  uint32_t config_id, uint32_t *data_offset_size,
			  uint8_t *fragment, size_t fragment_size);
#else
static inline int steamaudio_set_config(struct processing_module *mod,
					uint32_t param_id,
					enum module_cfg_fragment_position pos,
					uint32_t data_offset_size,
					const uint8_t *fragment,
					size_t fragment_size,
					uint8_t *response,
					size_t response_size)
{
	return -ENOTSUP;
}

static inline int steamaudio_get_config(struct processing_module *mod,
					uint32_t config_id, uint32_t *data_offset_size,
					uint8_t *fragment, size_t fragment_size)
{
	return -ENOTSUP;
}
#endif

void sys_comp_module_steamaudio_interface_init(void);

/* BVH ray tracer API */
void steamaudio_dsp_scene_init_box_room(struct dsp_scene *scene, float width, float length, float height);
bool steamaudio_dsp_trace_closest_hit(const struct dsp_scene *scene, const struct dsp_ray *ray, struct dsp_hit *hit);
float steamaudio_dsp_test_occlusion(const struct dsp_scene *scene, struct dsp_vec3 source, struct dsp_vec3 listener);

/* Path Simulator & Probe Math API */
void steamaudio_dsp_path_sim_eval(const float source[3], const float listener[3],
				  const float (*virtual_sources)[3], const float *path_weights,
				  const float *deviations, uint32_t num_paths, uint32_t order,
				  float eq_out[STEAMAUDIO_NUM_EQ_BANDS],
				  float sh_out[STEAMAUDIO_MAX_HOA_CHANNELS],
				  float avg_dir_out[3], float *dist_ratio_out, float *tot_dev_out);
void steamaudio_dsp_probe_weights(const float point[3], const float (*probe_centers)[3],
				  uint32_t num_probes, float *weights_out);

/* Energy Field & Acoustic Radiance Ray Marching API */
void steamaudio_dsp_energy_field_simulate(const float source[3], const float listener[3],
					  uint32_t num_rays, uint32_t num_bounces,
					  float duration, uint32_t order,
					  float irradiance_min_distance,
					  const float room_dimensions[3],
					  uint32_t num_channels, uint32_t num_bands, uint32_t num_bins,
					  float *out_data);
void steamaudio_dsp_energy_field_scale(const float *in, float scalar, float *out, uint32_t total_size);
void steamaudio_dsp_energy_field_add(const float *in1, const float *in2, float *out, uint32_t total_size);
void steamaudio_dsp_energy_field_scale_accum(const float *in, float scalar, float *out, uint32_t total_size);

/* Compressed Metadata Stream & PTS Sync API */
int steamaudio_dsp_unpack_metadata_packet(const uint8_t *data, uint32_t size,
					 struct steamaudio_compressed_metadata_packet *out_header,
					 struct steamaudio_voice_meta *out_voices, uint32_t max_voices);
void steamaudio_dsp_sync_metadata_pts(struct steamaudio_comp_data *cd, uint64_t audio_pts);

/* Acoustic Impulse Response Reconstructor & IR Synthesis API */
void steamaudio_dsp_reconstruct_ir(const float *energy_field,
				  uint32_t num_channels,
				  uint32_t num_bands,
				  uint32_t num_bins,
				  uint32_t sampling_rate,
				  uint32_t reconstruction_type,
				  const float *air_absorption,
				  const float *distance_correction,
				  float *out_ir,
				  uint32_t num_ir_samples);

/* Dynamic Fractional Delay Line & Doppler Pitch Modulation API */
struct __attribute__((packed)) sof_steamaudio_delay_config {
	uint32_t interpolation_type; /* 0 = linear, 1 = cubic hermite, 2 = sinc */
	float delay_samples;         /* current fractional delay in samples */
	float target_delay_samples;  /* target fractional delay in samples */
	float doppler_ratio;         /* frequency scaling factor */
	uint32_t max_delay_samples;  /* capacity of ring buffer */
	uint32_t channel;            /* channel index */
};

void steamaudio_dsp_process_delay(const struct sof_steamaudio_delay_config *config,
				  float *ring_buffer,
				  uint32_t *write_cursor,
				  const float *in,
				  float *out,
				  uint32_t num_samples);

/* Bidirectional Voice Chat (VoIP) Preprocessing & Positional 3D Spatialization API */
struct __attribute__((packed)) sof_steamaudio_voip_config {
	uint32_t enabled;             /* 1 = active, 0 = bypass/disabled */
	float gate_threshold_db;      /* noise gate threshold in dBFS (e.g. -45.0f) */
	float attack_time_ms;         /* envelope attack time in ms (e.g. 5.0f) */
	float release_time_ms;        /* envelope release time in ms (e.g. 100.0f) */
	float agc_target_db;          /* AGC target level in dBFS (e.g. -18.0f) */
	float agc_max_gain_db;        /* AGC max boost ceiling in dB (e.g. 12.0f) */
	float source_position[3];     /* 3D world position of speaking player */
	float listener_position[3];   /* 3D world position of listener */
	float listener_ahead[3];      /* listener orientation ahead vector */
	float listener_up[3];         /* listener orientation up vector */
	float directivity_weight;     /* source directivity factor */
	uint32_t spatial_mode;        /* 0 = mono bypass, 1 = stereo binaural, 2 = 3D panned */
	float sample_rate;            /* e.g. 48000.0f */
};

struct sof_steamaudio_voip_state {
	float hp_x1, hp_x2, hp_y1, hp_y2; /* Highpass 80Hz biquad filter state */
	float env_level;                  /* Smoothed envelope follower */
	float agc_gain;                   /* Current AGC gain multiplier */
	uint32_t is_speaking;             /* 1 if voice activity detected, 0 otherwise */
};

void steamaudio_dsp_process_voip(const struct sof_steamaudio_voip_config *config,
				 struct sof_steamaudio_voip_state *state,
				 const float *in,
				 float *out_left,
				 float *out_right,
				 uint32_t num_samples);

/* Multi-Listener Audio Splitting & Simultaneous Headphone + Speaker Virtualization API */
#define STEAMAUDIO_MAX_LISTENERS 4

enum steamaudio_endpoint_type {
	STEAMAUDIO_ENDPOINT_HEADPHONES = 0,
	STEAMAUDIO_ENDPOINT_SPEAKERS = 1,
};

struct __attribute__((packed)) sof_steamaudio_listener_endpoint {
	uint32_t endpoint_type;       /* 0 = Headphone (Binaural), 1 = Speakers (Surround) */
	uint32_t speaker_layout;      /* 0 = Stereo, 1 = Quad, 2 = 5.1, 3 = 7.1 */
	uint32_t num_channels;        /* 2, 4, 6, 8 */
	float position[3];            /* World position (x, y, z) */
	float ahead[3];               /* Forward orientation vector */
	float up[3];                  /* Up orientation vector */
	float gain;                   /* Endpoint volume multiplier */
	uint32_t muted;               /* 1 = muted, 0 = active */
};

struct __attribute__((packed)) sof_steamaudio_multilistener_config {
	uint32_t num_listeners;       /* Number of active listeners (1..4) */
	uint32_t distance_attenuation;/* 1 = enable inverse distance attenuation, 0 = disable */
	float source_position[3];     /* World position of audio source (x, y, z) */
	struct sof_steamaudio_listener_endpoint listeners[STEAMAUDIO_MAX_LISTENERS];
};

void steamaudio_dsp_process_multilistener(const struct sof_steamaudio_multilistener_config *config,
					  const float *in,
					  float out_channels[STEAMAUDIO_MAX_LISTENERS][STEAMAUDIO_MAX_SPEAKERS][256],
					  uint32_t frames);

/* Dynamic Occlusion Geometry Streaming API */
enum steamaudio_geom_op {
	STEAMAUDIO_GEOM_OP_CLEAR = 0,
	STEAMAUDIO_GEOM_OP_APPEND = 1,
	STEAMAUDIO_GEOM_OP_UPDATE_MESH = 2,
	STEAMAUDIO_GEOM_OP_REMOVE_MESH = 3,
};

struct __attribute__((packed)) sof_steamaudio_triangle {
	float v0[3];
	float v1[3];
	float v2[3];
	float normal[3];
	float transmission[3];
	uint32_t mesh_id;
};

struct __attribute__((packed)) sof_steamaudio_geom_stream_header {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_GEOMETRY_STREAM */
	uint32_t op;              /* enum steamaudio_geom_op */
	uint32_t mesh_id;
	uint32_t num_triangles;   /* 0..32 */
	uint32_t ring_write_seq;
};

struct __attribute__((packed)) sof_steamaudio_geom_stream_payload {
	struct sof_steamaudio_geom_stream_header header;
	struct sof_steamaudio_triangle triangles[32];
};

void steamaudio_dsp_dynamic_geom_init(struct dsp_dynamic_geometry *dg);
int steamaudio_dsp_dynamic_geom_stream(struct dsp_dynamic_geometry *dg,
				       const struct sof_steamaudio_geom_stream_payload *stream);
float steamaudio_dsp_test_dynamic_occlusion(const struct dsp_dynamic_geometry *dg,
					    struct dsp_vec3 source,
					    struct dsp_vec3 listener,
					    float out_transmission[3]);

/* Directional Sound Radiation & Source Directivity Patterns API */
struct __attribute__((packed)) sof_steamaudio_directivity_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_DIRECTIVITY */
	float source_pos[3];
	float source_ahead[3];
	float source_up[3];
	float listener_pos[3];
	float dipole_weight;
	float dipole_power;
	uint32_t freq_dependent;
	float band_weights[3];
	float band_powers[3];
	float directivity_gain;
	float directivity_eq[STEAMAUDIO_NUM_EQ_BANDS];
};

void steamaudio_dsp_directivity_init(struct dsp_directivity_state *dir);
float steamaudio_dsp_calculate_directivity(const float source_pos[3],
					   const float source_ahead[3],
					   const float listener_pos[3],
					   float dipole_weight,
					   float dipole_power);
void steamaudio_dsp_calculate_directivity_3band(const float source_pos[3],
						const float source_ahead[3],
						const float listener_pos[3],
						float dipole_weight,
						float dipole_power,
						float out_gains[STEAMAUDIO_NUM_EQ_BANDS]);

/* Dynamic Atmospheric Physics & ISO 9613-1 Air Absorption API */
struct __attribute__((packed)) sof_steamaudio_atmosphere_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_ATMOSPHERE */
	float temperature_c;      /* -50.0 to +60.0 C */
	float relative_humidity;  /* 0.0 to 1.0 (0% to 100%) */
	float pressure_kpa;       /* atmospheric pressure, default 101.325 kPa */
	float speed_of_sound;     /* calculated speed of sound m/s */
	float absorption_coefficients[STEAMAUDIO_NUM_EQ_BANDS]; /* calculated Np/m */
	uint32_t flags;           /* Bit 0: enabled, Bit 1: override defaults */
};

void steamaudio_dsp_atmosphere_init(struct dsp_atmosphere_state *atm);
void steamaudio_dsp_calculate_atmosphere(float temp_c, float rel_hum, float pressure_kpa,
					 float *speed_of_sound, float *abs_coeffs_3band);
void steamaudio_dsp_calculate_air_absorption(float distance, const float *abs_coeffs,
					     float *out_gains);

/* Acoustic Edge Diffraction & Obstacle Pathing (BTM / UTD) API */
struct __attribute__((packed)) sof_steamaudio_diffraction_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_DIFFRACTION */
	float wedge_angle_rad;    /* Wedge interior angle [0, pi), 0 = knife-edge */
	float deviation_angle_rad;/* Total deviation angle behind edge [0, pi] */
	float r_source;           /* Distance from source to edge apex (m) */
	float r_receiver;         /* Distance from receiver to edge apex (m) */
	float speed_of_sound;     /* Dynamic speed of sound (m/s) */
	float transmission[STEAMAUDIO_NUM_EQ_BANDS]; /* Material transmission loss [0, 1] */
	float diffraction_coeffs[STEAMAUDIO_NUM_EQ_BANDS]; /* 3-band diffraction gains */
	float combined_gains[STEAMAUDIO_NUM_EQ_BANDS];     /* Combined diffraction + transmission */
	uint32_t flags;           /* Bit 0: enabled, Bit 1: override defaults */
};

void steamaudio_dsp_diffraction_init(struct dsp_diffraction_state *diff);
void steamaudio_dsp_calculate_edge_diffraction(float wedge_angle_rad, float deviation_angle_rad,
					       float r_source, float r_receiver,
					       float speed_of_sound, float *out_diff_coeffs);
void steamaudio_dsp_calculate_obstacle_pathing(const float *diff_coeffs, const float *transmission,
					       float *out_combined_gains);

/* Clustered Acoustic Probe Batching & Spatial Interpolation API */
struct __attribute__((packed)) sof_steamaudio_probe_batch_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_PROBEBATCH */
	uint32_t num_probes;      /* Number of valid probes [0, STEAMAUDIO_MAX_PROBES] */
	struct dsp_probe probes[STEAMAUDIO_MAX_PROBES];
	uint32_t num_queries;     /* Number of query sources [0, STEAMAUDIO_MAX_PROBE_QUERIES] */
	float query_positions[STEAMAUDIO_MAX_PROBE_QUERIES][3];
	int32_t neighbor_indices[STEAMAUDIO_MAX_PROBE_QUERIES][STEAMAUDIO_MAX_NEIGHBORS];
	float neighbor_weights[STEAMAUDIO_MAX_PROBE_QUERIES][STEAMAUDIO_MAX_NEIGHBORS];
	float interpolated_eq[STEAMAUDIO_MAX_PROBE_QUERIES][STEAMAUDIO_NUM_EQ_BANDS];
	uint32_t flags;           /* Bit 0: enabled, Bit 1: override defaults */
};

void steamaudio_dsp_probe_batch_init(struct dsp_probe_batch_state *pb);
void steamaudio_dsp_probe_batch_lookup(const struct dsp_probe *probes, uint32_t num_probes,
				       const float query_pos[3], int32_t *out_indices,
				       uint32_t max_neighbors);
void steamaudio_dsp_probe_batch_interpolate(const struct dsp_probe *probes,
					    const int32_t *neighbor_indices, uint32_t num_neighbors,
					    const float query_pos[3], float *out_weights,
					    float *out_interpolated_eq);
void steamaudio_dsp_probe_batch_process(struct dsp_probe_batch_state *pb);

/* Measured SOFA / HRIR Direct DSP Convolution API */
struct __attribute__((packed)) sof_steamaudio_sofa_hrir_config {
	uint32_t comp_type;                          /* STEAMAUDIO_PARAM_SOFA_HRIR */
	uint32_t num_taps;                           /* Number of taps per ear [1, STEAMAUDIO_MAX_HRIR_TAPS] */
	float hrir_left[STEAMAUDIO_MAX_HRIR_TAPS];   /* Left ear measured HRIR FIR filter taps */
	float hrir_right[STEAMAUDIO_MAX_HRIR_TAPS];  /* Right ear measured HRIR FIR filter taps */
	float direction[3];                          /* Source incident vector [x, y, z] */
	float spatial_blend;                         /* Direct vs binaural blend [0.0, 1.0] */
	float volume;                                /* Volume trim factor (linear) */
	uint32_t flags;                              /* Bit 0: enabled, Bit 1: reset history / instant swap */
};

void steamaudio_dsp_sofa_hrir_init(struct dsp_sofa_hrir_state *hrir);
void steamaudio_dsp_sofa_hrir_set_impulse_response(struct dsp_sofa_hrir_state *hrir,
						   const float *left_taps,
						   const float *right_taps,
						   uint32_t num_taps,
						   bool reset_history);
void steamaudio_dsp_sofa_hrir_process(struct dsp_sofa_hrir_state *hrir,
				      const float *in,
				      float *out_l, float *out_r,
				      uint32_t frames,
				      bool muted);

/* Probe Network Graph Dijkstra Shortest Path Search API */
struct __attribute__((packed)) sof_steamaudio_graph_search_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_GRAPH_SEARCH */
	uint16_t num_nodes;
	uint16_t start_node;
	uint16_t target_node;
	float max_range;
	struct {
		uint16_t num_edges;
		struct {
			uint16_t node;
			float cost;
		} edges[STEAMAUDIO_MAX_GRAPH_EDGES_PER_NODE];
		float pos[3];
	} nodes[STEAMAUDIO_MAX_GRAPH_NODES];
	uint16_t num_path_nodes;
	uint16_t path_nodes[STEAMAUDIO_MAX_PATH_NODES];
	float total_cost;
	uint8_t path_found;
	uint8_t enabled;
	uint16_t flags;
};

void steamaudio_dsp_graph_search_init(struct dsp_graph_search_state *gs);
void steamaudio_dsp_graph_search_set_graph(struct dsp_graph_search_state *gs,
					   uint16_t num_nodes,
					   const struct dsp_graph_node *nodes);
void steamaudio_dsp_graph_search_find_path(struct dsp_graph_search_state *gs,
					   uint16_t start,
					   uint16_t target,
					   float max_range,
					   bool muted);

/* Multi-Source Reflection Mixer Coalescence API */
struct __attribute__((packed)) sof_steamaudio_reflection_mixer_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_REFLECTION_MIXER */
	uint32_t num_sources;     /* Active sources coalesced [0, STEAMAUDIO_MAX_MIXER_SOURCES] */
	uint32_t num_channels;    /* Channels [1, STEAMAUDIO_MAX_MIXER_CHANNELS] */
	uint32_t frames;          /* Frame size [1, STEAMAUDIO_MAX_MIXER_FRAMES] */
	float source_gains[STEAMAUDIO_MAX_MIXER_SOURCES];
	float mixed_output[STEAMAUDIO_MAX_MIXER_CHANNELS][STEAMAUDIO_MAX_MIXER_FRAMES];
	uint32_t flags;           /* Bit 0: enabled, Bit 1: reset accumulator */
};

void steamaudio_dsp_reflection_mixer_init(struct dsp_reflection_mixer_state *rm);
void steamaudio_dsp_reflection_mixer_reset(struct dsp_reflection_mixer_state *rm);
void steamaudio_dsp_reflection_mixer_accumulate(struct dsp_reflection_mixer_state *rm,
					       uint32_t source_idx,
					       const float *in,
					       uint32_t channel,
					       uint32_t frames,
					       float gain);
void steamaudio_dsp_reflection_mixer_process(struct dsp_reflection_mixer_state *rm,
					    struct sof_steamaudio_reflection_mixer_config *cfg,
					    bool muted);

/* Hierarchical Instanced Mesh Scene Graph API */
enum steamaudio_instanced_mesh_op {
	STEAMAUDIO_INSTANCED_MESH_OP_SET_PROTOTYPE = 0,
	STEAMAUDIO_INSTANCED_MESH_OP_SET_INSTANCE = 1,
	STEAMAUDIO_INSTANCED_MESH_OP_UPDATE_TRANSFORM = 2,
	STEAMAUDIO_INSTANCED_MESH_OP_REMOVE_INSTANCE = 3,
	STEAMAUDIO_INSTANCED_MESH_OP_CLEAR = 4,
	STEAMAUDIO_INSTANCED_MESH_OP_QUERY_OCCLUSION = 5,
};

struct __attribute__((packed)) sof_steamaudio_instanced_mesh_config {
	uint32_t comp_type;        /* STEAMAUDIO_PARAM_INSTANCED_MESH */
	uint32_t op;               /* enum steamaudio_instanced_mesh_op */
	uint32_t instance_id;
	uint32_t prototype_id;
	float transform[16];
	uint32_t num_triangles;
	struct sof_steamaudio_triangle triangles[32];
	float ray_origin[3];
	float ray_direction[3];
	float min_dist;
	float max_dist;
	float out_occlusion;
	float out_transmission[3];
	uint32_t out_has_hit;
	uint32_t flags;
};

void steamaudio_dsp_instanced_mesh_init(struct dsp_instanced_mesh_state *ims);
int steamaudio_dsp_instanced_mesh_set_prototype(struct dsp_instanced_mesh_state *ims,
						uint32_t prototype_id,
						const struct sof_steamaudio_triangle *triangles,
						uint32_t num_triangles);
int steamaudio_dsp_instanced_mesh_set_instance(struct dsp_instanced_mesh_state *ims,
					       uint32_t instance_id,
					       uint32_t prototype_id,
					       const float transform[16]);
int steamaudio_dsp_instanced_mesh_update_transform(struct dsp_instanced_mesh_state *ims,
						   uint32_t instance_id,
						   const float transform[16]);
int steamaudio_dsp_instanced_mesh_remove_instance(struct dsp_instanced_mesh_state *ims,
						  uint32_t instance_id);
void steamaudio_dsp_instanced_mesh_clear(struct dsp_instanced_mesh_state *ims);
float steamaudio_dsp_instanced_mesh_test_occlusion(const struct dsp_instanced_mesh_state *ims,
						   struct dsp_vec3 source,
						   struct dsp_vec3 listener,
						   float out_transmission[3],
						   bool muted);
bool steamaudio_dsp_instanced_mesh_trace_ray(const struct dsp_instanced_mesh_state *ims,
					     const struct dsp_ray *ray,
					     struct dsp_hit *hit,
					     bool muted);

/* Multi-Bounce Acoustic Ray Tracer API */
struct __attribute__((packed)) sof_steamaudio_ray_tracer_config {
	uint32_t comp_type;        /* STEAMAUDIO_PARAM_RAY_TRACER */
	uint32_t num_rays;         /* [1, STEAMAUDIO_RAY_TRACER_MAX_RAYS] */
	uint32_t max_bounces;      /* [1, STEAMAUDIO_RAY_TRACER_MAX_BOUNCES] */
	float speed_of_sound;      /* m/s, default 343.0 */
	float irradiance_min_dist; /* meters, default 1.0 */
	float listener_radius;     /* meters, default 1.0 */
	float material_absorption[STEAMAUDIO_NUM_EQ_BANDS];
	float scattering;
	float source_pos[3];
	float listener_pos[3];
	float ray_directions[STEAMAUDIO_RAY_TRACER_MAX_RAYS][3];
	uint32_t num_results;
	struct {
		uint32_t num_bounces;
		float total_distance;
		float delay_ms;
		float energy[STEAMAUDIO_NUM_EQ_BANDS];
		float arrival_dir[3];
		uint32_t reached_listener;
	} results[STEAMAUDIO_RAY_TRACER_MAX_RAYS];
	uint32_t flags;            /* Bit 0: enabled, Bit 1: specular only */
};

void steamaudio_dsp_ray_tracer_init(struct dsp_ray_tracer_state *rts);
void steamaudio_dsp_ray_tracer_set_config(struct dsp_ray_tracer_state *rts,
					 const struct sof_steamaudio_ray_tracer_config *cfg);
void steamaudio_dsp_ray_tracer_trace_path(const struct steamaudio_comp_data *cd,
					 const struct dsp_ray_tracer_state *rts,
					 struct dsp_vec3 origin,
					 struct dsp_vec3 dir,
					 struct dsp_vec3 listener,
					 uint32_t ray_idx,
					 uint32_t max_bounces,
					 struct dsp_acoustic_ray_path *out_path);
void steamaudio_dsp_ray_tracer_simulate_batch(const struct steamaudio_comp_data *cd,
					      struct dsp_ray_tracer_state *rts,
					      struct sof_steamaudio_ray_tracer_config *cfg,
					      bool muted);

struct sof_steamaudio_reverb_estimator_config {
	uint32_t num_bins;
	float bin_duration_s;
	float early_cutoff_s;
	float scattering;
	uint32_t num_input_paths;
	struct dsp_acoustic_ray_path input_paths[STEAMAUDIO_RAY_TRACER_MAX_RAYS];
	struct dsp_reverb_estimation_results results;
	uint32_t flags;
};

void steamaudio_dsp_reverb_estimator_init(struct dsp_reverb_estimator_state *res);
void steamaudio_dsp_reverb_estimator_set_config(struct dsp_reverb_estimator_state *res,
						const struct sof_steamaudio_reverb_estimator_config *cfg);
void steamaudio_dsp_reverb_estimator_accumulate_rays(struct dsp_reverb_estimator_state *res,
						     const struct dsp_acoustic_ray_path *paths,
						     uint32_t num_paths);
void steamaudio_dsp_reverb_estimator_compute_edc_rt60(struct dsp_reverb_estimator_state *res,
						      bool muted);
void steamaudio_dsp_diffuse_reflect(struct dsp_vec3 in_dir,
				    struct dsp_vec3 normal,
				    float scattering,
				    float seed_u,
				    float seed_v,
				    struct dsp_vec3 *out_dir);

/* Early Reflections Synthesizer & Tapped-Delay Filter Bank API */
struct __attribute__((packed)) sof_steamaudio_early_reflections_config {
	uint32_t comp_type; /* STEAMAUDIO_PARAM_EARLY_REFLECTIONS */
	uint32_t num_taps;
	float sample_rate;
	float speed_of_sound;
	struct dsp_early_reflection_tap taps[STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS];
	uint32_t num_channels;
	uint32_t frames;
	uint32_t flags;
};

void steamaudio_dsp_early_reflections_init(struct dsp_early_reflections_state *ers);
void steamaudio_dsp_early_reflections_reset(struct dsp_early_reflections_state *ers);
void steamaudio_dsp_early_reflections_set_config(struct dsp_early_reflections_state *ers,
						const struct sof_steamaudio_early_reflections_config *cfg);
void steamaudio_dsp_early_reflections_set_taps_from_paths(struct dsp_early_reflections_state *ers,
							  const struct dsp_acoustic_ray_path *paths,
							  uint32_t num_paths,
							  float speed_of_sound);
void steamaudio_dsp_early_reflections_process(struct dsp_early_reflections_state *ers,
					     const float *in,
					     float out[STEAMAUDIO_MAX_MIXER_CHANNELS][256],
					     uint32_t frames,
					     bool muted);

/* Acoustic Material Transmission & Sound Wall Partitioning API */
struct __attribute__((packed)) sof_steamaudio_material_transmission_config {
	uint32_t comp_type; /* STEAMAUDIO_PARAM_MATERIAL_TRANSMISSION */
	uint32_t num_layers;
	struct {
		float surface_density;
		float thickness;
		float transmission[STEAMAUDIO_NUM_EQ_BANDS];
		uint32_t preset;
	} layers[STEAMAUDIO_MAX_MATERIAL_LAYERS];
	float composite_transmission[STEAMAUDIO_NUM_EQ_BANDS];
	float wall_normal[3];
	float incident_angle_rad;
	uint32_t double_sided_compensation; /* 1 = apply sqrt rule for 2 faces, 0 = direct product */
	uint32_t sample_rate;
	uint32_t flags; /* Bit 0: enabled, Bit 1: calculate from mass law, Bit 2: filter audio */
};

void steamaudio_dsp_material_transmission_init(struct dsp_material_transmission_state *mts);
void steamaudio_dsp_material_transmission_set_config(struct dsp_material_transmission_state *mts,
						    const struct sof_steamaudio_material_transmission_config *cfg);
void steamaudio_dsp_material_calculate_mass_law(float surface_density,
					       float out_transmission[STEAMAUDIO_NUM_EQ_BANDS]);
void steamaudio_dsp_material_calculate_composite(const struct dsp_material_properties *layers,
						uint32_t num_layers,
						bool double_sided,
						float out_composite[STEAMAUDIO_NUM_EQ_BANDS]);
void steamaudio_dsp_material_transmission_process(struct dsp_material_transmission_state *mts,
						 const float *in,
						 float *out,
						 uint32_t frames,
						 bool muted);

/* Acoustic Portals & Coupled Room-to-Room Energy Transfer API */
struct __attribute__((packed)) sof_steamaudio_acoustic_portals_config {
	uint32_t comp_type; /* STEAMAUDIO_PARAM_ACOUSTIC_PORTALS */
	uint32_t num_portals;
	struct {
		float center[3];
		float normal[3];
		float dimensions[2]; /* width, height (m) */
		float area;          /* m^2 */
		float openness;      /* [0.0, 1.0] */
		int32_t room_id_front;
		int32_t room_id_back;
		uint32_t enabled;
	} portals[STEAMAUDIO_MAX_PORTALS];
	float source_pos[3];
	float listener_pos[3];
	int32_t source_room_id;
	int32_t listener_room_id;
	uint32_t sample_rate;
	uint32_t flags; /* Bit 0: enabled, Bit 1: filter audio */
};

void steamaudio_dsp_acoustic_portals_init(struct dsp_acoustic_portals_state *aps);
void steamaudio_dsp_acoustic_portals_set_config(struct dsp_acoustic_portals_state *aps,
					       const struct sof_steamaudio_acoustic_portals_config *cfg);
void steamaudio_dsp_acoustic_portals_evaluate_coupling(const struct dsp_acoustic_portals_state *aps,
						     struct dsp_vec3 source,
						     struct dsp_vec3 listener,
						     int32_t src_room,
						     int32_t lis_room,
						     struct dsp_portal_coupling_result *out_res);
void steamaudio_dsp_acoustic_portals_process(struct dsp_acoustic_portals_state *aps,
					    const float *in,
					    float *out,
					    uint32_t frames,
					    bool muted);

/* Volumetric Sound Sources & Spatial Soundfield Spread API */
struct __attribute__((packed)) sof_steamaudio_volumetric_source_config {
	uint32_t comp_type; /* STEAMAUDIO_PARAM_VOLUMETRIC_SOURCE */
	uint32_t num_sources;
	struct {
		uint32_t shape_type;
		float center[3];
		float params[4];
		float energy_distribution;
		uint32_t enabled;
	} sources[STEAMAUDIO_MAX_VOLUMETRIC_SOURCES];
	float listener_pos[3];
	float listener_ahead[3];
	float listener_up[3];
	uint32_t speaker_layout;
	uint32_t sample_rate;
	uint32_t flags; /* Bit 0: enabled, Bit 1: filter audio */
};

void steamaudio_dsp_volumetric_source_init(struct dsp_volumetric_source_state *vss);
void steamaudio_dsp_volumetric_source_set_config(struct dsp_volumetric_source_state *vss,
						const struct sof_steamaudio_volumetric_source_config *cfg);
void steamaudio_dsp_volumetric_source_evaluate(struct dsp_volumetric_source_state *vss,
					       uint32_t source_idx,
					       struct dsp_volumetric_spread_result *out_res);
void steamaudio_dsp_volumetric_source_process(struct dsp_volumetric_source_state *vss,
					      const float *in,
					      float *out,
					      uint32_t frames,
					      uint32_t num_channels,
					      bool muted);

/* Voice Management & Dynamic Source Prioritization API */
struct __attribute__((packed)) sof_steamaudio_source_prioritization_config {
	uint32_t comp_type; /* STEAMAUDIO_PARAM_SOURCE_PRIORITIZATION */
	uint32_t num_sources;
	uint32_t max_voices;
	struct {
		uint32_t source_id;
		float position[3];
		float base_priority;
		float volume;
		float direct_fraction;
		uint32_t flags;
		uint32_t enabled;
	} sources[STEAMAUDIO_MAX_PRIORITY_SOURCES];
	float listener_pos[3];
	float listener_ahead[3];
	float min_audible_threshold;
	float distance_reference;
	float distance_max;
	float fov_attenuation_bias;
	float hysteresis_margin;
	uint32_t sample_rate;
	uint32_t flags; /* Bit 0: enabled, Bit 1: filter audio */
};

void steamaudio_dsp_source_prioritization_init(struct dsp_source_prioritization_state *sps);
void steamaudio_dsp_source_prioritization_set_config(struct dsp_source_prioritization_state *sps,
						    const struct sof_steamaudio_source_prioritization_config *cfg);
void steamaudio_dsp_source_prioritization_evaluate(struct dsp_source_prioritization_state *sps);
void steamaudio_dsp_source_prioritization_process(struct dsp_source_prioritization_state *sps,
						 const float *in,
						 float *out,
						 uint32_t frames,
						 uint32_t num_channels,
						 bool muted);

/* Ground Reflection & Acoustic Multipath Interference API */
struct __attribute__((packed)) sof_steamaudio_ground_reflection_config {
	uint32_t comp_type; /* STEAMAUDIO_PARAM_GROUND_REFLECTION */
	float ground_plane_pos[3];
	float ground_plane_normal[3];
	uint32_t material_preset;
	float reflection_coeffs[STEAMAUDIO_NUM_EQ_BANDS];
	float sound_speed;
	float source_pos[3];
	float listener_pos[3];
	uint32_t sample_rate;
	uint32_t flags; /* Bit 0: enabled, Bit 1: filter audio */
};

void steamaudio_dsp_ground_reflection_init(struct dsp_ground_reflection_state *grs);
void steamaudio_dsp_ground_reflection_set_config(struct dsp_ground_reflection_state *grs,
						const struct sof_steamaudio_ground_reflection_config *cfg);
void steamaudio_dsp_ground_reflection_evaluate(struct dsp_ground_reflection_state *grs,
					       struct dsp_ground_reflection_result *out_res);
void steamaudio_dsp_ground_reflection_process(struct dsp_ground_reflection_state *grs,
					      const float *in,
					      float *out,
					      uint32_t frames,
					      uint32_t num_channels,
					      bool muted);

/* Spatial Audio True-Peak Limiter & Dynamic Range Control API */
struct __attribute__((packed)) sof_steamaudio_limiter_config {
	uint32_t comp_type;       /* STEAMAUDIO_PARAM_TRUE_PEAK_LIMITER */
	float threshold_db;
	float ceiling_db;
	float knee_width_db;
	float ratio;
	float attack_time_ms;
	float release_time_ms;
	float makeup_gain_db;
	uint32_t sample_rate;
	uint32_t flags;           /* Bit 0: enabled, Bit 1: soft knee, Bit 2: true peak */
};

void steamaudio_dsp_true_peak_limiter_init(struct dsp_limiter_state *dls);
void steamaudio_dsp_true_peak_limiter_set_config(struct dsp_limiter_state *dls,
						const struct sof_steamaudio_limiter_config *cfg);
void steamaudio_dsp_true_peak_limiter_process(struct dsp_limiter_state *dls,
					      const float *in,
					      float *out,
					      uint32_t frames,
					      uint32_t num_channels,
					      bool muted);

/* Room Modal Resonances & Standing Wave Eigenmodes API */
void steamaudio_dsp_room_modes_init(struct dsp_room_modes_state *rms);
void steamaudio_dsp_room_modes_set_config(struct dsp_room_modes_state *rms,
					 const struct sof_steamaudio_room_modes_config *cfg);
void steamaudio_dsp_room_modes_process(struct dsp_room_modes_state *rms,
				       const float *in,
				       float *out,
				       uint32_t frames,
				       uint32_t num_channels,
				       bool muted);

/* Atmospheric Turbulence & Wind Advection API */
void steamaudio_dsp_atmospheric_turbulence_init(struct dsp_atmospheric_turbulence_state *ats);
void steamaudio_dsp_atmospheric_turbulence_set_config(struct dsp_atmospheric_turbulence_state *ats,
						      const struct sof_steamaudio_atmospheric_turbulence_config *cfg);
void steamaudio_dsp_atmospheric_turbulence_process(struct dsp_atmospheric_turbulence_state *ats,
						   const float *in,
						   float *out,
						   uint32_t frames,
						   uint32_t num_channels,
						   bool muted);

/* Surface Acoustic Scattering & Rough Boundary Diffuse Dispersion API */
void steamaudio_dsp_surface_scattering_init(struct dsp_surface_scattering_state *sss);
void steamaudio_dsp_surface_scattering_set_config(struct dsp_surface_scattering_state *sss,
						  const struct sof_steamaudio_surface_scattering_config *cfg);
void steamaudio_dsp_surface_scattering_process(struct dsp_surface_scattering_state *sss,
					       const float *in,
					       float *out,
					       uint32_t frames,
					       uint32_t num_channels,
					       bool muted);

/* Sound Barrier Edge Diffraction & Maekawa Shadowing API */
void steamaudio_dsp_sound_barrier_init(struct dsp_sound_barrier_state *sbs);
void steamaudio_dsp_sound_barrier_set_config(struct dsp_sound_barrier_state *sbs,
					     const struct sof_steamaudio_sound_barrier_config *cfg);
void steamaudio_dsp_sound_barrier_process(struct dsp_sound_barrier_state *sbs,
					  const float *in,
					  float *out,
					  uint32_t frames,
					  uint32_t num_channels,
					  bool muted);

/* Near-Field HRIR Parallax & Proximity Effect Bass Boost API */
void steamaudio_dsp_near_field_init(struct dsp_near_field_state *nfs);
void steamaudio_dsp_near_field_set_config(struct dsp_near_field_state *nfs,
					  const struct sof_steamaudio_near_field_config *cfg);
void steamaudio_dsp_near_field_process(struct dsp_near_field_state *nfs,
				       const float *in,
				       float *out,
				       uint32_t frames,
				       uint32_t num_channels,
				       bool muted);

/* Nonlinear Acoustic Propagation & Shock Wave Crest Distortion API */
void steamaudio_dsp_nonlinear_wave_init(struct dsp_nonlinear_wave_state *nws);
void steamaudio_dsp_nonlinear_wave_set_config(struct dsp_nonlinear_wave_state *nws,
					      const struct sof_steamaudio_nonlinear_wave_config *cfg);
void steamaudio_dsp_nonlinear_wave_process(struct dsp_nonlinear_wave_state *nws,
					   const float *in,
					   float *out,
					   uint32_t frames,
					   uint32_t num_channels,
					   bool muted);

/* Autonomous DSP-Centric Engine API */
void steamaudio_dsp_derive_raw_scene(struct steamaudio_comp_data *cd,
				     const struct raw_scene_packet *scene);
void steamaudio_dsp_update_cycle_governor(struct steamaudio_comp_data *cd);

/* Playback-to-Capture Loopback & Battle Bleed Mixer API */
void steamaudio_dsp_battle_bleed_init(struct dsp_battle_bleed_state *bbs, uint32_t sample_rate);
void steamaudio_dsp_battle_bleed_set_config(struct dsp_battle_bleed_state *bbs,
					    const struct sof_steamaudio_battle_bleed_config *cfg);
void steamaudio_dsp_battle_bleed_process(struct dsp_battle_bleed_state *bbs,
					 const float *in_playback,
					 const float *in_mic,
					 float *out_capture,
					 uint32_t frames,
					 uint32_t num_channels,
					 bool muted);

/* High Polyphony Voice Scaling & DSP 3-Tier Level-of-Detail (LOD) API */
void steamaudio_dsp_voice_lod_init(struct dsp_voice_lod_state *vls, uint32_t sample_rate);
void steamaudio_dsp_voice_lod_set_config(struct dsp_voice_lod_state *vls,
					 const struct sof_steamaudio_voice_lod_config *cfg);
void steamaudio_dsp_voice_lod_classify(struct dsp_voice_lod_state *vls,
				       const struct raw_scene_packet *scene,
				       uint8_t *out_tiers);
void steamaudio_dsp_voice_lod_process(struct steamaudio_comp_data *cd,
				      const struct raw_scene_packet *scene,
				      const float *in_pcm,
				      float *out_l,
				      float *out_r,
				      uint32_t frames,
				      bool muted);

/* Scene-Aware 5.1/7.1 Acoustic Upmixer API */
void steamaudio_dsp_upmix_init(struct dsp_upmix_state *ums, uint32_t layout_type, uint32_t sample_rate);
void steamaudio_dsp_upmix_set_config(struct dsp_upmix_state *ums,
				     const struct sof_steamaudio_upmix_config *cfg);
void steamaudio_dsp_upmix_process(struct steamaudio_comp_data *cd,
				  const float *in_stereo_l,
				  const float *in_stereo_r,
				  float out_channels[STEAMAUDIO_MAX_SPEAKERS][256],
				  uint32_t frames,
				  bool muted);

#endif /* __SOF_AUDIO_STEAMAUDIO_H__ */


