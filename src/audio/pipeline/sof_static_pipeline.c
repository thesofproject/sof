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
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>
#include <sof/lib/dai.h>
#include <sof/lib/dai-zephyr.h>
#include <ipc/dai.h>
#include <soc/i2s_struct.h>
#include <soc/gpio_struct.h>
#include <soc/io_mux_struct.h>
#include <soc/hp_sys_clkrst_struct.h>

LOG_MODULE_REGISTER(sof_static_pipeline, CONFIG_SOF_LOG_LEVEL);

#define PLAYBACK_FU_ID       UAC2_ENTITY_ID(DT_NODELABEL(i2s_fu))
#define PLAYBACK_EQ_FU_ID    UAC2_ENTITY_ID(DT_NODELABEL(pb_eq_fu))
#define PLAYBACK_DRC_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(pb_drc_fu))
#define CAPTURE_TDFB_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(cap_tdfb_fu))
#define CAPTURE_EQ_FU_ID     UAC2_ENTITY_ID(DT_NODELABEL(cap_eq_fu))
#define CAPTURE_FU_ID        UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_fu))
#define PLAYBACK_TERM_ID     UAC2_ENTITY_ID(DT_NODELABEL(i2s_out_terminal))
#define CAPTURE_TERM_ID      UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_terminal))

K_MEM_SLAB_DEFINE_STATIC(uac2_rx_slab, 256, 32, 64);
K_MEM_SLAB_DEFINE_STATIC(uac2_tx_slab, 256, 64, 64);

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

/* UAC2 Class Callbacks */
void sof_uac2_sof_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (g_status.playback_active || g_status.capture_active) {
		s_sof_diag_cnt++;
		if (s_sof_diag_cnt % 1000 == 0) {
			LOG_INF("[DIAG SOF %u] tx_start=%u, rx_start=%u, int_raw=0x%08x, state=0x%08x | I2S0: rx_conf=0x%08x, rx_conf1=0x%08x, rx_tdm=0x%08x, rx_eof=%u",
				s_sof_diag_cnt,
				(unsigned int)I2S0.tx_conf.tx_start,
				(unsigned int)I2S0.rx_conf.rx_start,
				(unsigned int)I2S0.int_raw.val,
				(unsigned int)I2S0.state.val,
				(unsigned int)I2S0.rx_conf.val,
				(unsigned int)I2S0.rx_conf1.val,
				(unsigned int)I2S0.rx_tdm_ctrl.val,
				(unsigned int)I2S0.rx_eof_num.rx_eof_num);
			LOG_INF("[DIAG GPIOS] lo=0x%08x, hi=0x%08x (G2 lvl=%u, G3 lvl=%u, G20(DIN)=%u, G21(BCK)=%u, G22(WS)=%u, G23(DOUT)=%u)",
				(uint32_t)GPIO.in.val, (uint32_t)GPIO.in1.val,
				(uint32_t)((GPIO.in.val >> 2) & 1),
				(uint32_t)((GPIO.in.val >> 3) & 1),
				(uint32_t)((GPIO.in.val >> 20) & 1),
				(uint32_t)((GPIO.in.val >> 21) & 1),
				(uint32_t)((GPIO.in.val >> 22) & 1),
				(uint32_t)((GPIO.in.val >> 23) & 1));
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
			bool have_data = false;
			if (g_status.audio_route == SOF_AUDIO_ROUTE_USB_BT) {
				have_data = bt_audio_peek_capture_data(buf, frame_bytes);
			} else {
				have_data = usb_audio_peek_capture_data(buf, frame_bytes);
			}

			if (have_data) {
				if (g_status.capture_mute) {
					memset(buf, 0, frame_bytes);
				}
				int ret = usbd_uac2_send(dev, CAPTURE_TERM_ID, buf, frame_bytes);
				if (ret == 0) {
					if (g_status.audio_route == SOF_AUDIO_ROUTE_USB_BT) {
						bt_audio_consume_capture_data(frame_bytes);
					} else {
						usb_audio_consume_capture_data(frame_bytes);
					}
				} else {
					static uint32_t s_send_fail_cnt;
					s_send_fail_cnt++;
					if (s_send_fail_cnt <= 10 || s_send_fail_cnt % 100 == 0) {
						LOG_WRN("[UAC2 SEND RET %d (%u)] data retained in ring buffer", ret, s_send_fail_cnt);
					}
					k_mem_slab_free(&uac2_tx_slab, buf);
				}
			} else {
				k_mem_slab_free(&uac2_tx_slab, buf);
			}
		} else {
			static uint32_t s_slab_alloc_fails;
			s_slab_alloc_fails++;
			if (s_slab_alloc_fails <= 10 || s_slab_alloc_fails % 100 == 0) {
				LOG_WRN("[UAC2 TX SLAB FULL %u] Failed to allocate TX slab block!", s_slab_alloc_fails);
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
		if (g_status.audio_route == SOF_AUDIO_ROUTE_USB_BT) {
			bt_audio_feed_playback_data(buf, size);
		} else {
			usb_audio_feed_playback_data(buf, size);
		}
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

	sof_static_kcontrol_set_by_uac2(entity_id, channel, mute ? 1 : 0, false);
	return 0;
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
	int ret = sof_static_topology_init(&g_esp32p4_static_topology);
	if (ret < 0)
		return ret;

	uint8_t mac[6] = {0};
	extern int esp_efuse_mac_get_default(uint8_t *mac);
	esp_efuse_mac_get_default(mac);

	if (mac[5] == 0x17) {
		g_status.clock_mode = SOF_CLOCK_MASTER;
	} else {
		g_status.clock_mode = SOF_CLOCK_SLAVE;
	}

	return sof_static_pipeline_set_clock_mode(SOF_AUDIO_IF_I2S, g_status.clock_mode);
}

int sof_static_pipeline_set_clock_mode(enum sof_audio_interface iface, enum sof_clock_mode mode)
{
	g_status.active_interface = iface;
	g_status.clock_mode = mode;
	LOG_INF("Set interface %d clock mode to %s", iface,
		mode == SOF_CLOCK_MASTER ? "MASTER" : (mode == SOF_CLOCK_DMIC ? "DMIC (SLAVE TX)" : "SLAVE"));

	uint32_t sof_format = 0;
	if (iface == SOF_AUDIO_IF_I2S) {
		sof_format = (mode == SOF_CLOCK_MASTER) ?
			(SOF_DAI_FMT_I2S | SOF_DAI_FMT_CBC_CFC) :
			(SOF_DAI_FMT_I2S | SOF_DAI_FMT_CBP_CFP);

		const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(dai_i2s0));
		if (dev && device_is_ready(dev)) {
			uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;
			uint32_t channels = 2;
			uint32_t word_size = 16;
			uint32_t bytes_per_sample = word_size / 8;
			uint32_t frames_per_period = rate / 1000;
			if (frames_per_period == 0) {
				frames_per_period = 48;
			}
			struct dai_config cfg = {
				.type = DAI_ESP32_I2S,
				.dai_index = 0,
				.channels = channels,
				.rate = rate,
				.format = (mode == SOF_CLOCK_MASTER) ?
					(DAI_PROTO_I2S | DAI_CBC_CFC) :
					(DAI_PROTO_I2S | DAI_CBP_CFP),
				.word_size = word_size,
				.block_size = frames_per_period * channels * bytes_per_sample,
			};
			int ret = dai_config_set(dev, &cfg, NULL, 0);
			if (ret < 0) {
				LOG_ERR("Failed to set DAI I2S config: %d", ret);
				return ret;
			}
			LOG_INF("DAI I2S hardware successfully switched to %s mode",
				mode == SOF_CLOCK_MASTER ? "MASTER" : "SLAVE");
		} else {
			LOG_WRN("DAI I2S device not ready or not found");
		}
	} else if (iface == SOF_AUDIO_IF_PDM) {
		sof_format = (mode == SOF_CLOCK_MASTER) ?
			(SOF_DAI_FMT_PDM | SOF_DAI_FMT_CBC_CFC) :
			(SOF_DAI_FMT_PDM | SOF_DAI_FMT_CBP_CFP);

		const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(dai_pdm0));
		if (dev && device_is_ready(dev)) {
			uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;
			uint32_t channels = 2;
			uint32_t word_size = 16;
			uint32_t bytes_per_sample = word_size / 8;
			uint32_t frames_per_period = rate / 1000;
			if (frames_per_period == 0) {
				frames_per_period = 48;
			}
			struct dai_config cfg = {
				.type = DAI_ESP32_PDM,
				.dai_index = 1,
				.channels = channels,
				.rate = rate,
				.format = (mode == SOF_CLOCK_MASTER) ?
					(DAI_PROTO_PDM | DAI_CBC_CFC) :
					(DAI_PROTO_PDM | DAI_CBP_CFP),
				.options = (mode == SOF_CLOCK_DMIC) ? 1 : 0,
				.word_size = word_size,
				.block_size = frames_per_period * channels * bytes_per_sample,
			};
			int ret = dai_config_set(dev, &cfg, NULL, 0);
			if (ret < 0) {
				LOG_ERR("Failed to set DAI PDM config: %d", ret);
				return ret;
			}
			LOG_INF("DAI PDM hardware successfully switched to %s mode",
				mode == SOF_CLOCK_MASTER ? "MASTER" : (mode == SOF_CLOCK_DMIC ? "DMIC INJECTOR" : "SLAVE"));
		} else {
			LOG_WRN("DAI PDM device not ready or not found");
		}
	}

	/* Update static pipeline DAI components (comp 5 = PB DAI, comp 6 = CAP DAI) */
	uint32_t target_dai_type = (iface == SOF_AUDIO_IF_PDM) ? SOF_DAI_ESP32_PDM : SOF_DAI_ESP32_I2S;
	uint32_t target_dai_index = (iface == SOF_AUDIO_IF_PDM) ? 1 : 0;

	struct comp_dev *dev_pb = sof_static_comp_get(5);
	if (dev_pb) {
		struct dai_data *dd = comp_get_drvdata(dev_pb);
		if (dd) {
			if (dd->dai) {
				dai_put(dd->dai);
			}
			dd->dai = dai_get(target_dai_type, target_dai_index, DAI_CREAT);
			dd->ipc_config.type = target_dai_type;
			dd->ipc_config.dai_index = target_dai_index;
			dd->ipc_config.format = sof_format;
			dev_pb->state = COMP_STATE_READY;
		}
	}

	struct comp_dev *dev_cap = sof_static_comp_get(6);
	if (dev_cap) {
		struct dai_data *dd = comp_get_drvdata(dev_cap);
		if (dd) {
			if (dd->dai) {
				dai_put(dd->dai);
			}
			dd->dai = dai_get(target_dai_type, target_dai_index, DAI_CREAT);
			dd->ipc_config.type = target_dai_type;
			dd->ipc_config.dai_index = target_dai_index;
			dd->ipc_config.format = sof_format;
			dev_cap->state = COMP_STATE_READY;
		}
	}

	/* Reset and re-prepare pipelines 1 and 2 with updated DAI configuration */
	for (uint32_t pid = 1; pid <= 2; pid++) {
		struct pipeline *pipe = sof_static_pipeline_get(pid);
		if (!pipe)
			continue;

		struct comp_dev *host_or_sched = pipe->sched_comp ? pipe->sched_comp : pipe->source_comp;
		if (!host_or_sched)
			continue;

		pipeline_reset(pipe, host_or_sched);

		struct sof_ipc_pcm_params prms;
		memset(&prms, 0, sizeof(prms));
		uint32_t rate = g_status.sample_rate ? g_status.sample_rate : 48000;
		uint32_t channels = 2;
		prms.params.rate = rate;
		prms.params.channels = channels;
		prms.params.frame_fmt = host_or_sched->ipc_config.frame_fmt;
		uint32_t sbytes = (prms.params.frame_fmt == SOF_IPC_FRAME_FLOAT ||
				   prms.params.frame_fmt == SOF_IPC_FRAME_S32_LE) ? 4 : 2;
		prms.params.sample_container_bytes = sbytes;
		prms.params.sample_valid_bytes = sbytes;
		prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		uint32_t frames_per_ms = rate / 1000;
		if (frames_per_ms == 0) {
			frames_per_ms = 48;
		}
		prms.params.host_period_bytes = frames_per_ms * channels * sbytes;
		prms.comp_id = dev_comp_id(host_or_sched);
		prms.params.direction = (pid == 1) ? SOF_IPC_STREAM_PLAYBACK : SOF_IPC_STREAM_CAPTURE;
		prms.params.chmap[0] = SOF_CHMAP_FL;
		prms.params.chmap[1] = SOF_CHMAP_FR;

		pipeline_params(pipe, host_or_sched, &prms);
		pipeline_prepare(pipe, host_or_sched);
	}

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

int sof_static_pipeline_set_dmic_injector(bool enable)
{
	if (enable) {
		return sof_static_pipeline_set_clock_mode(SOF_AUDIO_IF_PDM, SOF_CLOCK_DMIC);
	} else {
		if (g_status.clock_mode == SOF_CLOCK_DMIC) {
			return sof_static_pipeline_set_clock_mode(SOF_AUDIO_IF_PDM, SOF_CLOCK_MASTER);
		}
		return 0;
	}
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

int sof_static_pipeline_set_bt_stream(bool enable)
{
	g_status.bt_stream_enabled = enable;
	if (enable) {
		return bt_service_start_broadcast();
	} else {
		return bt_service_stop_broadcast();
	}
}

int sof_static_pipeline_set_route(enum sof_audio_route route)
{
	g_status.audio_route = route;
	LOG_INF("SOF pipeline audio route switched to %s",
		route == SOF_AUDIO_ROUTE_USB_DAI ? "USB <-> DAI" :
		(route == SOF_AUDIO_ROUTE_BT_DAI ? "BT <-> DAI" : "USB <-> BT"));
	return 0;
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
	if (sof_static_kcontrol_get(8, &val) == 0)
		status->bt_stream_enabled = (val != 0);
	if (sof_static_kcontrol_get(10, &val) == 0)
		status->audio_route = (enum sof_audio_route)val;
}
