// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware (SOF) Project
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/**
 * \file audio/pipeline/static_pipeline_uac2.c
 * \brief USB Audio Class 2.0 (UAC2) Bridge & Control Operations for Static Pipelines
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Provides translation between USB Audio Class 2.0 (UAC2) host requests (volume,
 * mute switch, streaming terminal triggers) and Sound Open Firmware static
 * topology components and kcontrols.
 */

#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/common.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(static_pipeline_uac2, CONFIG_SOF_LOG_LEVEL);

/* Standard 0 dB gain constant for SOF volume representation (Q1.16 format) */
#define SOF_VOL_ZERO_DB 65536

/**
 * \brief Convert USB Audio Class 2.0 8.8 fixed-point dB volume to linear SOF volume.
 *
 * UAC2 expresses volume in 1/256 dB steps (e.g. 0x0000 = 0 dB, -90 dB = 0xa600).
 * This helper translates UAC2 dB volume into SOF 16-bit linear volume using
 * an integer 6 dB per bit shift approximation:
 *   - >= 0 dB maps to SOF_VOL_ZERO_DB (unity gain)
 *   - <= -90 dB maps to 0 (digital silence)
 *   - Values between -90 dB and 0 dB are scaled proportionally.
 *
 * \param[in] volume USB Audio Class 2.0 8.8 fixed-point dB volume value.
 * \return 16-bit linear volume value (0 to SOF_VOL_ZERO_DB).
 */
static int32_t uac2_to_sof_volume(int16_t volume)
{
	/* Treat volume below -90 dB as complete digital silence */
	if (volume <= -90 * 256)
		return 0;

	/* Clip volume at or above 0 dB to unity gain */
	if (volume >= 0)
		return SOF_VOL_ZERO_DB;

	/* Convert negative dB (scaled by 256) to tenths of dB: 1 dB = ~6 dB per bit */
	int32_t db_x10 = (int32_t)(-volume) * 10 / 256;
	int shift = db_x10 / 60;
	if (shift >= 31)
		return 0;

	/* Linear interpolation between 6 dB bit shifts */
	int rem = db_x10 % 60;
	uint64_t v = (uint64_t)SOF_VOL_ZERO_DB >> shift;
	v = (v * (60 - rem)) / 60;
	return (int32_t)v;
}

/**
 * \brief Dispatch USB Audio Class 2.0 Feature Unit control requests to matching kcontrols.
 *
 * Maps incoming USB Audio Class 2 (UAC2) volume or mute requests to the corresponding
 * static kcontrol bound to that UAC2 entity ID.
 *
 * \param[in] entity_id UAC2 Feature Unit Entity ID.
 * \param[in] channel Audio channel index (0 = master/all).
 * \param[in] val Value from USB request (8.8 fixed-point dB volume or boolean mute).
 * \param[in] is_volume True for volume command, false for mute switch command.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_set_by_uac2(uint8_t entity_id, uint8_t channel, int32_t val, bool is_volume)
{
	const struct sof_static_topology *topo = sof_static_topology_get();

	if (!topo) {
		LOG_ERR("sof_static_kcontrol_set_by_uac2: no active topology");
		return -ENODEV;
	}

	/* Search controls in active topology for matching UAC2 entity ID */
	for (size_t i = 0; i < topo->num_controls; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];

		if (ctl->uac2_entity_id == entity_id) {
			if (is_volume && ctl->type == SOF_STATIC_CTRL_VOLUME) {
				/* Translate UAC2 8.8 dB volume to linear SOF volume */
				int32_t sof_vol = uac2_to_sof_volume((int16_t)val);
				return sof_static_kcontrol_set(ctl->id, sof_vol);
			} else if (!is_volume && ctl->type == SOF_STATIC_CTRL_SWITCH) {
				/* In UAC2: val=1 means MUTED, so enabled switch state is 0 */
				return sof_static_kcontrol_set(ctl->id, val ? 0 : 1);
			}
		}
	}

	LOG_ERR("sof_static_kcontrol_set_by_uac2: no control found for UAC2 entity %u", entity_id);
	return -ENOENT;
}

/**
 * \brief Query kcontrol value formatted for USB Audio Class 2.0 response.
 *
 * \param[in] entity_id UAC2 Feature Unit Entity ID.
 * \param[in] channel Audio channel index.
 * \param[out] val Pointer to store retrieved value.
 * \param[in] is_volume True for volume query, false for mute query.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_kcontrol_get_by_uac2(uint8_t entity_id, uint8_t channel, int32_t *val, bool is_volume)
{
	const struct sof_static_topology *topo = sof_static_topology_get();

	if (!topo || !val) {
		LOG_ERR("sof_static_kcontrol_get_by_uac2: invalid topology or null val pointer");
		return -EINVAL;
	}

	/* Search controls in active topology for matching UAC2 entity ID */
	for (size_t i = 0; i < topo->num_controls; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];

		if (ctl->uac2_entity_id == entity_id) {
			int32_t ctl_val = 0;
			int ret = sof_static_kcontrol_get(ctl->id, &ctl_val);
			if (ret < 0) {
				LOG_ERR("sof_static_kcontrol_get_by_uac2: failed to get ctl %u", ctl->id);
				return ret;
			}

			if (is_volume && ctl->type == SOF_STATIC_CTRL_VOLUME) {
				*val = ctl_val;
				return 0;
			} else if (!is_volume && ctl->type == SOF_STATIC_CTRL_SWITCH) {
				/* In UAC2: mute is 1 if disabled (0), 0 if enabled (1) */
				*val = (ctl_val == 0) ? 1 : 0;
				return 0;
			}
		}
	}

	LOG_ERR("sof_static_kcontrol_get_by_uac2: no control found for UAC2 entity %u", entity_id);
	return -ENOENT;
}

/**
 * \brief Trigger pipeline start or stop associated with a UAC2 terminal ID.
 *
 * Finds the host streaming component bound to the given USB terminal ID
 * and delegates to sof_static_pipeline_start() or sof_static_pipeline_stop().
 *
 * \param[in] terminal_id Bound UAC2 Terminal Entity ID.
 * \param[in] start True to start pipeline, false to pause/stop.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipeline_trigger_by_uac2_term(uint8_t terminal_id, bool start)
{
	const struct sof_static_topology *topo = sof_static_topology_get();

	if (!topo) {
		LOG_ERR("sof_static_pipeline_trigger_by_uac2_term: no active topology");
		return -ENODEV;
	}

	/* Find host component matching terminal_id */
	for (size_t i = 0; i < topo->num_comps; i++) {
		const struct sof_static_comp *cdesc = &topo->comps[i];

		if (cdesc->type == SOF_STATIC_COMP_HOST && cdesc->ep.usb.terminal_id == terminal_id) {
			/* Delegate directly to modular start/stop routines */
			int ret = start ? sof_static_pipeline_start(cdesc->pipeline_id) :
					  sof_static_pipeline_stop(cdesc->pipeline_id);

			if (ret == 0)
				LOG_INF("Pipeline %u %s via UAC2 terminal %u", cdesc->pipeline_id,
					start ? "STARTED" : "STOPPED", terminal_id);
			else
				LOG_ERR("Pipeline %u %s via UAC2 terminal %u failed: %d",
					cdesc->pipeline_id, start ? "start" : "stop", terminal_id, ret);

			return ret;
		}
	}

	LOG_ERR("sof_static_pipeline_trigger_by_uac2_term: no host component found for terminal %u", terminal_id);
	return -ENOENT;
}
