// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/usb_audio.h>
#include <sof/audio/bt_audio.h>
#include <sof/audio/bt_service.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>
#include <sof/lib/dai.h>
#include <sof/lib/dai-zephyr.h>
#include <ipc/dai.h>

LOG_MODULE_REGISTER(sof_static_pipeline, CONFIG_SOF_LOG_LEVEL);

#if defined(CONFIG_USBD_AUDIO2_CLASS)
#define PLAYBACK_FU_ID       UAC2_ENTITY_ID(DT_NODELABEL(i2s_fu))
#define PLAYBACK_EQ_FU_ID    UAC2_ENTITY_ID(DT_NODELABEL(pb_eq_fu))
#define PLAYBACK_DRC_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(pb_drc_fu))
#define CAPTURE_EQ_FU_ID     UAC2_ENTITY_ID(DT_NODELABEL(cap_eq_fu))
#define CAPTURE_FU_ID        UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_fu))
#define PLAYBACK_TERM_ID     UAC2_ENTITY_ID(DT_NODELABEL(i2s_out_terminal))
#define CAPTURE_TERM_ID      UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_terminal))
#else
#define PLAYBACK_FU_ID       1
#define PLAYBACK_EQ_FU_ID    2
#define PLAYBACK_DRC_FU_ID   3
#define CAPTURE_EQ_FU_ID     4
#define CAPTURE_FU_ID        5
#define PLAYBACK_TERM_ID     6
#define CAPTURE_TERM_ID      7
#endif

#if defined(CONFIG_NOCACHE_MEMORY)
K_MEM_SLAB_DEFINE_IN_SECT_STATIC(uac2_rx_slab, __nocache, 256, 32, 64);
K_MEM_SLAB_DEFINE_IN_SECT_STATIC(uac2_tx_slab, __nocache, 256, 64, 64);
#else
K_MEM_SLAB_DEFINE_STATIC(uac2_rx_slab, 256, 32, 64);
K_MEM_SLAB_DEFINE_STATIC(uac2_tx_slab, 256, 64, 64);
#endif

static struct sof_static_pipeline_status g_status = {
	.playback_active = false,
	.capture_active = false,
	.sample_rate = 48000,
	.active_interface = SOF_AUDIO_IF_I2S,
	.clock_mode = SOF_CLOCK_MASTER,
	.audio_route = SOF_AUDIO_ROUTE_USB_DAI,
	.bt_stream_enabled = false,
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
uint32_t g_rx_pkt_cnt = 0;
uint32_t g_tx_pkt_cnt = 0;

static void sof_uac2_send_capture_pending(const struct device *dev)
{
	if (!g_status.capture_active || !dev) {
		return;
	}

	uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;
	uint32_t frame_bytes;
	if (g_status.microframes) {
		frame_bytes = (rate / 8000) * 4; /* 6 samples * 4 bytes = 24 bytes */
	} else {
		frame_bytes = (rate / 1000) * 4; /* 48 samples * 4 bytes = 192 bytes */
	}
	if (frame_bytes == 0 || frame_bytes > 512) {
		frame_bytes = g_status.microframes ? 24 : 192;
	}

#if CONFIG_COMP_BT_AUDIO
	if (g_status.audio_route != SOF_AUDIO_ROUTE_USB_BT && !usb_audio_has_capture_data(frame_bytes)) {
		return;
	}
#else
	if (!usb_audio_has_capture_data(frame_bytes)) {
		return;
	}
#endif

	for (int iter = 0; iter < 2; iter++) {
		void *buf = NULL;
		if (k_mem_slab_alloc(&uac2_tx_slab, &buf, K_NO_WAIT) != 0) {
			static uint32_t s_slab_alloc_fails;
			s_slab_alloc_fails++;
			if (s_slab_alloc_fails <= 5 || s_slab_alloc_fails % 100 == 0) {
				LOG_WRN("[UAC2 TX SLAB FULL %u] Failed to allocate TX slab block!", s_slab_alloc_fails);
			}
			break;
		}

		bool have_data = false;
#if CONFIG_COMP_BT_AUDIO
		if (g_status.audio_route == SOF_AUDIO_ROUTE_USB_BT) {
			have_data = bt_audio_peek_capture_data(buf, frame_bytes);
		} else
#endif
		{
			have_data = usb_audio_peek_capture_data(buf, frame_bytes);
		}

		if (!have_data) {
			k_mem_slab_free(&uac2_tx_slab, buf);
			break;
		}

		if (g_status.capture_mute) {
			memset(buf, 0, frame_bytes);
		}

		int ret = usbd_uac2_send(dev, CAPTURE_TERM_ID, buf, frame_bytes);
		if (ret == 0) {
			g_tx_pkt_cnt++;
			if (!g_status.capture_mute) {
#if CONFIG_COMP_BT_AUDIO
				if (g_status.audio_route == SOF_AUDIO_ROUTE_USB_BT) {
					bt_audio_consume_capture_data(frame_bytes);
				} else
#endif
				{
					usb_audio_consume_capture_data(frame_bytes);
				}
			}
		} else {
			k_mem_slab_free(&uac2_tx_slab, buf);
			break;
		}
	}
}

/* UAC2 Class Callbacks */
void sof_uac2_sof_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (g_status.playback_active || g_status.capture_active) {
		s_sof_diag_cnt++;
		if (s_sof_diag_cnt % 1000 == 0) {
			LOG_DBG("[DIAG SOF %u] Active streaming", s_sof_diag_cnt);
		}
	} else {
		s_sof_diag_cnt = 0;
	}

	if (g_status.capture_active && dev) {
		sof_uac2_send_capture_pending(dev);
	}
}

void sof_uac2_terminal_update_cb(const struct device *dev, uint8_t terminal,
				 bool enabled, bool microframes, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	LOG_INF("[UAC2 Terminal] update: terminal %u, enabled %d, microframes %d (PB expected %u, CAP expected %u)",
		terminal, enabled, microframes, PLAYBACK_TERM_ID, CAPTURE_TERM_ID);

	g_status.microframes = microframes;

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
	g_rx_pkt_cnt++;
	if (s_rx_pkt_cnt == 1 || s_rx_pkt_cnt % 1000 == 0) {
		LOG_DBG("[SOF UAC2] Received playback packet #%u, size %u bytes", s_rx_pkt_cnt, size);
	}

	if (buf && size > 0) {
		if (g_status.playback_mute) {
			memset(buf, 0, size);
		}
#if CONFIG_COMP_BT_AUDIO
		if (g_status.audio_route == SOF_AUDIO_ROUTE_BT_DAI) {
			bt_audio_write_playback(buf, size);
		} else
#endif
		{
			usb_audio_feed_playback_data(buf, size);
		}
	}
}

void sof_uac2_buf_release(const struct device *dev, uint8_t terminal,
			  void *buf, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (buf) {
		k_mem_slab_free(&uac2_tx_slab, buf);
	}

	if (terminal == CAPTURE_TERM_ID && dev) {
		sof_uac2_send_capture_pending(dev);
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

enum usbd_speed sample_usbd_get_speed(void);

uint32_t sof_uac2_feedback_cb(const struct device *dev, uint8_t terminal,
			      void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;

	if (sample_usbd_get_speed() == USBD_SPEED_HS) {
		/* High-Speed UAC2 explicit feedback: Q16.16 format in samples per microframe (8000/s) */
		return (uint32_t)(((uint64_t)rate << 16) / 8000);
	} else {
		/* Full-Speed UAC2 explicit feedback: Q10.14 format in samples per frame (1000/s) */
		return (uint32_t)(((uint64_t)rate << 14) / 1000);
	}
}

int sof_uac2_set_feature_mute(const struct device *dev, uint8_t entity_id,
			      uint8_t channel, bool mute, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	ARG_UNUSED(user_data);

	LOG_DBG("Host set mute/switch: entity=%u, ch=%u, mute=%d", entity_id, channel, mute);
	if (entity_id == PLAYBACK_FU_ID) {
		g_status.playback_mute = mute;
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

	if (entity_id == PLAYBACK_FU_ID) {
		*mute = g_status.playback_mute;
		return 0;
	} else if (entity_id == CAPTURE_FU_ID) {
		*mute = g_status.capture_mute;
		return 0;
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

	LOG_DBG("Host set volume: entity=%u, ch=%u, vol=%d (0x%04x, %d dB)",
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
		return 0;
	} else if (entity_id == CAPTURE_FU_ID) {
		*volume = g_status.capture_volume;
		return 0;
	}

	int32_t val = 0;
	int ret = sof_static_kcontrol_get_by_uac2(entity_id, channel, &val, true);
	if (ret == 0) {
		*volume = (int16_t)val;
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

#if defined(CONFIG_PLATFORM_NRF54L15)
extern const struct sof_static_topology g_nrf54l15_static_topology;
#elif defined(CONFIG_PLATFORM_NRF54LM20)
extern const struct sof_static_topology g_nrf54lm20_static_topology;
#endif

int sof_static_pipelines_init(struct sof *sof)
{
	ARG_UNUSED(sof);
#if defined(CONFIG_PLATFORM_NRF54L15)
	int ret = sof_static_topology_init(&g_nrf54l15_static_topology);
	if (ret < 0)
		return ret;

	g_status.clock_mode = SOF_CLOCK_SLAVE;
	sof_static_pipeline_trigger(1, true);
	return 0;
#elif defined(CONFIG_PLATFORM_NRF54LM20)
	int ret = sof_static_topology_init(&g_nrf54lm20_static_topology);
	if (ret < 0)
		return ret;

	g_status.clock_mode = SOF_CLOCK_MASTER;
	sof_static_pipeline_trigger(1, true);
	sof_static_pipeline_trigger(2, true);
	return 0;
#else
	return -ENODEV;
#endif
}

int sof_static_pipeline_set_route(enum sof_audio_route route)
{
	g_status.audio_route = route;
	return 0;
}

void sof_static_pipeline_get_status(struct sof_static_pipeline_status *status)
{
	if (status) {
		*status = g_status;
	}
}
