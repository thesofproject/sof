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
#include <rtos/userspace_helper.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_USBD_AUDIO2_CLASS) && defined(CONFIG_COMP_USB_AUDIO)
#include <sof/audio/usb_audio.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/class/usbd_uac2.h>
#endif

LOG_MODULE_REGISTER(static_pipeline_uac2, CONFIG_SOF_LOG_LEVEL);

/* Standard 0 dB gain constant for SOF volume representation (Q1.31 format) */
#define SOF_VOL_ZERO_DB INT32_MAX

#ifndef CONFIG_STATIC_PIPELINE_MAX_CONTROLS
#define CONFIG_STATIC_PIPELINE_MAX_CONTROLS 64
#endif

/*
 * Cache structure storing raw UAC2 control values (signed 8.8 dB for volume,
 * 0/1 for mute) per UAC2 entity and channel.
 * Channel index 0 corresponds to Master; indices 1..SOF_IPC_MAX_CHANNELS correspond
 * to individual audio channels.
 */
struct uac2_control_cache {
	uint8_t entity_id;
	bool is_volume;
	bool valid;
	int32_t val[SOF_IPC_MAX_CHANNELS + 1];
};

static APP_SYSUSER_BSS struct uac2_control_cache s_uac2_cache[CONFIG_STATIC_PIPELINE_MAX_CONTROLS];

/**
 * \brief Look up or allocate a cached control state entry for a UAC2 entity.
 * \param[in] entity_id UAC2 Feature Unit Entity ID.
 * \param[in] is_volume True for volume control, false for mute switch.
 * \return Pointer to cache entry or NULL if table is full.
 */
static struct uac2_control_cache *get_uac2_cache(uint8_t entity_id, bool is_volume)
{
	/* Look for existing cache entry matching entity and control type */
	for (size_t i = 0; i < CONFIG_STATIC_PIPELINE_MAX_CONTROLS; i++) {
		if (s_uac2_cache[i].valid &&
		    s_uac2_cache[i].entity_id == entity_id &&
		    s_uac2_cache[i].is_volume == is_volume)
			return &s_uac2_cache[i];
	}

	/* Allocate a free cache slot if not already present */
	for (size_t i = 0; i < CONFIG_STATIC_PIPELINE_MAX_CONTROLS; i++) {
		if (!s_uac2_cache[i].valid) {
			s_uac2_cache[i].entity_id = entity_id;
			s_uac2_cache[i].is_volume = is_volume;
			s_uac2_cache[i].valid = true;
			/* Default: volume 0 dB = 0x0000, mute = 0 (unmuted) */
			memset(s_uac2_cache[i].val, 0, sizeof(s_uac2_cache[i].val));
			return &s_uac2_cache[i];
		}
	}

	return NULL;
}

/**
 * \brief Convert USB Audio Class 2.0 8.8 fixed-point dB volume to linear SOF volume.
 *
 * UAC2 expresses volume in 1/256 dB steps (e.g. 0x0000 = 0 dB, -90 dB = 0xa600).
 * This helper translates UAC2 dB volume into SOF linear volume using
 * an integer 6 dB per bit shift approximation:
 *   - >= 0 dB maps to zero_db (unity gain, e.g. ctl->max or SOF_VOL_ZERO_DB)
 *   - <= -90 dB maps to 0 (digital silence)
 *   - Values between -90 dB and 0 dB are scaled proportionally.
 *
 * \param[in] volume USB Audio Class 2.0 8.8 fixed-point dB volume value.
 * \param[in] zero_db Linear unity gain value for the target control.
 * \return Linear volume value (0 to zero_db).
 */
static int32_t uac2_to_sof_volume(int16_t volume, int32_t zero_db)
{
	if (zero_db <= 0)
		zero_db = SOF_VOL_ZERO_DB;

	/* Treat volume below -90 dB as complete digital silence */
	if (volume <= -90 * 256)
		return 0;

	/* Clip volume at or above 0 dB to unity gain */
	if (volume >= 0)
		return zero_db;

	/* Convert negative dB (scaled by 256) to tenths of dB: 1 dB = ~6 dB per bit */
	int32_t db_x10 = (int32_t)(-volume) * 10 / 256;
	int shift = db_x10 / 60;
	if (shift >= 31)
		return 0;

	/*
	 * Monotonically interpolate between 6 dB bit shifts.
	 * v is the gain at the current 6 dB boundary, and v_next is the gain at the
	 * next 6 dB boundary (v >> 1). We interpolate smoothly between v and v_next.
	 */
	int rem = db_x10 % 60;
	uint64_t v = (uint64_t)zero_db >> shift;
	uint64_t v_next = v >> 1;
	uint64_t val = v_next + (((v - v_next) * (60 - rem)) / 60);

	return (int32_t)val;
}

/**
 * \brief Dispatch USB Audio Class 2.0 Feature Unit control requests to matching kcontrols.
 *
 * Maps incoming USB Audio Class 2 (UAC2) volume or mute requests to the corresponding
 * static kcontrol bound to that UAC2 entity ID, supporting channel 0 (master) and
 * individual channels.
 *
 * \param[in] entity_id UAC2 Feature Unit Entity ID.
 * \param[in] channel Audio channel index (0 = master/all, 1 = first channel, etc.).
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

	/* Search controls in active topology for matching UAC2 entity ID and control type */
	for (size_t i = 0; i < topo->num_controls; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];
		bool type_match = (is_volume && ctl->type == SOF_STATIC_CTRL_VOLUME) ||
				  (!is_volume && ctl->type == SOF_STATIC_CTRL_SWITCH);

		if (ctl->uac2_entity_id == entity_id && type_match) {
			/* Validate channel bounds */
			if (channel > ctl->channels && channel != 0) {
				LOG_ERR("sof_static_kcontrol_set_by_uac2: channel %u exceeds ctl channels %u",
					channel, ctl->channels);
				return -EINVAL;
			}

			struct uac2_control_cache *cache = get_uac2_cache(entity_id, is_volume);

			if (is_volume) {
				/* Translate UAC2 8.8 dB volume to linear SOF volume */
				int32_t zero_db = (ctl->max > 0) ? ctl->max : SOF_VOL_ZERO_DB;
				int32_t sof_vol = uac2_to_sof_volume((int16_t)val, zero_db);
				int ret;

				if (channel == 0) {
					/* Master channel: apply volume change to all channels first */
					ret = sof_static_kcontrol_set(ctl->id, sof_vol);
					if (ret < 0)
						return ret;

					/* Update cache only after the hardware/DSP operation succeeds */
					if (cache) {
						for (uint32_t ch = 0; ch <= ctl->channels && ch <= SOF_IPC_MAX_CHANNELS; ch++)
							cache->val[ch] = val;
					}
					return 0;
				}

				/* Individual channel: apply volume change to specific channel first */
				ret = sof_static_kcontrol_set_chan(ctl->id, channel - 1, sof_vol);
				if (ret < 0)
					return ret;

				/* Update channel cache only after the hardware/DSP operation succeeds */
				if (cache && channel <= SOF_IPC_MAX_CHANNELS)
					cache->val[channel] = val;

				return 0;
			} else {
				/* In UAC2: val=1 means MUTED, so enabled switch state is 0 */
				int32_t sof_sw = val ? 0 : 1;
				int ret;

				if (channel == 0) {
					/* Master channel: apply mute switch change to all channels first */
					ret = sof_static_kcontrol_set(ctl->id, sof_sw);
					if (ret < 0)
						return ret;

					/* Update cache only after the hardware/DSP operation succeeds */
					if (cache) {
						for (uint32_t ch = 0; ch <= ctl->channels && ch <= SOF_IPC_MAX_CHANNELS; ch++)
							cache->val[ch] = val;
					}
					return 0;
				}

				/* Individual channel: apply mute switch change to specific channel first */
				ret = sof_static_kcontrol_set_chan(ctl->id, channel - 1, sof_sw);
				if (ret < 0)
					return ret;

				/* Update channel cache only after the hardware/DSP operation succeeds */
				if (cache && channel <= SOF_IPC_MAX_CHANNELS)
					cache->val[channel] = val;

				return 0;
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
 * \param[in] channel Audio channel index (0 = master, 1 = first channel, etc.).
 * \param[out] val Pointer to store retrieved UAC2 value.
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

	/* Search controls in active topology for matching UAC2 entity ID and control type */
	for (size_t i = 0; i < topo->num_controls; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];
		bool type_match = (is_volume && ctl->type == SOF_STATIC_CTRL_VOLUME) ||
				  (!is_volume && ctl->type == SOF_STATIC_CTRL_SWITCH);

		if (ctl->uac2_entity_id == entity_id && type_match) {
			/* Validate channel bounds */
			if (channel > ctl->channels && channel != 0) {
				LOG_ERR("sof_static_kcontrol_get_by_uac2: channel %u exceeds ctl channels %u",
					channel, ctl->channels);
				return -EINVAL;
			}

			/* Retrieve cached UAC2 value (exact 8.8 dB or mute state) */
			struct uac2_control_cache *cache = get_uac2_cache(entity_id, is_volume);
			if (cache && channel <= SOF_IPC_MAX_CHANNELS) {
				*val = cache->val[channel];
				return 0;
			}

			/* Default if uninitialized: 0 dB for volume, 0 (unmuted) for switch */
			*val = 0;
			return 0;
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

#if defined(CONFIG_USBD_AUDIO2_CLASS) && defined(CONFIG_COMP_USB_AUDIO)

#define UAC2_BUF_BLOCK_SIZE 256

#if defined(CONFIG_NOCACHE_MEMORY)
K_MEM_SLAB_DEFINE_IN_SECT_STATIC(uac2_rx_slab, __nocache, UAC2_BUF_BLOCK_SIZE, 32, 64);
K_MEM_SLAB_DEFINE_IN_SECT_STATIC(uac2_tx_slab, __nocache, UAC2_BUF_BLOCK_SIZE, 64, 64);
#else
K_MEM_SLAB_DEFINE_STATIC(uac2_rx_slab, UAC2_BUF_BLOCK_SIZE, 32, 64);
K_MEM_SLAB_DEFINE_STATIC(uac2_tx_slab, UAC2_BUF_BLOCK_SIZE, 64, 64);
#endif

static uint32_t s_uac2_sample_rate = 48000;
static uint32_t s_uac2_cap_frame_sz = 4;
static uint8_t s_uac2_capture_term_id;
static bool s_uac2_capture_active;
static bool s_uac2_microframes;

static void sof_uac2_send_capture_pending(const struct device *dev)
{
	uint32_t rate;
	uint32_t frame_bytes;

	if (!s_uac2_capture_active || !s_uac2_capture_term_id || !dev)
		return;

	rate = s_uac2_sample_rate ? s_uac2_sample_rate : 48000;
	if (s_uac2_microframes)
		frame_bytes = (rate / 8000) * s_uac2_cap_frame_sz;
	else
		frame_bytes = (rate / 1000) * s_uac2_cap_frame_sz;

	if (frame_bytes == 0 || frame_bytes > UAC2_BUF_BLOCK_SIZE)
		frame_bytes = s_uac2_microframes ? (6 * s_uac2_cap_frame_sz) :
						   (48 * s_uac2_cap_frame_sz);

	for (int iter = 0; iter < 2; iter++) {
		void *buf = NULL;
		int ret;

		if (k_mem_slab_alloc(&uac2_tx_slab, &buf, K_NO_WAIT) != 0)
			break;

		if (!usb_audio_peek_capture_data(buf, frame_bytes)) {
			k_mem_slab_free(&uac2_tx_slab, buf);
			break;
		}

		ret = usbd_uac2_send(dev, s_uac2_capture_term_id, buf, (uint16_t)frame_bytes);
		if (ret == 0) {
			usb_audio_consume_capture_data(frame_bytes);
		} else {
			k_mem_slab_free(&uac2_tx_slab, buf);
			break;
		}
	}
}

static void sof_uac2_sof_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (s_uac2_capture_active && dev)
		sof_uac2_send_capture_pending(dev);
}

static void sof_uac2_terminal_update_cb(const struct device *dev, uint8_t terminal,
					bool enabled, bool microframes, void *user_data)
{
	const struct sof_static_topology *topo = sof_static_topology_get();

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	s_uac2_microframes = microframes;

	if (topo) {
		for (size_t i = 0; i < topo->num_comps; i++) {
			const struct sof_static_comp *cdesc = &topo->comps[i];

			if (cdesc->type == SOF_STATIC_COMP_HOST &&
			    cdesc->ep.usb.terminal_id == terminal &&
			    cdesc->direction == SOF_IPC_STREAM_CAPTURE) {
				uint32_t ch = cdesc->caps.max_channels ?
					      cdesc->caps.max_channels : 2;
				uint32_t bps = (cdesc->caps.default_fmt == SOF_IPC_FRAME_S16_LE) ?
					       2 : 4;

				s_uac2_cap_frame_sz = ch * bps;
				s_uac2_capture_term_id = terminal;
				s_uac2_capture_active = enabled;
				break;
			}
		}
	}

	sof_static_pipeline_trigger_by_uac2_term(terminal, enabled);
}

static void *sof_uac2_get_recv_buf(const struct device *dev, uint8_t terminal,
				   uint16_t size, void *user_data)
{
	void *buf = NULL;

	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	if (size > UAC2_BUF_BLOCK_SIZE)
		return NULL;

	if (k_mem_slab_alloc(&uac2_rx_slab, &buf, K_NO_WAIT) != 0)
		return NULL;

	return buf;
}

static void sof_uac2_data_recv_cb(const struct device *dev, uint8_t terminal,
				  void *buf, uint16_t size, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	if (buf && size > 0)
		usb_audio_feed_playback_data(buf, size);

	if (buf)
		k_mem_slab_free(&uac2_rx_slab, buf);
}

static void sof_uac2_buf_release_cb(const struct device *dev, uint8_t terminal,
				    void *buf, void *user_data)
{
	ARG_UNUSED(user_data);

	if (buf)
		k_mem_slab_free(&uac2_tx_slab, buf);

	if (s_uac2_capture_active && terminal == s_uac2_capture_term_id && dev)
		sof_uac2_send_capture_pending(dev);
}

static uint32_t sof_uac2_feedback_cb(const struct device *dev, uint8_t terminal,
				     void *user_data)
{
	uint32_t rate = s_uac2_sample_rate ? s_uac2_sample_rate : 48000;

	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	if (s_uac2_microframes)
		return (uint32_t)(((uint64_t)rate << 16) / 8000);

	return (uint32_t)(((uint64_t)rate << 14) / 1000);
}

static uint32_t sof_uac2_get_sample_rate(const struct device *dev, uint8_t clock_id,
					 void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(clock_id);
	ARG_UNUSED(user_data);

	if (!s_uac2_sample_rate) {
		uint32_t topo_rate = sof_static_get_sample_rate();

		s_uac2_sample_rate = topo_rate ? topo_rate : 48000;
	}

	return s_uac2_sample_rate;
}

static int sof_uac2_set_sample_rate(const struct device *dev, uint8_t clock_id,
				    uint32_t rate, void *user_data)
{
	const struct sof_static_topology *topo = sof_static_topology_get();

	ARG_UNUSED(dev);
	ARG_UNUSED(clock_id);
	ARG_UNUSED(user_data);

	s_uac2_sample_rate = rate;

	if (topo) {
		for (size_t i = 0; i < topo->num_pipelines; i++) {
			uint32_t pid = topo->pipelines[i].pipeline_id;
			struct pipeline *pipe = sof_static_pipeline_get(pid);

			if (pipe && pipe->status != COMP_STATE_ACTIVE &&
			    sof_static_pipeline_get_sample_rate(pid) != rate)
				sof_static_pipeline_set_sample_rate(pid, rate);
		}
	}

	usb_audio_set_playback_rate(rate);
	usb_audio_set_capture_rate(rate);
	return 0;
}

static int sof_uac2_set_feature_mute(const struct device *dev, uint8_t entity_id,
				     uint8_t channel, bool mute, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	return sof_static_kcontrol_set_by_uac2(entity_id, channel, mute ? 1 : 0, false);
}

static int sof_uac2_get_feature_mute(const struct device *dev, uint8_t entity_id,
				     uint8_t channel, bool *mute, void *user_data)
{
	int32_t val = 0;
	int ret;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (!mute)
		return -EINVAL;

	ret = sof_static_kcontrol_get_by_uac2(entity_id, channel, &val, false);
	if (ret == 0)
		*mute = (val != 0);

	return ret;
}

static int sof_uac2_set_feature_volume(const struct device *dev, uint8_t entity_id,
				       uint8_t channel, int16_t volume, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	return sof_static_kcontrol_set_by_uac2(entity_id, channel, volume, true);
}

static int sof_uac2_get_feature_volume(const struct device *dev, uint8_t entity_id,
				       uint8_t channel, int16_t *volume, void *user_data)
{
	int32_t val = 0;
	int ret;

	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (!volume)
		return -EINVAL;

	ret = sof_static_kcontrol_get_by_uac2(entity_id, channel, &val, true);
	if (ret == 0)
		*volume = (int16_t)val;

	return ret;
}

static const struct uac2_ops s_sof_static_uac2_ops = {
	.sof_cb = sof_uac2_sof_cb,
	.terminal_update_cb = sof_uac2_terminal_update_cb,
	.get_recv_buf = sof_uac2_get_recv_buf,
	.data_recv_cb = sof_uac2_data_recv_cb,
	.buf_release_cb = sof_uac2_buf_release_cb,
	.feedback_cb = sof_uac2_feedback_cb,
	.get_sample_rate = sof_uac2_get_sample_rate,
	.set_sample_rate = sof_uac2_set_sample_rate,
	.set_feature_mute = sof_uac2_set_feature_mute,
	.get_feature_mute = sof_uac2_get_feature_mute,
	.set_feature_volume = sof_uac2_set_feature_volume,
	.get_feature_volume = sof_uac2_get_feature_volume,
};

const struct uac2_ops *sof_static_uac2_get_ops(void)
{
	return &s_sof_static_uac2_ops;
}

#endif /* CONFIG_USBD_AUDIO2_CLASS && CONFIG_COMP_USB_AUDIO */
