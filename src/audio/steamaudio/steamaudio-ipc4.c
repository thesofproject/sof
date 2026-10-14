// SPDX-License-Identifier: Apache-2.0
//
// Copyright (c) 2017-2024 Valve Corporation. All rights reserved.
// Copyright (c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio IPC4 Control Handler

#include "steamaudio.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <rtos/string.h>

LOG_MODULE_DECLARE(steamaudio, CONFIG_SOF_LOG_LEVEL);

__cold int steamaudio_set_config(struct processing_module *mod,
				 uint32_t param_id,
				 enum module_cfg_fragment_position pos,
				 uint32_t data_offset_size,
				 const uint8_t *fragment,
				 size_t fragment_size,
				 uint8_t *response,
				 size_t response_size)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID: {
		struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
		if (ctl->num_elems != 1)
			return -EINVAL;
		cd->enable = (ctl->chanv[0].value != 0);
		comp_info(dev, "steamaudio: enable set to %d", cd->enable);
		return 0;
	}

	case STEAMAUDIO_PARAM_DIRECT_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_direct_config))
			return -EINVAL;

		const struct sof_steamaudio_direct_config *cfg =
			(const struct sof_steamaudio_direct_config *)fragment;

		cd->direct.flags = cfg->flags;
		cd->direct.transmission_type = cfg->transmission_type;
		cd->direct.distance_attenuation = cfg->distance_attenuation;
		cd->direct.directivity = cfg->directivity;
		cd->direct.occlusion = cfg->occlusion;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cd->direct.air_absorption[b] = cfg->air_absorption[b];
			cd->direct.transmission[b] = cfg->transmission[b];
		}

		float dir_gain = 1.0f;
		if (!(cd->mute_mask & (1u << STEAMAUDIO_STEP_DIRECTIVITY))) {
			dir_gain = (cfg->flags & (1 << 2)) ? cfg->directivity : 1.0f;
		}

		cd->direct.target_gain = cfg->distance_attenuation * (1.0f - cfg->occlusion) * dir_gain;
		cd->direct.gain_step = (cd->direct.target_gain - cd->direct.current_gain) / 128.0f;

		steamaudio_dsp_update_direct_eq(cd);

		comp_dbg(dev, "steamaudio: direct dist=%f, occ=%f, dir=%f",
			 (double)cfg->distance_attenuation, (double)cfg->occlusion, (double)dir_gain);
		return 0;
	}

	case STEAMAUDIO_PARAM_BINAURAL_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_binaural_config))
			return -EINVAL;

		const struct sof_steamaudio_binaural_config *cfg =
			(const struct sof_steamaudio_binaural_config *)fragment;

		cd->binaural.direction[0] = cfg->direction[0];
		cd->binaural.direction[1] = cfg->direction[1];
		cd->binaural.direction[2] = cfg->direction[2];
		cd->binaural.interpolation = cfg->interpolation;
		cd->binaural.spatial_blend = cfg->spatial_blend;
		cd->binaural.hrtf_slot_id = cfg->hrtf_slot_id;

		comp_dbg(dev, "steamaudio: binaural dir=(%f, %f, %f)",
			 (double)cfg->direction[0], (double)cfg->direction[1], (double)cfg->direction[2]);
		return 0;
	}

	case STEAMAUDIO_PARAM_REVERB_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_reverb_config))
			return -EINVAL;

		const struct sof_steamaudio_reverb_config *cfg =
			(const struct sof_steamaudio_reverb_config *)fragment;

		cd->reverb.wet_gain = cfg->wet_gain;
		if (cfg->delay_samples > 0 && cfg->delay_samples < STEAMAUDIO_FDN_MAX_DELAY)
			cd->reverb.delay_lengths[0] = cfg->delay_samples;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cd->reverb.reverb_times[b] = cfg->reverb_times[b];
			cd->reverb.eq_gains[b] = cfg->eq_gains[b];
		}
		comp_dbg(dev, "steamaudio: reverb wet=%f delay=%u",
			 (double)cfg->wet_gain, cfg->delay_samples);
		return 0;
	}

	case STEAMAUDIO_PARAM_AMBISONICS_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_ambisonics_config))
			return -EINVAL;

		const struct sof_steamaudio_ambisonics_config *cfg =
			(const struct sof_steamaudio_ambisonics_config *)fragment;

		cd->ambisonics.order = (cfg->order >= 1 && cfg->order <= 3) ? cfg->order : 1;
		cd->ambisonics.num_channels = (cd->ambisonics.order + 1) * (cd->ambisonics.order + 1);
		cd->ambisonics.direction[0] = cfg->direction[0];
		cd->ambisonics.direction[1] = cfg->direction[1];
		cd->ambisonics.direction[2] = cfg->direction[2];
		memcpy(cd->ambisonics.rotation, cfg->listener_rotation, sizeof(cd->ambisonics.rotation));
		comp_dbg(dev, "steamaudio: ambisonics order=%d ch=%d", cd->ambisonics.order, cd->ambisonics.num_channels);
		return 0;
	}

	case STEAMAUDIO_PARAM_PANNING_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_panning_config))
			return -EINVAL;

		const struct sof_steamaudio_panning_config *cfg =
			(const struct sof_steamaudio_panning_config *)fragment;

		if (cfg->layout_type != cd->panning.layout_type)
			steamaudio_dsp_panning_init(&cd->panning, cfg->layout_type);
		float dir[3] = { cfg->direction[0], cfg->direction[1], cfg->direction[2] };
		steamaudio_dsp_panning_set_direction(&cd->panning, dir);
		comp_dbg(dev, "steamaudio: panning layout=%u dir=(%f, %f, %f)",
			 cfg->layout_type, (double)cfg->direction[0],
			 (double)cfg->direction[1], (double)cfg->direction[2]);
		return 0;
	}

	case STEAMAUDIO_PARAM_VIRTUAL_SURROUND_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_virtual_surround_config))
			return -EINVAL;

		const struct sof_steamaudio_virtual_surround_config *cfg =
			(const struct sof_steamaudio_virtual_surround_config *)fragment;

		if (cfg->layout_type != cd->virtual_surround.layout_type)
			steamaudio_dsp_virtual_surround_init(&cd->virtual_surround, cfg->layout_type, cd->sample_rate);
		cd->virtual_surround.hrtf_blend = cfg->hrtf_blend;
		comp_dbg(dev, "steamaudio: virtual surround layout=%u blend=%f",
			 cfg->layout_type, (double)cfg->hrtf_blend);
		return 0;
	}

	case STEAMAUDIO_PARAM_OUTPUT_MODE: {
		if (fragment_size < sizeof(struct sof_steamaudio_output_mode_config))
			return -EINVAL;

		const struct sof_steamaudio_output_mode_config *cfg =
			(const struct sof_steamaudio_output_mode_config *)fragment;

		cd->output_mode = cfg->mode;
		comp_info(dev, "steamaudio: output mode set to %u", cd->output_mode);
		return 0;
	}

	case STEAMAUDIO_PARAM_PATHING_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_pathing_config))
			return -EINVAL;

		const struct sof_steamaudio_pathing_config *cfg =
			(const struct sof_steamaudio_pathing_config *)fragment;

		float eq[STEAMAUDIO_NUM_EQ_BANDS];
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			eq[b] = cfg->eq_coeffs[b];

		float sh[STEAMAUDIO_MAX_HOA_CHANNELS];
		for (int i = 0; i < STEAMAUDIO_MAX_HOA_CHANNELS; i++)
			sh[i] = cfg->sh_coeffs[i];

		float rot[3][3];
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				rot[r][c] = cfg->listener_rotation[r][c];

		steamaudio_dsp_pathing_set_params(&cd->pathing, eq, sh,
						  cfg->order, (cfg->binaural != 0),
						  rot, cd->sample_rate);
		comp_dbg(dev, "steamaudio: pathing order=%u binaural=%u",
			 cfg->order, cfg->binaural);
		return 0;
	}

	case STEAMAUDIO_PARAM_BITSTREAM_MODE: {
		if (fragment_size < sizeof(uint32_t))
			return -EINVAL;
		cd->bitstream_mode = (*((const uint32_t *)fragment) != 0);
		comp_info(dev, "steamaudio: bitstream mode set to %d", cd->bitstream_mode);
		return 0;
	}

	case STEAMAUDIO_PARAM_MUTE_CONFIG: {
		if (fragment_size < sizeof(uint32_t))
			return -EINVAL;

		if (fragment_size >= sizeof(uint32_t) * 4) {
			const uint32_t *pkt = (const uint32_t *)fragment;
			cd->mute_mask = ((uint64_t)pkt[3] << 32) | (uint64_t)pkt[2];
		} else if (fragment_size >= sizeof(struct sof_steamaudio_mute_config)) {
			const struct sof_steamaudio_mute_config *cfg =
				(const struct sof_steamaudio_mute_config *)fragment;
			cd->mute_mask = ((uint64_t)cfg->mute_mask_hi << 32) | (uint64_t)cfg->mute_mask;
		} else if (fragment_size >= sizeof(uint32_t) * 3) {
			const uint32_t *pkt = (const uint32_t *)fragment;
			cd->mute_mask = pkt[2];
		} else if (fragment_size >= sizeof(uint32_t) * 2) {
			const uint32_t *pkt = (const uint32_t *)fragment;
			cd->mute_mask = pkt[1];
		} else {
			cd->mute_mask = *((const uint32_t *)fragment);
		}
		comp_info(dev, "steamaudio: mute_mask set to 0x%08x%08x",
			  (uint32_t)(cd->mute_mask >> 32), (uint32_t)(cd->mute_mask & 0xFFFFFFFFu));
		return 0;
	}

	case STEAMAUDIO_PARAM_GEOMETRY_STREAM: {
		if (fragment_size < sizeof(struct sof_steamaudio_geom_stream_header))
			return -EINVAL;

		const struct sof_steamaudio_geom_stream_payload *payload =
			(const struct sof_steamaudio_geom_stream_payload *)fragment;
		return steamaudio_dsp_dynamic_geom_stream(&cd->dynamic_geom, payload);
	}

	case STEAMAUDIO_PARAM_DIRECTIVITY: {
		if (fragment_size < sizeof(struct sof_steamaudio_directivity_config))
			return -EINVAL;

		const struct sof_steamaudio_directivity_config *cfg =
			(const struct sof_steamaudio_directivity_config *)fragment;

		cd->directivity.source_pos[0] = cfg->source_pos[0];
		cd->directivity.source_pos[1] = cfg->source_pos[1];
		cd->directivity.source_pos[2] = cfg->source_pos[2];

		cd->directivity.source_ahead[0] = cfg->source_ahead[0];
		cd->directivity.source_ahead[1] = cfg->source_ahead[1];
		cd->directivity.source_ahead[2] = cfg->source_ahead[2];

		cd->directivity.source_up[0] = cfg->source_up[0];
		cd->directivity.source_up[1] = cfg->source_up[1];
		cd->directivity.source_up[2] = cfg->source_up[2];

		cd->directivity.listener_pos[0] = cfg->listener_pos[0];
		cd->directivity.listener_pos[1] = cfg->listener_pos[1];
		cd->directivity.listener_pos[2] = cfg->listener_pos[2];

		cd->directivity.dipole_weight = cfg->dipole_weight;
		cd->directivity.dipole_power = cfg->dipole_power;
		cd->directivity.freq_dependent = cfg->freq_dependent;

		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cd->directivity.band_weights[b] = cfg->band_weights[b];
			cd->directivity.band_powers[b] = cfg->band_powers[b];
		}

		if (cd->directivity.freq_dependent) {
			steamaudio_dsp_calculate_directivity_3band(
				cd->directivity.source_pos,
				cd->directivity.source_ahead,
				cd->directivity.listener_pos,
				cd->directivity.dipole_weight,
				cd->directivity.dipole_power,
				cd->directivity.calculated_eq);
			cd->directivity.calculated_gain = (cd->directivity.calculated_eq[0] +
							   cd->directivity.calculated_eq[1] +
							   cd->directivity.calculated_eq[2]) / 3.0f;
		} else {
			cd->directivity.calculated_gain = steamaudio_dsp_calculate_directivity(
				cd->directivity.source_pos,
				cd->directivity.source_ahead,
				cd->directivity.listener_pos,
				cd->directivity.dipole_weight,
				cd->directivity.dipole_power);
			cd->directivity.calculated_eq[0] = cd->directivity.calculated_gain;
			cd->directivity.calculated_eq[1] = cd->directivity.calculated_gain;
			cd->directivity.calculated_eq[2] = cd->directivity.calculated_gain;
		}

		if (cd->mute_mask & (1u << STEAMAUDIO_STEP_DIRECTIVITY)) {
			cd->directivity.calculated_gain = 1.0f;
			cd->directivity.calculated_eq[0] = 1.0f;
			cd->directivity.calculated_eq[1] = 1.0f;
			cd->directivity.calculated_eq[2] = 1.0f;
		}

		comp_dbg(dev, "steamaudio: directivity w=%f, p=%f, gain=%f",
			 (double)cd->directivity.dipole_weight,
			 (double)cd->directivity.dipole_power,
			 (double)cd->directivity.calculated_gain);
		return 0;
	}

	case STEAMAUDIO_PARAM_ATMOSPHERE: {
		if (fragment_size < sizeof(struct sof_steamaudio_atmosphere_config))
			return -EINVAL;

		const struct sof_steamaudio_atmosphere_config *cfg =
			(const struct sof_steamaudio_atmosphere_config *)fragment;

		cd->atmosphere.temperature_c = cfg->temperature_c;
		cd->atmosphere.relative_humidity = cfg->relative_humidity;
		cd->atmosphere.pressure_kpa = cfg->pressure_kpa;
		cd->atmosphere.enabled = (cfg->flags & 1) != 0;
		cd->atmosphere.flags = cfg->flags;

		if (cfg->flags & (1 << 1)) {
			cd->atmosphere.speed_of_sound = cfg->speed_of_sound;
			for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
				cd->atmosphere.absorption_coefficients[b] = cfg->absorption_coefficients[b];
		} else {
			steamaudio_dsp_calculate_atmosphere(
				cd->atmosphere.temperature_c,
				cd->atmosphere.relative_humidity,
				cd->atmosphere.pressure_kpa,
				&cd->atmosphere.speed_of_sound,
				cd->atmosphere.absorption_coefficients);
		}

		comp_dbg(dev, "steamaudio: atmosphere T=%f C, RH=%f, c=%f m/s, alpha=[%f, %f, %f]",
			 (double)cd->atmosphere.temperature_c,
			 (double)cd->atmosphere.relative_humidity,
			 (double)cd->atmosphere.speed_of_sound,
			 (double)cd->atmosphere.absorption_coefficients[0],
			 (double)cd->atmosphere.absorption_coefficients[1],
			 (double)cd->atmosphere.absorption_coefficients[2]);
		return 0;
	}

	case STEAMAUDIO_PARAM_DIFFRACTION: {
		if (fragment_size < sizeof(struct sof_steamaudio_diffraction_config))
			return -EINVAL;

		const struct sof_steamaudio_diffraction_config *cfg =
			(const struct sof_steamaudio_diffraction_config *)fragment;

		cd->diffraction.wedge_angle_rad = cfg->wedge_angle_rad;
		cd->diffraction.deviation_angle_rad = cfg->deviation_angle_rad;
		cd->diffraction.r_source = cfg->r_source;
		cd->diffraction.r_receiver = cfg->r_receiver;
		cd->diffraction.speed_of_sound = (cfg->speed_of_sound > 50.0f) ? cfg->speed_of_sound : cd->atmosphere.speed_of_sound;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			cd->diffraction.transmission[b] = cfg->transmission[b];

		cd->diffraction.enabled = (cfg->flags & 1) != 0;
		cd->diffraction.flags = cfg->flags;

		if (cfg->flags & (1 << 1)) {
			for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
				cd->diffraction.diffraction_coeffs[b] = cfg->diffraction_coeffs[b];
				cd->diffraction.combined_gains[b] = cfg->combined_gains[b];
			}
		} else {
			steamaudio_dsp_calculate_edge_diffraction(
				cd->diffraction.wedge_angle_rad,
				cd->diffraction.deviation_angle_rad,
				cd->diffraction.r_source,
				cd->diffraction.r_receiver,
				cd->diffraction.speed_of_sound,
				cd->diffraction.diffraction_coeffs);
			steamaudio_dsp_calculate_obstacle_pathing(
				cd->diffraction.diffraction_coeffs,
				cd->diffraction.transmission,
				cd->diffraction.combined_gains);
		}

		comp_dbg(dev, "steamaudio: diffraction wedge=%f dev=%f D=[%f, %f, %f] comb=[%f, %f, %f]",
			 (double)cd->diffraction.wedge_angle_rad,
			 (double)cd->diffraction.deviation_angle_rad,
			 (double)cd->diffraction.diffraction_coeffs[0],
			 (double)cd->diffraction.diffraction_coeffs[1],
			 (double)cd->diffraction.diffraction_coeffs[2],
			 (double)cd->diffraction.combined_gains[0],
			 (double)cd->diffraction.combined_gains[1],
			 (double)cd->diffraction.combined_gains[2]);
		return 0;
	}

	case STEAMAUDIO_PARAM_PROBEBATCH: {
		if (fragment_size < sizeof(struct sof_steamaudio_probe_batch_config))
			return -EINVAL;

		const struct sof_steamaudio_probe_batch_config *cfg =
			(const struct sof_steamaudio_probe_batch_config *)fragment;

		uint32_t num_p = cfg->num_probes;
		if (num_p > STEAMAUDIO_MAX_PROBES)
			num_p = STEAMAUDIO_MAX_PROBES;
		cd->probe_batch.num_probes = num_p;

		for (uint32_t i = 0; i < num_p; i++) {
			cd->probe_batch.probes[i] = cfg->probes[i];
		}

		uint32_t num_q = cfg->num_queries;
		if (num_q > STEAMAUDIO_MAX_PROBE_QUERIES)
			num_q = STEAMAUDIO_MAX_PROBE_QUERIES;
		cd->probe_batch.num_queries = num_q;

		for (uint32_t q = 0; q < num_q; q++) {
			cd->probe_batch.query_positions[q][0] = cfg->query_positions[q][0];
			cd->probe_batch.query_positions[q][1] = cfg->query_positions[q][1];
			cd->probe_batch.query_positions[q][2] = cfg->query_positions[q][2];
		}

		cd->probe_batch.enabled = (cfg->flags & 1) != 0;
		cd->probe_batch.flags = cfg->flags;

		if (cfg->flags & (1 << 1)) {
			/* Override from config payload */
			for (uint32_t q = 0; q < num_q; q++) {
				for (int n = 0; n < STEAMAUDIO_MAX_NEIGHBORS; n++) {
					cd->probe_batch.neighbor_indices[q][n] = cfg->neighbor_indices[q][n];
					cd->probe_batch.neighbor_weights[q][n] = cfg->neighbor_weights[q][n];
				}
				for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
					cd->probe_batch.interpolated_eq[q][b] = cfg->interpolated_eq[q][b];
			}
		} else {
			/* Execute spatial lookup and distance-weighted interpolation on DSP */
			steamaudio_dsp_probe_batch_process(&cd->probe_batch);
		}

		comp_dbg(dev, "steamaudio: probe_batch num_probes=%u num_queries=%u",
			 cd->probe_batch.num_probes, cd->probe_batch.num_queries);
		return 0;
	}

	case STEAMAUDIO_PARAM_SOFA_HRIR: {
		if (fragment_size < sizeof(struct sof_steamaudio_sofa_hrir_config))
			return -EINVAL;

		const struct sof_steamaudio_sofa_hrir_config *cfg =
			(const struct sof_steamaudio_sofa_hrir_config *)fragment;

		uint32_t num_t = cfg->num_taps;
		if (num_t > STEAMAUDIO_MAX_HRIR_TAPS)
			num_t = STEAMAUDIO_MAX_HRIR_TAPS;
		if (num_t == 0)
			num_t = 1;
		cd->sofa_hrir.num_taps = num_t;

		for (uint32_t i = 0; i < num_t; i++) {
			cd->sofa_hrir.hrir_left[i] = cfg->hrir_left[i];
			cd->sofa_hrir.hrir_right[i] = cfg->hrir_right[i];
		}
		for (uint32_t i = num_t; i < STEAMAUDIO_MAX_HRIR_TAPS; i++) {
			cd->sofa_hrir.hrir_left[i] = 0.0f;
			cd->sofa_hrir.hrir_right[i] = 0.0f;
		}

		cd->sofa_hrir.direction[0] = cfg->direction[0];
		cd->sofa_hrir.direction[1] = cfg->direction[1];
		cd->sofa_hrir.direction[2] = cfg->direction[2];
		cd->sofa_hrir.spatial_blend = cfg->spatial_blend;
		cd->sofa_hrir.volume = (cfg->volume > 0.0f) ? cfg->volume : 1.0f;
		cd->sofa_hrir.enabled = (cfg->flags & 1) != 0;
		cd->sofa_hrir.flags = cfg->flags;

		if (cfg->flags & (1 << 1)) {
			memset(cd->sofa_hrir.history, 0, sizeof(cd->sofa_hrir.history));
			cd->sofa_hrir.history_idx = 0;
		}

		comp_dbg(dev, "steamaudio: sofa_hrir num_taps=%u blend=%f vol=%f en=%u",
			 cd->sofa_hrir.num_taps, (double)cd->sofa_hrir.spatial_blend,
			 (double)cd->sofa_hrir.volume, cd->sofa_hrir.enabled);
		return 0;
	}

	case STEAMAUDIO_PARAM_GRAPH_SEARCH: {
		if (fragment_size < sizeof(struct sof_steamaudio_graph_search_config))
			return -EINVAL;

		const struct sof_steamaudio_graph_search_config *cfg =
			(const struct sof_steamaudio_graph_search_config *)fragment;

		uint16_t num_n = cfg->num_nodes;
		if (num_n > STEAMAUDIO_MAX_GRAPH_NODES)
			num_n = STEAMAUDIO_MAX_GRAPH_NODES;

		cd->graph_search.num_nodes = num_n;
		for (uint16_t i = 0; i < num_n; i++) {
			cd->graph_search.nodes[i].num_edges = cfg->nodes[i].num_edges;
			if (cd->graph_search.nodes[i].num_edges > STEAMAUDIO_MAX_GRAPH_EDGES_PER_NODE)
				cd->graph_search.nodes[i].num_edges = STEAMAUDIO_MAX_GRAPH_EDGES_PER_NODE;
			for (uint16_t e = 0; e < cd->graph_search.nodes[i].num_edges; e++) {
				cd->graph_search.nodes[i].edges[e].node = cfg->nodes[i].edges[e].node;
				cd->graph_search.nodes[i].edges[e].cost = cfg->nodes[i].edges[e].cost;
			}
			cd->graph_search.nodes[i].pos[0] = cfg->nodes[i].pos[0];
			cd->graph_search.nodes[i].pos[1] = cfg->nodes[i].pos[1];
			cd->graph_search.nodes[i].pos[2] = cfg->nodes[i].pos[2];
		}

		cd->graph_search.start_node = cfg->start_node;
		cd->graph_search.target_node = cfg->target_node;
		cd->graph_search.max_range = (cfg->max_range > 0.0f) ? cfg->max_range : 1000.0f;
		cd->graph_search.enabled = (cfg->enabled != 0);
		cd->graph_search.flags = cfg->flags;

		/* Execute search query on demand */
		bool muted = (cd->mute_mask & (1u << STEAMAUDIO_STEP_GRAPH_SEARCH)) != 0;
		steamaudio_dsp_graph_search_find_path(&cd->graph_search,
						      cfg->start_node,
						      cfg->target_node,
						      cd->graph_search.max_range,
						      muted);

		comp_dbg(dev, "steamaudio: graph_search nodes=%u start=%u target=%u found=%u path_len=%u cost=%f",
			 cd->graph_search.num_nodes, cfg->start_node, cfg->target_node,
			 cd->graph_search.path_found, cd->graph_search.num_path_nodes,
			 (double)cd->graph_search.total_cost);
		return 0;
	}

	case STEAMAUDIO_PARAM_REFLECTION_MIXER: {
		if (fragment_size < sizeof(struct sof_steamaudio_reflection_mixer_config))
			return -EINVAL;

		const struct sof_steamaudio_reflection_mixer_config *cfg =
			(const struct sof_steamaudio_reflection_mixer_config *)fragment;

		uint32_t num_s = cfg->num_sources;
		if (num_s > STEAMAUDIO_MAX_MIXER_SOURCES)
			num_s = STEAMAUDIO_MAX_MIXER_SOURCES;

		uint32_t num_ch = cfg->num_channels;
		if (num_ch > STEAMAUDIO_MAX_MIXER_CHANNELS)
			num_ch = STEAMAUDIO_MAX_MIXER_CHANNELS;
		if (num_ch == 0)
			num_ch = 2;

		uint32_t frames = cfg->frames;
		if (frames > STEAMAUDIO_MAX_MIXER_FRAMES)
			frames = STEAMAUDIO_MAX_MIXER_FRAMES;
		if (frames == 0)
			frames = 256;

		cd->reflection_mixer.num_sources = num_s;
		cd->reflection_mixer.num_channels = num_ch;
		cd->reflection_mixer.frames = frames;
		cd->reflection_mixer.enabled = (cfg->flags & 1) != 0;
		cd->reflection_mixer.flags = cfg->flags;

		for (uint32_t s = 0; s < STEAMAUDIO_MAX_MIXER_SOURCES; s++) {
			cd->reflection_mixer.source_gains[s] = cfg->source_gains[s];
		}

		if (cfg->flags & (1 << 1)) {
			steamaudio_dsp_reflection_mixer_reset(&cd->reflection_mixer);
		}

		comp_dbg(dev, "steamaudio: reflection_mixer sources=%u ch=%u frames=%u en=%u",
			 cd->reflection_mixer.num_sources, cd->reflection_mixer.num_channels,
			 cd->reflection_mixer.frames, cd->reflection_mixer.enabled);
		return 0;
	}

	case STEAMAUDIO_PARAM_INSTANCED_MESH: {
		if (fragment_size < sizeof(struct sof_steamaudio_instanced_mesh_config))
			return -EINVAL;

		const struct sof_steamaudio_instanced_mesh_config *cfg =
			(const struct sof_steamaudio_instanced_mesh_config *)fragment;

		switch (cfg->op) {
		case STEAMAUDIO_INSTANCED_MESH_OP_SET_PROTOTYPE:
			steamaudio_dsp_instanced_mesh_set_prototype(&cd->instanced_mesh,
								    cfg->prototype_id,
								    cfg->triangles,
								    cfg->num_triangles);
			break;
		case STEAMAUDIO_INSTANCED_MESH_OP_SET_INSTANCE: {
			float transform[16];
			memcpy(transform, cfg->transform, sizeof(transform));
			steamaudio_dsp_instanced_mesh_set_instance(&cd->instanced_mesh,
								   cfg->instance_id,
								   cfg->prototype_id,
								   transform);
			break;
		}
		case STEAMAUDIO_INSTANCED_MESH_OP_UPDATE_TRANSFORM: {
			float transform[16];
			memcpy(transform, cfg->transform, sizeof(transform));
			steamaudio_dsp_instanced_mesh_update_transform(&cd->instanced_mesh,
								       cfg->instance_id,
								       transform);
			break;
		}
		case STEAMAUDIO_INSTANCED_MESH_OP_REMOVE_INSTANCE:
			steamaudio_dsp_instanced_mesh_remove_instance(&cd->instanced_mesh,
								      cfg->instance_id);
			break;
		case STEAMAUDIO_INSTANCED_MESH_OP_CLEAR:
			steamaudio_dsp_instanced_mesh_clear(&cd->instanced_mesh);
			break;
		default:
			break;
		}

		comp_dbg(dev, "steamaudio: instanced_mesh op=%u insts=%u protos=%u",
			 cfg->op, cd->instanced_mesh.num_instances, cd->instanced_mesh.num_prototypes);
		return 0;
	}

	case STEAMAUDIO_PARAM_RAY_TRACER: {
		if (fragment_size < sizeof(struct sof_steamaudio_ray_tracer_config))
			return -EINVAL;

		const struct sof_steamaudio_ray_tracer_config *cfg =
			(const struct sof_steamaudio_ray_tracer_config *)fragment;

		steamaudio_dsp_ray_tracer_set_config(&cd->ray_tracer, cfg);
		comp_dbg(dev, "steamaudio: ray_tracer set_config rays=%u bounces=%u",
			 cfg->num_rays, cfg->max_bounces);
		return 0;
	}

	case STEAMAUDIO_PARAM_REVERB_ESTIMATOR: {
		if (fragment_size < sizeof(struct sof_steamaudio_reverb_estimator_config))
			return -EINVAL;

		const struct sof_steamaudio_reverb_estimator_config *cfg =
			(const struct sof_steamaudio_reverb_estimator_config *)fragment;

		steamaudio_dsp_reverb_estimator_set_config(&cd->reverb_estimator, cfg);
		comp_dbg(dev, "steamaudio: reverb_estimator set_config bins=%u dt=%.3f",
			 cfg->num_bins, (double)cfg->bin_duration_s);
		return 0;
	}

	case STEAMAUDIO_PARAM_EARLY_REFLECTIONS: {
		if (fragment_size < sizeof(struct sof_steamaudio_early_reflections_config))
			return -EINVAL;

		const struct sof_steamaudio_early_reflections_config *cfg =
			(const struct sof_steamaudio_early_reflections_config *)fragment;

		steamaudio_dsp_early_reflections_set_config(&cd->early_reflections, cfg);
		comp_dbg(dev, "steamaudio: early_reflections set_config taps=%u ch=%u en=%u",
			 cfg->num_taps, cfg->num_channels, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_MATERIAL_TRANSMISSION: {
		if (fragment_size < sizeof(struct sof_steamaudio_material_transmission_config))
			return -EINVAL;

		const struct sof_steamaudio_material_transmission_config *cfg =
			(const struct sof_steamaudio_material_transmission_config *)fragment;

		steamaudio_dsp_material_transmission_set_config(&cd->material_transmission, cfg);
		comp_dbg(dev, "steamaudio: material_transmission set_config layers=%u comp=[%.3f, %.3f, %.3f] en=%u",
			 cfg->num_layers,
			 (double)cd->material_transmission.composite_transmission[0],
			 (double)cd->material_transmission.composite_transmission[1],
			 (double)cd->material_transmission.composite_transmission[2],
			 (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_ACOUSTIC_PORTALS: {
		if (fragment_size < sizeof(struct sof_steamaudio_acoustic_portals_config))
			return -EINVAL;

		const struct sof_steamaudio_acoustic_portals_config *cfg =
			(const struct sof_steamaudio_acoustic_portals_config *)fragment;

		steamaudio_dsp_acoustic_portals_set_config(&cd->acoustic_portals, cfg);
		comp_dbg(dev, "steamaudio: acoustic_portals set_config portals=%u en=%u",
			 cfg->num_portals, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_VOLUMETRIC_SOURCE: {
		if (fragment_size < sizeof(struct sof_steamaudio_volumetric_source_config))
			return -EINVAL;

		const struct sof_steamaudio_volumetric_source_config *cfg =
			(const struct sof_steamaudio_volumetric_source_config *)fragment;

		steamaudio_dsp_volumetric_source_set_config(&cd->volumetric_source, cfg);
		comp_dbg(dev, "steamaudio: volumetric_source set_config sources=%u en=%u",
			 cfg->num_sources, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_SOURCE_PRIORITIZATION: {
		if (fragment_size < sizeof(struct sof_steamaudio_source_prioritization_config))
			return -EINVAL;

		const struct sof_steamaudio_source_prioritization_config *cfg =
			(const struct sof_steamaudio_source_prioritization_config *)fragment;

		steamaudio_dsp_source_prioritization_set_config(&cd->source_prioritization, cfg);
		comp_dbg(dev, "steamaudio: source_prioritization set_config sources=%u voices=%u en=%u",
			 cfg->num_sources, cfg->max_voices, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_GROUND_REFLECTION: {
		if (fragment_size < sizeof(struct sof_steamaudio_ground_reflection_config))
			return -EINVAL;

		const struct sof_steamaudio_ground_reflection_config *cfg =
			(const struct sof_steamaudio_ground_reflection_config *)fragment;

		steamaudio_dsp_ground_reflection_set_config(&cd->ground_reflection, cfg);
		comp_dbg(dev, "steamaudio: ground_reflection set_config mat=%u en=%u",
			 cfg->material_preset, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_TRUE_PEAK_LIMITER: {
		if (fragment_size < sizeof(struct sof_steamaudio_limiter_config))
			return -EINVAL;

		const struct sof_steamaudio_limiter_config *cfg =
			(const struct sof_steamaudio_limiter_config *)fragment;

		steamaudio_dsp_true_peak_limiter_set_config(&cd->true_peak_limiter, cfg);
		comp_dbg(dev, "steamaudio: true_peak_limiter set_config thresh=%f ceiling=%f en=%u",
			 (double)cfg->threshold_db, (double)cfg->ceiling_db, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_ROOM_MODES: {
		if (fragment_size < sizeof(struct sof_steamaudio_room_modes_config))
			return -EINVAL;

		const struct sof_steamaudio_room_modes_config *cfg =
			(const struct sof_steamaudio_room_modes_config *)fragment;

		steamaudio_dsp_room_modes_set_config(&cd->room_modes, cfg);
		comp_dbg(dev, "steamaudio: room_modes set_config dim=[%f,%f,%f] en=%u",
			 (double)cfg->room_dimensions[0], (double)cfg->room_dimensions[1],
			 (double)cfg->room_dimensions[2], (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_ATMOSPHERIC_TURBULENCE: {
		if (fragment_size < sizeof(struct sof_steamaudio_atmospheric_turbulence_config))
			return -EINVAL;

		const struct sof_steamaudio_atmospheric_turbulence_config *cfg =
			(const struct sof_steamaudio_atmospheric_turbulence_config *)fragment;

		steamaudio_dsp_atmospheric_turbulence_set_config(&cd->atmospheric_turbulence, cfg);
		comp_dbg(dev, "steamaudio: atmospheric_turbulence set_config wind=[%f,%f,%f] en=%u",
			 (double)cfg->wind_velocity[0], (double)cfg->wind_velocity[1],
			 (double)cfg->wind_velocity[2], (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_SURFACE_SCATTERING: {
		if (fragment_size < sizeof(struct sof_steamaudio_surface_scattering_config))
			return -EINVAL;

		const struct sof_steamaudio_surface_scattering_config *cfg =
			(const struct sof_steamaudio_surface_scattering_config *)fragment;

		steamaudio_dsp_surface_scattering_set_config(&cd->surface_scattering, cfg);
		comp_dbg(dev, "steamaudio: surface_scattering set_config rough=%f diff_frac=%f en=%u",
			 (double)cfg->roughness_rms, (double)cfg->diffuse_fraction, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_SOUND_BARRIER: {
		if (fragment_size < sizeof(struct sof_steamaudio_sound_barrier_config))
			return -EINVAL;

		const struct sof_steamaudio_sound_barrier_config *cfg =
			(const struct sof_steamaudio_sound_barrier_config *)fragment;

		steamaudio_dsp_sound_barrier_set_config(&cd->sound_barrier, cfg);
		comp_dbg(dev, "steamaudio: sound_barrier set_config height=%f type=%u en=%u",
			 (double)cfg->barrier_height, cfg->barrier_type, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_NEAR_FIELD: {
		if (fragment_size < sizeof(struct sof_steamaudio_near_field_config))
			return -EINVAL;

		const struct sof_steamaudio_near_field_config *cfg =
			(const struct sof_steamaudio_near_field_config *)fragment;

		steamaudio_dsp_near_field_set_config(&cd->near_field, cfg);
		comp_dbg(dev, "steamaudio: near_field set_config head_radius=%f ref_dist=%f en=%u",
			 (double)cfg->head_radius, (double)cfg->reference_distance, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_NONLINEAR_WAVE: {
		if (fragment_size < sizeof(struct sof_steamaudio_nonlinear_wave_config))
			return -EINVAL;

		const struct sof_steamaudio_nonlinear_wave_config *cfg =
			(const struct sof_steamaudio_nonlinear_wave_config *)fragment;

		steamaudio_dsp_nonlinear_wave_set_config(&cd->nonlinear_wave, cfg);
		comp_dbg(dev, "steamaudio: nonlinear_wave set_config spl=%f dist=%f en=%u",
			 (double)cfg->source_spl_db, (double)cfg->distance_m, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_RAW_SCENE: {
		if (fragment_size < sizeof(struct raw_scene_packet))
			return -EINVAL;

		const struct raw_scene_packet *scene =
			(const struct raw_scene_packet *)fragment;

		if (scene->sync_word != STEAMAUDIO_RAW_SCENE_MAGIC)
			return -EINVAL;

		if (scene->header_bytes && scene->header_bytes != sizeof(struct raw_scene_packet))
			return -EINVAL;

		if (scene->emitter_stride && scene->emitter_stride != sizeof(struct raw_emitter_descriptor))
			return -EINVAL;

		size_t expected_size = sizeof(struct raw_scene_packet) +
				       scene->num_emitters * sizeof(struct raw_emitter_descriptor);
		if (fragment_size < expected_size)
			return -EINVAL;

		if (scene->total_frame_bytes && fragment_size < scene->total_frame_bytes)
			return -EINVAL;

		if (scene->pcm_bytes > 0) {
			if (scene->pcm_offset < expected_size)
				return -EINVAL;
			if (fragment_size < scene->pcm_offset + scene->pcm_bytes)
				return -EINVAL;
		}

		steamaudio_dsp_derive_raw_scene(cd, scene);
		comp_dbg(dev, "steamaudio: raw scene seq=%u emitters=%u frame=%u pcm_bytes=%u",
			 scene->seq_id, scene->num_emitters, scene->frame_index, scene->pcm_bytes);
		return 0;
	}

	case STEAMAUDIO_PARAM_BATTLE_BLEED: {
		if (fragment_size < sizeof(struct sof_steamaudio_battle_bleed_config))
			return -EINVAL;

		const struct sof_steamaudio_battle_bleed_config *cfg =
			(const struct sof_steamaudio_battle_bleed_config *)fragment;

		steamaudio_dsp_battle_bleed_set_config(&cd->battle_bleed, cfg);
		comp_dbg(dev, "steamaudio: battle_bleed set_config vol=%f duck=%f db en=%u",
			 (double)cfg->bleed_volume, (double)cfg->ducking_depth_db, (cfg->flags & 1));
		return 0;
	}

	case STEAMAUDIO_PARAM_VOICE_LOD: {
		if (fragment_size < sizeof(struct sof_steamaudio_voice_lod_config))
			return -EINVAL;

		const struct sof_steamaudio_voice_lod_config *cfg =
			(const struct sof_steamaudio_voice_lod_config *)fragment;

		steamaudio_dsp_voice_lod_set_config(&cd->voice_lod, cfg);
		comp_dbg(dev, "steamaudio: voice_lod set_config t1=%u t2=%u t3=%u d1=%f d2=%f en=%u",
			 cfg->max_tier1_voices, cfg->max_tier2_voices, cfg->max_tier3_voices,
			 (double)cfg->tier1_distance_m, (double)cfg->tier2_distance_m, cfg->enabled);
		return 0;
	}

	case STEAMAUDIO_PARAM_SCENE_UPMIX: {
		if (fragment_size < sizeof(struct sof_steamaudio_upmix_config))
			return -EINVAL;

		const struct sof_steamaudio_upmix_config *cfg =
			(const struct sof_steamaudio_upmix_config *)fragment;

		steamaudio_dsp_upmix_set_config(&cd->upmix, cfg);
		comp_dbg(dev, "steamaudio: scene_upmix set_config layout=%u spread=%f decorr=%f rev_surr=%f fc=%f",
			 cfg->layout_type, (double)cfg->center_spread, (double)cfg->ambient_decorrelation,
			 (double)cfg->reverb_surround_mix, (double)cfg->crossover_freq_hz);
		return 0;
	}

	default:
		comp_warn(dev, "steamaudio: unhandled param_id 0x%x", param_id);
		return -EINVAL;
	}
}

__cold int steamaudio_get_config(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	switch (config_id) {
	case STEAMAUDIO_PARAM_DIRECT_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_direct_config))
			return -EINVAL;

		struct sof_steamaudio_direct_config *cfg = (struct sof_steamaudio_direct_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_DIRECT_CONFIG;
		cfg->flags = cd->direct.flags;
		cfg->transmission_type = cd->direct.transmission_type;
		cfg->distance_attenuation = cd->direct.distance_attenuation ? cd->direct.distance_attenuation : cd->direct.current_gain;
		cfg->directivity = cd->direct.directivity ? cd->direct.directivity : 1.0f;
		cfg->occlusion = cd->direct.occlusion;
		for (int i = 0; i < STEAMAUDIO_NUM_EQ_BANDS; i++) {
			cfg->air_absorption[i] = cd->direct.air_absorption[i] ? cd->direct.air_absorption[i] : 1.0f;
			cfg->transmission[i] = cd->direct.transmission[i] ? cd->direct.transmission[i] : 1.0f;
		}
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_BINAURAL_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_binaural_config))
			return -EINVAL;

		struct sof_steamaudio_binaural_config *cfg = (struct sof_steamaudio_binaural_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_BINAURAL_CONFIG;
		cfg->direction[0] = cd->binaural.direction[0];
		cfg->direction[1] = cd->binaural.direction[1];
		cfg->direction[2] = cd->binaural.direction[2];
		cfg->interpolation = cd->binaural.interpolation;
		cfg->spatial_blend = cd->binaural.spatial_blend;
		cfg->hrtf_slot_id = cd->binaural.hrtf_slot_id;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_REVERB_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_reverb_config))
			return -EINVAL;

		struct sof_steamaudio_reverb_config *cfg = (struct sof_steamaudio_reverb_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_REVERB_CONFIG;
		for (int i = 0; i < STEAMAUDIO_NUM_EQ_BANDS; i++) {
			cfg->reverb_times[i] = cd->reverb.reverb_times[i] ? cd->reverb.reverb_times[i] : 1.0f;
			cfg->eq_gains[i] = cd->reverb.eq_gains[i] ? cd->reverb.eq_gains[i] : 1.0f;
		}
		cfg->delay_samples = cd->reverb.delay_lengths[0];
		cfg->wet_gain = cd->reverb.wet_gain;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_AMBISONICS_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_ambisonics_config))
			return -EINVAL;

		struct sof_steamaudio_ambisonics_config *cfg = (struct sof_steamaudio_ambisonics_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_AMBISONICS_CONFIG;
		cfg->order = cd->ambisonics.order;
		cfg->direction[0] = cd->ambisonics.direction[0];
		cfg->direction[1] = cd->ambisonics.direction[1];
		cfg->direction[2] = cd->ambisonics.direction[2];
		memcpy(cfg->listener_rotation, cd->ambisonics.rotation, sizeof(cfg->listener_rotation));
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_PANNING_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_panning_config))
			return -EINVAL;

		struct sof_steamaudio_panning_config *cfg = (struct sof_steamaudio_panning_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_PANNING_CONFIG;
		cfg->layout_type = cd->panning.layout_type;
		cfg->direction[0] = cd->panning.direction[0];
		cfg->direction[1] = cd->panning.direction[1];
		cfg->direction[2] = cd->panning.direction[2];
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_VIRTUAL_SURROUND_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_virtual_surround_config))
			return -EINVAL;

		struct sof_steamaudio_virtual_surround_config *cfg = (struct sof_steamaudio_virtual_surround_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_VIRTUAL_SURROUND_CONFIG;
		cfg->layout_type = cd->virtual_surround.layout_type;
		cfg->hrtf_blend = cd->virtual_surround.hrtf_blend;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_OUTPUT_MODE: {
		if (fragment_size < sizeof(struct sof_steamaudio_output_mode_config))
			return -EINVAL;

		struct sof_steamaudio_output_mode_config *cfg = (struct sof_steamaudio_output_mode_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_OUTPUT_MODE;
		cfg->mode = cd->output_mode;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_PATHING_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_pathing_config))
			return -EINVAL;

		struct sof_steamaudio_pathing_config *cfg = (struct sof_steamaudio_pathing_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_PATHING_CONFIG;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			cfg->eq_coeffs[b] = cd->pathing.eq_coeffs[b];
		for (int i = 0; i < STEAMAUDIO_MAX_HOA_CHANNELS; i++)
			cfg->sh_coeffs[i] = cd->pathing.sh_coeffs[i];
		cfg->order = cd->pathing.order;
		cfg->binaural = cd->pathing.binaural ? 1 : 0;
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				cfg->listener_rotation[r][c] = cd->pathing.rotation[r][c];
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_BVH_QUERY: {
		if (fragment_size < sizeof(struct sof_steamaudio_bvh_query))
			return -EINVAL;

		struct sof_steamaudio_bvh_query *query = (struct sof_steamaudio_bvh_query *)fragment;
		struct dsp_vec3 src = { query->source[0], query->source[1], query->source[2] };
		struct dsp_vec3 lis = { query->listener[0], query->listener[1], query->listener[2] };
		float occ = 0.0f;
		if (!(cd->mute_mask & (1u << STEAMAUDIO_STEP_OCCLUSION))) {
			occ = steamaudio_dsp_test_occlusion(&cd->scene, src, lis);
			if (occ < 0.5f && !(cd->mute_mask & (1u << STEAMAUDIO_STEP_GEOMETRY))) {
				float dyn_trans[3];
				float dyn_occ = steamaudio_dsp_test_dynamic_occlusion(&cd->dynamic_geom, src, lis, dyn_trans);
				if (dyn_occ > occ)
					occ = dyn_occ;
			}
		}
		query->comp_type = STEAMAUDIO_PARAM_BVH_QUERY;
		query->occlusion_result = occ;
		query->has_line_of_sight = (occ < 0.5f) ? 1 : 0;
		if (data_offset_size)
			*data_offset_size = sizeof(*query);
		return 0;
	}

	case STEAMAUDIO_PARAM_MUTE_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_mute_config))
			return -EINVAL;

		struct sof_steamaudio_mute_config *cfg = (struct sof_steamaudio_mute_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_MUTE_CONFIG;
		cfg->mute_mask = (uint32_t)(cd->mute_mask & 0xFFFFFFFFu);
		cfg->mute_mask_hi = (uint32_t)((cd->mute_mask >> 32) & 0xFFFFFFFFu);
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_DIRECTIVITY: {
		if (fragment_size < sizeof(struct sof_steamaudio_directivity_config))
			return -EINVAL;

		struct sof_steamaudio_directivity_config *cfg =
			(struct sof_steamaudio_directivity_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_DIRECTIVITY;
		cfg->source_pos[0] = cd->directivity.source_pos[0];
		cfg->source_pos[1] = cd->directivity.source_pos[1];
		cfg->source_pos[2] = cd->directivity.source_pos[2];
		cfg->source_ahead[0] = cd->directivity.source_ahead[0];
		cfg->source_ahead[1] = cd->directivity.source_ahead[1];
		cfg->source_ahead[2] = cd->directivity.source_ahead[2];
		cfg->source_up[0] = cd->directivity.source_up[0];
		cfg->source_up[1] = cd->directivity.source_up[1];
		cfg->source_up[2] = cd->directivity.source_up[2];
		cfg->listener_pos[0] = cd->directivity.listener_pos[0];
		cfg->listener_pos[1] = cd->directivity.listener_pos[1];
		cfg->listener_pos[2] = cd->directivity.listener_pos[2];
		cfg->dipole_weight = cd->directivity.dipole_weight;
		cfg->dipole_power = cd->directivity.dipole_power;
		cfg->freq_dependent = cd->directivity.freq_dependent;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cfg->band_weights[b] = cd->directivity.band_weights[b];
			cfg->band_powers[b] = cd->directivity.band_powers[b];
			cfg->directivity_eq[b] = (cd->mute_mask & (1u << STEAMAUDIO_STEP_DIRECTIVITY)) ?
				1.0f : cd->directivity.calculated_eq[b];
		}
		cfg->directivity_gain = (cd->mute_mask & (1u << STEAMAUDIO_STEP_DIRECTIVITY)) ?
			1.0f : cd->directivity.calculated_gain;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_ATMOSPHERE: {
		if (fragment_size < sizeof(struct sof_steamaudio_atmosphere_config))
			return -EINVAL;

		struct sof_steamaudio_atmosphere_config *cfg =
			(struct sof_steamaudio_atmosphere_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_ATMOSPHERE;
		cfg->temperature_c = cd->atmosphere.temperature_c;
		cfg->relative_humidity = cd->atmosphere.relative_humidity;
		cfg->pressure_kpa = cd->atmosphere.pressure_kpa;
		cfg->speed_of_sound = cd->atmosphere.speed_of_sound;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cfg->absorption_coefficients[b] = (cd->mute_mask & (1u << STEAMAUDIO_STEP_ATMOSPHERE)) ?
				((b == 0) ? 0.0002f : ((b == 1) ? 0.0017f : 0.0182f)) :
				cd->atmosphere.absorption_coefficients[b];
		}
		cfg->flags = cd->atmosphere.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_DIFFRACTION: {
		if (fragment_size < sizeof(struct sof_steamaudio_diffraction_config))
			return -EINVAL;

		struct sof_steamaudio_diffraction_config *cfg =
			(struct sof_steamaudio_diffraction_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_DIFFRACTION;
		cfg->wedge_angle_rad = cd->diffraction.wedge_angle_rad;
		cfg->deviation_angle_rad = cd->diffraction.deviation_angle_rad;
		cfg->r_source = cd->diffraction.r_source;
		cfg->r_receiver = cd->diffraction.r_receiver;
		cfg->speed_of_sound = cd->diffraction.speed_of_sound;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cfg->transmission[b] = cd->diffraction.transmission[b];
			cfg->diffraction_coeffs[b] = (cd->mute_mask & (1u << STEAMAUDIO_STEP_DIFFRACTION)) ?
				1.0f : cd->diffraction.diffraction_coeffs[b];
			cfg->combined_gains[b] = (cd->mute_mask & (1u << STEAMAUDIO_STEP_DIFFRACTION)) ?
				1.0f : cd->diffraction.combined_gains[b];
		}
		cfg->flags = cd->diffraction.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_PROBEBATCH: {
		if (fragment_size < sizeof(struct sof_steamaudio_probe_batch_config))
			return -EINVAL;

		struct sof_steamaudio_probe_batch_config *cfg =
			(struct sof_steamaudio_probe_batch_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_PROBEBATCH;
		cfg->num_probes = cd->probe_batch.num_probes;
		for (uint32_t i = 0; i < cd->probe_batch.num_probes && i < STEAMAUDIO_MAX_PROBES; i++) {
			cfg->probes[i] = cd->probe_batch.probes[i];
		}
		cfg->num_queries = cd->probe_batch.num_queries;
		for (uint32_t q = 0; q < cd->probe_batch.num_queries && q < STEAMAUDIO_MAX_PROBE_QUERIES; q++) {
			cfg->query_positions[q][0] = cd->probe_batch.query_positions[q][0];
			cfg->query_positions[q][1] = cd->probe_batch.query_positions[q][1];
			cfg->query_positions[q][2] = cd->probe_batch.query_positions[q][2];
			for (int n = 0; n < STEAMAUDIO_MAX_NEIGHBORS; n++) {
				cfg->neighbor_indices[q][n] = cd->probe_batch.neighbor_indices[q][n];
				cfg->neighbor_weights[q][n] = cd->probe_batch.neighbor_weights[q][n];
			}
			for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
				cfg->interpolated_eq[q][b] = (cd->mute_mask & (1u << STEAMAUDIO_STEP_PROBEBATCH)) ?
					1.0f : cd->probe_batch.interpolated_eq[q][b];
			}
		}
		cfg->flags = cd->probe_batch.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_SOFA_HRIR: {
		if (fragment_size < sizeof(struct sof_steamaudio_sofa_hrir_config))
			return -EINVAL;

		struct sof_steamaudio_sofa_hrir_config *cfg =
			(struct sof_steamaudio_sofa_hrir_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_SOFA_HRIR;
		cfg->num_taps = cd->sofa_hrir.num_taps;
		for (uint32_t i = 0; i < STEAMAUDIO_MAX_HRIR_TAPS; i++) {
			cfg->hrir_left[i] = cd->sofa_hrir.hrir_left[i];
			cfg->hrir_right[i] = cd->sofa_hrir.hrir_right[i];
		}
		cfg->direction[0] = cd->sofa_hrir.direction[0];
		cfg->direction[1] = cd->sofa_hrir.direction[1];
		cfg->direction[2] = cd->sofa_hrir.direction[2];
		cfg->spatial_blend = cd->sofa_hrir.spatial_blend;
		cfg->volume = cd->sofa_hrir.volume;
		cfg->flags = cd->sofa_hrir.flags | (cd->sofa_hrir.enabled ? 1 : 0);
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_GRAPH_SEARCH: {
		if (fragment_size < sizeof(struct sof_steamaudio_graph_search_config))
			return -EINVAL;

		struct sof_steamaudio_graph_search_config *cfg =
			(struct sof_steamaudio_graph_search_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_GRAPH_SEARCH;
		cfg->num_nodes = cd->graph_search.num_nodes;
		cfg->start_node = cd->graph_search.start_node;
		cfg->target_node = cd->graph_search.target_node;
		cfg->max_range = cd->graph_search.max_range;

		for (uint16_t i = 0; i < cd->graph_search.num_nodes; i++) {
			cfg->nodes[i].num_edges = cd->graph_search.nodes[i].num_edges;
			for (uint16_t e = 0; e < cd->graph_search.nodes[i].num_edges; e++) {
				cfg->nodes[i].edges[e].node = cd->graph_search.nodes[i].edges[e].node;
				cfg->nodes[i].edges[e].cost = cd->graph_search.nodes[i].edges[e].cost;
			}
			cfg->nodes[i].pos[0] = cd->graph_search.nodes[i].pos[0];
			cfg->nodes[i].pos[1] = cd->graph_search.nodes[i].pos[1];
			cfg->nodes[i].pos[2] = cd->graph_search.nodes[i].pos[2];
		}

		cfg->num_path_nodes = cd->graph_search.num_path_nodes;
		for (uint16_t i = 0; i < cd->graph_search.num_path_nodes; i++) {
			cfg->path_nodes[i] = cd->graph_search.path_nodes[i];
		}
		cfg->total_cost = cd->graph_search.total_cost;
		cfg->path_found = cd->graph_search.path_found ? 1 : 0;
		cfg->enabled = cd->graph_search.enabled ? 1 : 0;
		cfg->flags = cd->graph_search.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_REFLECTION_MIXER: {
		if (fragment_size < sizeof(struct sof_steamaudio_reflection_mixer_config))
			return -EINVAL;

		struct sof_steamaudio_reflection_mixer_config *cfg =
			(struct sof_steamaudio_reflection_mixer_config *)fragment;
		memset(cfg, 0, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_REFLECTION_MIXER;
		cfg->num_sources = cd->reflection_mixer.num_sources;
		cfg->num_channels = cd->reflection_mixer.num_channels;
		cfg->frames = cd->reflection_mixer.frames;
		cfg->flags = cd->reflection_mixer.flags;
		if (cd->reflection_mixer.enabled)
			cfg->flags |= 1;
		else
			cfg->flags &= ~1;

		for (uint32_t s = 0; s < STEAMAUDIO_MAX_MIXER_SOURCES; s++) {
			cfg->source_gains[s] = cd->reflection_mixer.source_gains[s];
		}

		bool muted = (cd->mute_mask & (1u << STEAMAUDIO_STEP_REFLECTION_MIXER)) != 0;
		steamaudio_dsp_reflection_mixer_process(&cd->reflection_mixer, cfg, muted);
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_INSTANCED_MESH: {
		if (fragment_size < sizeof(struct sof_steamaudio_instanced_mesh_config))
			return -EINVAL;

		struct sof_steamaudio_instanced_mesh_config *cfg =
			(struct sof_steamaudio_instanced_mesh_config *)fragment;

		bool muted = (cd->mute_mask & (1u << STEAMAUDIO_STEP_INSTANCED_MESH)) != 0;

		struct dsp_vec3 src = { cfg->ray_origin[0], cfg->ray_origin[1], cfg->ray_origin[2] };
		struct dsp_vec3 lst = { cfg->ray_direction[0], cfg->ray_direction[1], cfg->ray_direction[2] };

		float out_trans[3] = { 0 };
		cfg->out_occlusion = steamaudio_dsp_instanced_mesh_test_occlusion(
			&cd->instanced_mesh, src, lst, out_trans, muted);
		cfg->out_transmission[0] = out_trans[0];
		cfg->out_transmission[1] = out_trans[1];
		cfg->out_transmission[2] = out_trans[2];

		struct dsp_ray ray;
		ray.origin = src;
		ray.direction = (struct dsp_vec3){ cfg->ray_direction[0], cfg->ray_direction[1], cfg->ray_direction[2] };
		ray.min_distance = cfg->min_dist;
		ray.max_distance = cfg->max_dist;

		struct dsp_hit hit;
		cfg->out_has_hit = steamaudio_dsp_instanced_mesh_trace_ray(
			&cd->instanced_mesh, &ray, &hit, muted) ? 1 : 0;

		cfg->comp_type = STEAMAUDIO_PARAM_INSTANCED_MESH;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_RAY_TRACER: {
		if (fragment_size < sizeof(struct sof_steamaudio_ray_tracer_config))
			return -EINVAL;

		struct sof_steamaudio_ray_tracer_config *cfg =
			(struct sof_steamaudio_ray_tracer_config *)fragment;

		bool muted = (cd->mute_mask & (1u << STEAMAUDIO_STEP_RAY_TRACER)) != 0;

		steamaudio_dsp_ray_tracer_simulate_batch(cd, &cd->ray_tracer, cfg, muted);
		cfg->comp_type = STEAMAUDIO_PARAM_RAY_TRACER;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_REVERB_ESTIMATOR: {
		if (fragment_size < sizeof(struct sof_steamaudio_reverb_estimator_config))
			return -EINVAL;

		struct sof_steamaudio_reverb_estimator_config *cfg =
			(struct sof_steamaudio_reverb_estimator_config *)fragment;

		bool muted = (cd->mute_mask & (1u << STEAMAUDIO_STEP_REVERB_ESTIMATOR)) != 0;

		steamaudio_dsp_reverb_estimator_set_config(&cd->reverb_estimator, cfg);
		if (cfg->num_input_paths > 0) {
			uint32_t n = cfg->num_input_paths;
			if (n > STEAMAUDIO_RAY_TRACER_MAX_RAYS)
				n = STEAMAUDIO_RAY_TRACER_MAX_RAYS;
			steamaudio_dsp_reverb_estimator_accumulate_rays(&cd->reverb_estimator, cfg->input_paths, n);
		}
		steamaudio_dsp_reverb_estimator_compute_edc_rt60(&cd->reverb_estimator, muted);
		cfg->results = cd->reverb_estimator.results;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_EARLY_REFLECTIONS: {
		if (fragment_size < sizeof(struct sof_steamaudio_early_reflections_config))
			return -EINVAL;

		struct sof_steamaudio_early_reflections_config *cfg =
			(struct sof_steamaudio_early_reflections_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_EARLY_REFLECTIONS;
		cfg->num_taps = cd->early_reflections.num_taps;
		cfg->sample_rate = cd->early_reflections.sample_rate;
		cfg->speed_of_sound = cd->early_reflections.speed_of_sound;
		cfg->num_channels = cd->early_reflections.num_channels;
		cfg->flags = cd->early_reflections.flags;
		for (uint32_t i = 0; i < cd->early_reflections.num_taps && i < STEAMAUDIO_EARLY_REFLECTIONS_MAX_TAPS; i++) {
			cfg->taps[i] = cd->early_reflections.taps[i];
		}
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_MATERIAL_TRANSMISSION: {
		if (fragment_size < sizeof(struct sof_steamaudio_material_transmission_config))
			return -EINVAL;

		struct sof_steamaudio_material_transmission_config *cfg =
			(struct sof_steamaudio_material_transmission_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_MATERIAL_TRANSMISSION;
		cfg->num_layers = cd->material_transmission.num_layers;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
			cfg->composite_transmission[b] = cd->material_transmission.composite_transmission[b];
		}
		for (uint32_t k = 0; k < cd->material_transmission.num_layers && k < STEAMAUDIO_MAX_MATERIAL_LAYERS; k++) {
			cfg->layers[k].surface_density = cd->material_transmission.layers[k].surface_density;
			cfg->layers[k].thickness = cd->material_transmission.layers[k].thickness;
			cfg->layers[k].preset = cd->material_transmission.layers[k].preset;
			for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++) {
				cfg->layers[k].transmission[b] = cd->material_transmission.layers[k].transmission[b];
			}
		}
		cfg->sample_rate = cd->material_transmission.sample_rate;
		cfg->flags = cd->material_transmission.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_ACOUSTIC_PORTALS: {
		if (fragment_size < sizeof(struct sof_steamaudio_acoustic_portals_config))
			return -EINVAL;

		struct sof_steamaudio_acoustic_portals_config *cfg =
			(struct sof_steamaudio_acoustic_portals_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_ACOUSTIC_PORTALS;
		cfg->num_portals = cd->acoustic_portals.num_portals;
		for (uint32_t i = 0; i < cd->acoustic_portals.num_portals && i < STEAMAUDIO_MAX_PORTALS; i++) {
			cfg->portals[i].center[0] = cd->acoustic_portals.portals[i].center[0];
			cfg->portals[i].center[1] = cd->acoustic_portals.portals[i].center[1];
			cfg->portals[i].center[2] = cd->acoustic_portals.portals[i].center[2];
			cfg->portals[i].normal[0] = cd->acoustic_portals.portals[i].normal[0];
			cfg->portals[i].normal[1] = cd->acoustic_portals.portals[i].normal[1];
			cfg->portals[i].normal[2] = cd->acoustic_portals.portals[i].normal[2];
			cfg->portals[i].dimensions[0] = cd->acoustic_portals.portals[i].dimensions[0];
			cfg->portals[i].dimensions[1] = cd->acoustic_portals.portals[i].dimensions[1];
			cfg->portals[i].area = cd->acoustic_portals.portals[i].area;
			cfg->portals[i].openness = cd->acoustic_portals.portals[i].openness;
			cfg->portals[i].room_id_front = cd->acoustic_portals.portals[i].room_id_front;
			cfg->portals[i].room_id_back = cd->acoustic_portals.portals[i].room_id_back;
			cfg->portals[i].enabled = cd->acoustic_portals.portals[i].enabled;
		}
		cfg->sample_rate = cd->acoustic_portals.sample_rate;
		cfg->flags = cd->acoustic_portals.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_VOLUMETRIC_SOURCE: {
		if (fragment_size < sizeof(struct sof_steamaudio_volumetric_source_config))
			return -EINVAL;

		struct sof_steamaudio_volumetric_source_config *cfg =
			(struct sof_steamaudio_volumetric_source_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_VOLUMETRIC_SOURCE;
		cfg->num_sources = cd->volumetric_source.num_sources;
		for (uint32_t i = 0; i < cd->volumetric_source.num_sources && i < STEAMAUDIO_MAX_VOLUMETRIC_SOURCES; i++) {
			cfg->sources[i].shape_type = cd->volumetric_source.sources[i].shape_type;
			cfg->sources[i].center[0] = cd->volumetric_source.sources[i].center[0];
			cfg->sources[i].center[1] = cd->volumetric_source.sources[i].center[1];
			cfg->sources[i].center[2] = cd->volumetric_source.sources[i].center[2];
			for (int p = 0; p < 4; p++)
				cfg->sources[i].params[p] = cd->volumetric_source.sources[i].params[p];
			cfg->sources[i].energy_distribution = cd->volumetric_source.sources[i].energy_distribution;
			cfg->sources[i].enabled = cd->volumetric_source.sources[i].enabled;
		}
		cfg->listener_pos[0] = cd->volumetric_source.listener_pos[0];
		cfg->listener_pos[1] = cd->volumetric_source.listener_pos[1];
		cfg->listener_pos[2] = cd->volumetric_source.listener_pos[2];
		cfg->listener_ahead[0] = cd->volumetric_source.listener_ahead[0];
		cfg->listener_ahead[1] = cd->volumetric_source.listener_ahead[1];
		cfg->listener_ahead[2] = cd->volumetric_source.listener_ahead[2];
		cfg->listener_up[0] = cd->volumetric_source.listener_up[0];
		cfg->listener_up[1] = cd->volumetric_source.listener_up[1];
		cfg->listener_up[2] = cd->volumetric_source.listener_up[2];
		cfg->speaker_layout = cd->volumetric_source.speaker_layout;
		cfg->sample_rate = cd->volumetric_source.sample_rate;
		cfg->flags = cd->volumetric_source.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_SOURCE_PRIORITIZATION: {
		if (fragment_size < sizeof(struct sof_steamaudio_source_prioritization_config))
			return -EINVAL;

		struct sof_steamaudio_source_prioritization_config *cfg =
			(struct sof_steamaudio_source_prioritization_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_SOURCE_PRIORITIZATION;
		cfg->num_sources = cd->source_prioritization.num_sources;
		cfg->max_voices = cd->source_prioritization.max_voices;
		for (uint32_t i = 0; i < cd->source_prioritization.num_sources && i < STEAMAUDIO_MAX_PRIORITY_SOURCES; i++) {
			cfg->sources[i].source_id = cd->source_prioritization.sources[i].source_id;
			cfg->sources[i].position[0] = cd->source_prioritization.sources[i].position[0];
			cfg->sources[i].position[1] = cd->source_prioritization.sources[i].position[1];
			cfg->sources[i].position[2] = cd->source_prioritization.sources[i].position[2];
			cfg->sources[i].base_priority = cd->source_prioritization.sources[i].base_priority;
			cfg->sources[i].volume = cd->source_prioritization.sources[i].volume;
			cfg->sources[i].direct_fraction = cd->source_prioritization.sources[i].direct_fraction;
			cfg->sources[i].flags = cd->source_prioritization.sources[i].flags;
			cfg->sources[i].enabled = cd->source_prioritization.sources[i].enabled;
		}
		cfg->listener_pos[0] = cd->source_prioritization.listener_pos[0];
		cfg->listener_pos[1] = cd->source_prioritization.listener_pos[1];
		cfg->listener_pos[2] = cd->source_prioritization.listener_pos[2];
		cfg->listener_ahead[0] = cd->source_prioritization.listener_ahead[0];
		cfg->listener_ahead[1] = cd->source_prioritization.listener_ahead[1];
		cfg->listener_ahead[2] = cd->source_prioritization.listener_ahead[2];
		cfg->min_audible_threshold = cd->source_prioritization.min_audible_threshold;
		cfg->distance_reference = cd->source_prioritization.distance_reference;
		cfg->distance_max = cd->source_prioritization.distance_max;
		cfg->fov_attenuation_bias = cd->source_prioritization.fov_attenuation_bias;
		cfg->hysteresis_margin = cd->source_prioritization.hysteresis_margin;
		cfg->sample_rate = cd->source_prioritization.sample_rate;
		cfg->flags = cd->source_prioritization.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_GROUND_REFLECTION: {
		if (fragment_size < sizeof(struct sof_steamaudio_ground_reflection_config))
			return -EINVAL;

		struct sof_steamaudio_ground_reflection_config *cfg =
			(struct sof_steamaudio_ground_reflection_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_GROUND_REFLECTION;
		for (int i = 0; i < 3; i++) {
			cfg->ground_plane_pos[i] = cd->ground_reflection.config.ground_plane_pos[i];
			cfg->ground_plane_normal[i] = cd->ground_reflection.config.ground_plane_normal[i];
			cfg->source_pos[i] = cd->ground_reflection.config.source_pos[i];
			cfg->listener_pos[i] = cd->ground_reflection.config.listener_pos[i];
		}
		cfg->material_preset = cd->ground_reflection.config.material_preset;
		for (int b = 0; b < STEAMAUDIO_NUM_EQ_BANDS; b++)
			cfg->reflection_coeffs[b] = cd->ground_reflection.config.reflection_coeffs[b];
		cfg->sound_speed = cd->ground_reflection.config.sound_speed;
		cfg->sample_rate = cd->ground_reflection.config.sample_rate;
		cfg->flags = cd->ground_reflection.config.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_TRUE_PEAK_LIMITER: {
		if (fragment_size < sizeof(struct sof_steamaudio_limiter_config))
			return -EINVAL;

		struct sof_steamaudio_limiter_config *cfg =
			(struct sof_steamaudio_limiter_config *)fragment;

		cfg->comp_type = STEAMAUDIO_PARAM_TRUE_PEAK_LIMITER;
		cfg->threshold_db = cd->true_peak_limiter.config.threshold_db;
		cfg->ceiling_db = cd->true_peak_limiter.config.ceiling_db;
		cfg->knee_width_db = cd->true_peak_limiter.config.knee_width_db;
		cfg->ratio = cd->true_peak_limiter.config.ratio;
		cfg->attack_time_ms = cd->true_peak_limiter.config.attack_time_ms;
		cfg->release_time_ms = cd->true_peak_limiter.config.release_time_ms;
		cfg->makeup_gain_db = cd->true_peak_limiter.config.makeup_gain_db;
		cfg->sample_rate = cd->true_peak_limiter.config.sample_rate;
		cfg->flags = cd->true_peak_limiter.config.flags;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_ROOM_MODES: {
		if (fragment_size < sizeof(struct sof_steamaudio_room_modes_config))
			return -EINVAL;

		struct sof_steamaudio_room_modes_config *cfg =
			(struct sof_steamaudio_room_modes_config *)fragment;
		memcpy(cfg, &cd->room_modes.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_ROOM_MODES;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_ATMOSPHERIC_TURBULENCE: {
		if (fragment_size < sizeof(struct sof_steamaudio_atmospheric_turbulence_config))
			return -EINVAL;

		struct sof_steamaudio_atmospheric_turbulence_config *cfg =
			(struct sof_steamaudio_atmospheric_turbulence_config *)fragment;
		memcpy(cfg, &cd->atmospheric_turbulence.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_ATMOSPHERIC_TURBULENCE;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_SURFACE_SCATTERING: {
		if (fragment_size < sizeof(struct sof_steamaudio_surface_scattering_config))
			return -EINVAL;

		struct sof_steamaudio_surface_scattering_config *cfg =
			(struct sof_steamaudio_surface_scattering_config *)fragment;
		memcpy(cfg, &cd->surface_scattering.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_SURFACE_SCATTERING;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_SOUND_BARRIER: {
		if (fragment_size < sizeof(struct sof_steamaudio_sound_barrier_config))
			return -EINVAL;

		struct sof_steamaudio_sound_barrier_config *cfg =
			(struct sof_steamaudio_sound_barrier_config *)fragment;
		memcpy(cfg, &cd->sound_barrier.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_SOUND_BARRIER;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_NEAR_FIELD: {
		if (fragment_size < sizeof(struct sof_steamaudio_near_field_config))
			return -EINVAL;

		struct sof_steamaudio_near_field_config *cfg =
			(struct sof_steamaudio_near_field_config *)fragment;
		memcpy(cfg, &cd->near_field.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_NEAR_FIELD;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_NONLINEAR_WAVE: {
		if (fragment_size < sizeof(struct sof_steamaudio_nonlinear_wave_config))
			return -EINVAL;

		struct sof_steamaudio_nonlinear_wave_config *cfg =
			(struct sof_steamaudio_nonlinear_wave_config *)fragment;
		memcpy(cfg, &cd->nonlinear_wave.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_NONLINEAR_WAVE;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_BATTLE_BLEED: {
		if (fragment_size < sizeof(struct sof_steamaudio_battle_bleed_config))
			return -EINVAL;

		struct sof_steamaudio_battle_bleed_config *cfg =
			(struct sof_steamaudio_battle_bleed_config *)fragment;
		memcpy(cfg, &cd->battle_bleed.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_BATTLE_BLEED;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_VOICE_LOD: {
		if (fragment_size < sizeof(struct sof_steamaudio_voice_lod_config))
			return -EINVAL;

		struct sof_steamaudio_voice_lod_config *cfg =
			(struct sof_steamaudio_voice_lod_config *)fragment;
		memcpy(cfg, &cd->voice_lod.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_VOICE_LOD;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	case STEAMAUDIO_PARAM_SCENE_UPMIX: {
		if (fragment_size < sizeof(struct sof_steamaudio_upmix_config))
			return -EINVAL;

		struct sof_steamaudio_upmix_config *cfg =
			(struct sof_steamaudio_upmix_config *)fragment;
		memcpy(cfg, &cd->upmix.config, sizeof(*cfg));
		cfg->comp_type = STEAMAUDIO_PARAM_SCENE_UPMIX;
		if (data_offset_size)
			*data_offset_size = sizeof(*cfg);
		return 0;
	}

	default:
		return -EINVAL;
	}
}
