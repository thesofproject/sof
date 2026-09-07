// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/usb_audio.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/device.h>

LOG_MODULE_REGISTER(sof_static_pipeline, CONFIG_SOF_LOG_LEVEL);

#define PLAYBACK_FU_ID       UAC2_ENTITY_ID(DT_NODELABEL(i2s_fu))
#define PLAYBACK_EQ_FU_ID    UAC2_ENTITY_ID(DT_NODELABEL(pb_eq_fu))
#define PLAYBACK_DRC_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(pb_drc_fu))
#define CAPTURE_TDFB_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(cap_tdfb_fu))
#define CAPTURE_EQ_FU_ID     UAC2_ENTITY_ID(DT_NODELABEL(cap_eq_fu))
#define CAPTURE_FU_ID        UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_fu))
#define PLAYBACK_TERM_ID     UAC2_ENTITY_ID(DT_NODELABEL(i2s_out_terminal))
#define CAPTURE_TERM_ID      UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_terminal))

K_MEM_SLAB_DEFINE_STATIC(uac2_rx_slab, 1024, 16, 64);
K_MEM_SLAB_DEFINE_STATIC(uac2_tx_slab, 1024, 16, 64);

static struct sof_static_pipeline_status g_status = {
	.playback_active = false,
	.capture_active = false,
	.sample_rate = 48000,
	.active_interface = SOF_AUDIO_IF_I2S,
	.clock_mode = SOF_CLOCK_SLAVE,
	.playback_volume = 0,
	.playback_mute = false,
	.capture_volume = 0,
	.capture_mute = false,
	.eq_playback_bypassed = false,
	.drc_playback_bypassed = false,
	.tdfb_capture_bypassed = false,
	.eq_capture_bypassed = false,
};

static uint32_t s_sof_diag_cnt;

/* UAC2 Class Callbacks */
void sof_uac2_sof_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (g_status.playback_active) {
		s_sof_diag_cnt++;
		if (s_sof_diag_cnt % 1000 == 0) {
			LOG_INF("[SOF UAC2] Playback SOFs: %u, Free RX: %u",
				s_sof_diag_cnt, k_mem_slab_num_free_get(&uac2_rx_slab));
		}
	} else {
		s_sof_diag_cnt = 0;
	}

	if (g_status.capture_active && dev) {
		uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;
		uint32_t frame_bytes = (rate / 1000) * 4; /* 2ch 16-bit */
		if (frame_bytes == 0 || frame_bytes > 512) {
			frame_bytes = 192;
		}

		void *buf = NULL;
		if (k_mem_slab_alloc(&uac2_tx_slab, &buf, K_NO_WAIT) == 0) {
			usb_audio_fetch_capture_data(buf, frame_bytes);
			if (g_status.capture_mute) {
				memset(buf, 0, frame_bytes);
			}
			if (usbd_uac2_send(dev, CAPTURE_TERM_ID, buf, frame_bytes) < 0) {
				k_mem_slab_free(&uac2_tx_slab, buf);
			}
		}
	}
}

void sof_uac2_terminal_update_cb(const struct device *dev, uint8_t terminal,
				 bool enabled, bool microframes, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(microframes);
	ARG_UNUSED(user_data);

	LOG_INF("[UAC2 Terminal] update: terminal %u, enabled %d (PB expected %u, CAP expected %u)",
		terminal, enabled, PLAYBACK_TERM_ID, CAPTURE_TERM_ID);

	if (terminal == PLAYBACK_TERM_ID) {
		g_status.playback_active = enabled;
		sof_static_pipeline_trigger_by_uac2_term(terminal, enabled);
	} else if (terminal == CAPTURE_TERM_ID) {
		g_status.capture_active = enabled;
		sof_static_pipeline_trigger_by_uac2_term(terminal, enabled);
	}
}

void *sof_uac2_get_recv_buf(const struct device *dev, uint8_t terminal,
			    uint16_t size, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	void *buf = NULL;
	if (k_mem_slab_alloc(&uac2_rx_slab, &buf, K_NO_WAIT) != 0) {
		return NULL;
	}
	return buf;
}

void sof_uac2_data_recv_cb(const struct device *dev, uint8_t terminal,
			   void *buf, uint16_t size, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	static uint32_t s_rx_pkt_cnt;
	s_rx_pkt_cnt++;
	if (s_rx_pkt_cnt == 1 || s_rx_pkt_cnt % 1000 == 0) {
		LOG_INF("[SOF UAC2] Received playback packet #%u, size %u bytes", s_rx_pkt_cnt, size);
	}

	if (buf && size > 0) {
		usb_audio_feed_playback_data(buf, size);
	}
	if (buf) {
		k_mem_slab_free(&uac2_rx_slab, buf);
	}
}

void sof_uac2_buf_release(const struct device *dev, uint8_t terminal,
			  void *buf, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	if (buf) {
		k_mem_slab_free(&uac2_tx_slab, buf);
	}
}

uint32_t sof_uac2_get_sample_rate(const struct device *dev, uint8_t clock_id,
				  void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(clock_id);
	ARG_UNUSED(user_data);

	return g_status.sample_rate ? g_status.sample_rate : 48000;
}

int sof_uac2_set_sample_rate(const struct device *dev, uint8_t clock_id,
			     uint32_t rate, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(clock_id);
	ARG_UNUSED(user_data);

	LOG_INF("Host set sample rate: %u Hz", rate);
	g_status.sample_rate = rate;
	sof_static_set_sample_rate(rate);
	usb_audio_set_playback_rate(rate);
	usb_audio_set_capture_rate(rate);
	return 0;
}

uint32_t sof_uac2_feedback_cb(const struct device *dev, uint8_t terminal,
			      void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;
	/* Q16.16 feedback format */
	return ((rate / 1000) << 14);
}

int sof_uac2_set_feature_mute(const struct device *dev, uint8_t entity_id,
			      uint8_t channel, bool mute, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	ARG_UNUSED(user_data);

	LOG_INF("Host set mute/switch: entity=%u, ch=%u, mute=%d", entity_id, channel, mute);
	if (entity_id == PLAYBACK_FU_ID) {
		g_status.playback_mute = mute;
	} else if (entity_id == PLAYBACK_EQ_FU_ID) {
		g_status.eq_playback_bypassed = mute;
	} else if (entity_id == PLAYBACK_DRC_FU_ID) {
		g_status.drc_playback_bypassed = mute;
	} else if (entity_id == CAPTURE_TDFB_FU_ID) {
		g_status.tdfb_capture_bypassed = mute;
	} else if (entity_id == CAPTURE_EQ_FU_ID) {
		g_status.eq_capture_bypassed = mute;
	} else if (entity_id == CAPTURE_FU_ID) {
		g_status.capture_mute = mute;
	}

	return sof_static_kcontrol_set_by_uac2(entity_id, channel, mute ? 1 : 0, false);
}

int sof_uac2_get_feature_mute(const struct device *dev, uint8_t entity_id,
			      uint8_t channel, bool *mute, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	ARG_UNUSED(user_data);

	if (!mute) {
		return -EINVAL;
	}

	int32_t val = 0;
	int ret = sof_static_kcontrol_get_by_uac2(entity_id, channel, &val, false);
	if (ret == 0) {
		*mute = (val != 0);
	} else {
		*mute = false;
	}
	return 0;
}

int sof_uac2_set_feature_volume(const struct device *dev, uint8_t entity_id,
				uint8_t channel, int16_t volume, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	ARG_UNUSED(user_data);

	LOG_INF("Host set volume: entity=%u, ch=%u, vol=%d (0x%04x, %d dB)",
		entity_id, channel, volume, (uint16_t)volume, volume / 256);

	if (entity_id == PLAYBACK_FU_ID) {
		g_status.playback_volume = volume;
	} else if (entity_id == CAPTURE_FU_ID) {
		g_status.capture_volume = volume;
	}

	return sof_static_kcontrol_set_by_uac2(entity_id, channel, volume, true);
}

int sof_uac2_get_feature_volume(const struct device *dev, uint8_t entity_id,
				uint8_t channel, int16_t *volume, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	ARG_UNUSED(user_data);

	if (!volume) {
		return -EINVAL;
	}
	if (entity_id == PLAYBACK_FU_ID) {
		*volume = g_status.playback_volume;
	} else if (entity_id == CAPTURE_FU_ID) {
		*volume = g_status.capture_volume;
	} else {
		*volume = 0;
	}
	return 0;
}

static const struct uac2_ops g_uac2_ops = {
	.sof_cb = sof_uac2_sof_cb,
	.terminal_update_cb = sof_uac2_terminal_update_cb,
	.get_recv_buf = sof_uac2_get_recv_buf,
	.data_recv_cb = sof_uac2_data_recv_cb,
	.buf_release_cb = sof_uac2_buf_release,
	.feedback_cb = sof_uac2_feedback_cb,
	.get_sample_rate = sof_uac2_get_sample_rate,
	.set_sample_rate = sof_uac2_set_sample_rate,
	.set_feature_mute = sof_uac2_set_feature_mute,
	.get_feature_mute = sof_uac2_get_feature_mute,
	.set_feature_volume = sof_uac2_set_feature_volume,
	.get_feature_volume = sof_uac2_get_feature_volume,
};

const struct uac2_ops *sof_get_uac2_ops(void)
{
	return &g_uac2_ops;
}

extern const struct sof_static_topology g_esp32p4_static_topology;

int sof_static_pipelines_init(struct sof *sof)
{
	ARG_UNUSED(sof);
	return sof_static_topology_init(&g_esp32p4_static_topology);
}

int sof_static_pipeline_set_clock_mode(enum sof_audio_interface iface, enum sof_clock_mode mode)
{
	g_status.active_interface = iface;
	g_status.clock_mode = mode;
	LOG_INF("Set interface %d clock mode to %s", iface, mode == SOF_CLOCK_MASTER ? "MASTER" : "SLAVE");
	return 0;
}

int sof_static_pipeline_set_eq_bypass(bool is_capture, bool bypass)
{
	if (is_capture) {
		g_status.eq_capture_bypassed = bypass;
		return sof_static_kcontrol_set(5, bypass ? 0 : 1);
	} else {
		g_status.eq_playback_bypassed = bypass;
		return sof_static_kcontrol_set(2, bypass ? 0 : 1);
	}
}

int sof_static_pipeline_set_drc_bypass(bool bypass)
{
	g_status.drc_playback_bypassed = bypass;
	return sof_static_kcontrol_set(3, bypass ? 0 : 1);
}

int sof_static_pipeline_set_tdfb_bypass(bool bypass)
{
	g_status.tdfb_capture_bypassed = bypass;
	return sof_static_kcontrol_set(4, bypass ? 0 : 1);
}

int sof_static_pipeline_set_volume(uint32_t pipeline_id, int16_t volume)
{
	if (pipeline_id == 1) {
		return sof_uac2_set_feature_volume(NULL, PLAYBACK_FU_ID, 0, volume, NULL);
	} else if (pipeline_id == 2) {
		return sof_uac2_set_feature_volume(NULL, CAPTURE_FU_ID, 0, volume, NULL);
	}
	return -EINVAL;
}

int sof_static_pipeline_set_mute(uint32_t pipeline_id, bool mute)
{
	if (pipeline_id == 1) {
		return sof_uac2_set_feature_mute(NULL, PLAYBACK_FU_ID, 0, mute, NULL);
	} else if (pipeline_id == 2) {
		return sof_uac2_set_feature_mute(NULL, CAPTURE_FU_ID, 0, mute, NULL);
	}
	return -EINVAL;
}

int sof_static_pipeline_get_volume(uint32_t pipeline_id, int16_t *volume)
{
	if (pipeline_id == 1) {
		return sof_uac2_get_feature_volume(NULL, PLAYBACK_FU_ID, 0, volume, NULL);
	} else if (pipeline_id == 2) {
		return sof_uac2_get_feature_volume(NULL, CAPTURE_FU_ID, 0, volume, NULL);
	}
	return -EINVAL;
}

int sof_static_pipeline_get_mute(uint32_t pipeline_id, bool *mute)
{
	if (pipeline_id == 1) {
		return sof_uac2_get_feature_mute(NULL, PLAYBACK_FU_ID, 0, mute, NULL);
	} else if (pipeline_id == 2) {
		return sof_uac2_get_feature_mute(NULL, CAPTURE_FU_ID, 0, mute, NULL);
	}
	return -EINVAL;
}

int sof_static_pipeline_set_playback_active(bool start)
{
	g_status.playback_active = start;
	return sof_static_pipeline_trigger_by_uac2_term(PLAYBACK_TERM_ID, start);
}

int sof_static_pipeline_set_capture_active(bool start)
{
	g_status.capture_active = start;
	return sof_static_pipeline_trigger_by_uac2_term(CAPTURE_TERM_ID, start);
}

void sof_static_pipeline_get_status(struct sof_static_pipeline_status *status)
{
	if (!status)
		return;

	*status = g_status;

	int32_t val = 0;
	if (sof_static_kcontrol_get(2, &val) == 0)
		status->eq_playback_bypassed = (val == 0);
	if (sof_static_kcontrol_get(3, &val) == 0)
		status->drc_playback_bypassed = (val == 0);
	if (sof_static_kcontrol_get(4, &val) == 0)
		status->tdfb_capture_bypassed = (val == 0);
	if (sof_static_kcontrol_get(5, &val) == 0)
		status->eq_capture_bypassed = (val == 0);
}
