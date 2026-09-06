// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <sof/audio/usb_audio.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/lib/dai-zephyr.h>
#include <ipc/topology.h>
#include <ipc/control.h>
#include <kernel/header.h>
#include <user/eq.h>
#include "../eq_iir/eq_iir.h"
#include "../drc/drc_user.h"
#include "../drc/drc.h"
#include <module/ipc4/base-config.h>
#include "../volume/peak_volume.h"
#include <rtos/sof.h>
#include <rtos/alloc.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/device.h>

LOG_MODULE_REGISTER(sof_static_pipeline, CONFIG_SOF_LOG_LEVEL);

extern const struct sof_uuid usb_audio_uuid;
extern const struct sof_uuid volume_uuid;
extern const struct sof_uuid eq_iir_uuid;
extern const struct sof_uuid drc_uuid;
extern const struct sof_uuid tdfb_uuid;
extern const struct sof_uuid dai_uuid;

struct drc_state;
extern void drc_reset_state(struct processing_module *mod, struct drc_state *state);
void * const g_drc_force __used = (void *)drc_reset_state;

#ifndef VOL_MAX
#define VOL_MAX INT32_MAX
#endif

static const struct ipc4_base_module_cfg s_default_ipc4_base_cfg = {
	.ibs = 192,
	.obs = 192,
	.is_pages = 1,
	.audio_fmt = {
		.sampling_frequency = 48000,
		.depth = IPC4_DEPTH_16BIT,
		.ch_map = 0x10,
		.ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
		.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
		.channels_count = 2,
		.valid_bit_depth = IPC4_DEPTH_16BIT,
		.s_type = IPC4_TYPE_SIGNED_INTEGER,
	},
};

struct static_ipc4_vol_init_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_peak_volume_config config[1];
};

static const struct static_ipc4_vol_init_cfg s_default_ipc4_vol_cfg = {
	.base_cfg = {
		.ibs = 192,
		.obs = 192,
		.is_pages = 1,
		.audio_fmt = {
			.sampling_frequency = 48000,
			.depth = IPC4_DEPTH_16BIT,
			.ch_map = 0x10,
			.ch_cfg = IPC4_CHANNEL_CONFIG_STEREO,
			.interleaving_style = IPC4_CHANNELS_INTERLEAVED,
			.channels_count = 2,
			.valid_bit_depth = IPC4_DEPTH_16BIT,
			.s_type = IPC4_TYPE_SIGNED_INTEGER,
		},
	},
	.config = {
		{
			.channel_id = 0xffffffff,
			.target_volume = 0x7FFFFFFF,
			.curve_type = IPC4_AUDIO_CURVE_TYPE_WINDOWS_FADE,
			.curve_duration = 100000,
		},
	},
};

/* Standard 2-channel 4-band parametric IIR EQ configuration blob (ABI header + sof_eq_iir_config) */
static const uint32_t sof_default_iir_coef_2ch[51] = {
	0x00464f53, 0x00000000, 0x000000ac, 0x03013000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x000000ac, 0x00000002, 0x00000001, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000004, 0x00000004, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0xc12c82bd,
	0x7ed0b52e, 0x1fc7cc0c, 0xc07067e9, 0x1fc7cc0c,
	0x00000000, 0x00004000, 0xcad0cdef, 0x742e8c5d,
	0x0cdc9086, 0xe2f11723, 0x10b2f932, 0x00000000,
	0x00004000, 0xcf45334a, 0x68260de9, 0x0a54e176,
	0xe5d6cb75, 0x11fc1f3d, 0x00000000, 0x00004000,
	0xf2940609, 0xe25f3930, 0x0d69ba64, 0x1ad374c8,
	0x0d69ba64, 0xfffffffb, 0x000045bf
};

/* Standard Speaker Default DRC compressor configuration blob (ABI header + sof_drc_config) */
static const uint32_t sof_default_drc_coef[35] = {
	0x00464f53, 0x00000000, 0x0000006c, 0x0301a000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000006c, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000001, 0xe2000000, 0x14000000,
	0x0a000000, 0x00624dd3, 0x02061b8a, 0x06666666,
	0x00ba972f, 0x001e0c18, 0xffe04220, 0x0050f44e,
	0x08349f9a, 0x04d82cd3, 0x0071c71c, 0xff777777,
	0x001f77d8, 0x00000005, 0x00438000, 0x00047dd7,
	0x0025cea0, 0x00097dd7, 0x0000b5b1
};

static int sof_static_send_comp_config(struct comp_dev *dev, const void *abi_blob, size_t abi_blob_total_size)
{
	if (!dev)
		return -EINVAL;

	const struct sof_abi_hdr *hdr = (const struct sof_abi_hdr *)abi_blob;
	struct processing_module *mod = comp_mod(dev);
	if (!mod || !mod->dev || !mod->dev->drv || !mod->dev->drv->adapter_ops ||
	    !mod->dev->drv->adapter_ops->set_configuration) {
		LOG_ERR("No adapter_ops set_configuration for comp %d", dev->ipc_config.id);
		return -EINVAL;
	}

	size_t cdata_size = sizeof(struct sof_ipc_ctrl_data) + sizeof(struct sof_abi_hdr) + hdr->size;
	struct sof_ipc_ctrl_data *cdata = rzalloc(SOF_MEM_FLAG_USER, cdata_size);
	if (!cdata) {
		LOG_ERR("Failed to allocate ctrl_data size %zu", cdata_size);
		return -ENOMEM;
	}

	cdata->cmd = SOF_CTRL_CMD_BINARY;
	cdata->num_elems = hdr->size;
	cdata->data[0].magic = hdr->magic;
	cdata->data[0].type = hdr->type;
	cdata->data[0].size = hdr->size;
	cdata->data[0].abi = hdr->abi;
	memcpy_s(cdata->data[0].data, hdr->size, (const uint8_t *)abi_blob + sizeof(struct sof_abi_hdr), hdr->size);

	int ret = mod->dev->drv->adapter_ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
								hdr->size, (const uint8_t *)cdata,
								hdr->size, NULL, 0);
	if (ret < 0) {
		LOG_ERR("set_configuration failed for comp %d: ret %d", dev->ipc_config.id, ret);
	} else {
		LOG_INF("Default DSP configuration loaded for comp %d (%u bytes)",
			dev->ipc_config.id, hdr->size);
	}

	rfree(cdata);
	return ret;
}

int volume_set_chan(struct processing_module *mod, int chan, int32_t vol, bool constant_rate_ramp);
void volume_set_chan_mute(struct processing_module *mod, int chan);
void volume_set_chan_unmute(struct processing_module *mod, int chan);

static struct pipeline *g_playback_pipe;
static struct pipeline *g_capture_pipe;

static struct comp_dev *g_comp_usb_playback;
static struct comp_dev *g_comp_vol_playback;
static struct comp_dev *g_comp_eq_playback;
static struct comp_dev *g_comp_drc_playback;
static struct comp_dev *g_comp_dai_playback;

static struct comp_dev *g_comp_dai_capture;
static struct comp_dev *g_comp_tdfb_capture;
static struct comp_dev *g_comp_eq_capture;
static struct comp_dev *g_comp_vol_capture;
static struct comp_dev *g_comp_usb_capture;

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

K_MEM_SLAB_DEFINE_STATIC(uac2_rx_slab, 1024, 16, 64);
K_MEM_SLAB_DEFINE_STATIC(uac2_tx_slab, 1024, 16, 64);

#define PLAYBACK_FU_ID       UAC2_ENTITY_ID(DT_NODELABEL(i2s_fu))
#define PLAYBACK_EQ_FU_ID    UAC2_ENTITY_ID(DT_NODELABEL(pb_eq_fu))
#define PLAYBACK_DRC_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(pb_drc_fu))
#define CAPTURE_TDFB_FU_ID   UAC2_ENTITY_ID(DT_NODELABEL(cap_tdfb_fu))
#define CAPTURE_EQ_FU_ID     UAC2_ENTITY_ID(DT_NODELABEL(cap_eq_fu))
#define CAPTURE_FU_ID        UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_fu))
#define PLAYBACK_TERM_ID     UAC2_ENTITY_ID(DT_NODELABEL(i2s_out_terminal))
#define CAPTURE_TERM_ID      UAC2_ENTITY_ID(DT_NODELABEL(i2s_in_terminal))

/* Helper to find a registered SOF component driver */
static const struct comp_driver *sof_static_find_driver(const struct sof_uuid *uuid, uint32_t type)
{
	struct comp_driver_list *drivers = comp_drivers_get();
	struct list_item *clist;

	if (!drivers)
		return NULL;

	list_for_item(clist, &drivers->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (uuid && info->drv->uid && !memcmp(info->drv->uid, uuid, UUID_SIZE)) {
			return info->drv;
		}
		if (!uuid && info->drv->type == type) {
			return info->drv;
		}
	}
	return NULL;
}

extern volatile uint32_t g_dwc2_rxflvl_ep1_cnt;
extern volatile uint32_t g_dwc2_xfercompl_ep1_cnt;
extern volatile uint32_t g_dwc2_incompisoout_cnt;
extern volatile uint32_t g_dwc2_incompisoin_cnt;

static uint32_t s_sof_diag_cnt;

/* UAC2 Class Callbacks */
void sof_uac2_sof_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (g_status.playback_active) {
		s_sof_diag_cnt++;
		if (s_sof_diag_cnt % 1000 == 0) {
			struct comp_data *cd_eq = g_comp_eq_playback ? module_get_private_data(comp_mod(g_comp_eq_playback)) : NULL;
			struct drc_comp_data *cd_drc = g_comp_drc_playback ? module_get_private_data(comp_mod(g_comp_drc_playback)) : NULL;
			LOG_INF("[SOF UAC2] Playback SOFs: %u, Free RX: %u | EQ: %s, DRC: %s",
				s_sof_diag_cnt, k_mem_slab_num_free_get(&uac2_rx_slab),
				(cd_eq && cd_eq->eq_iir_func == eq_iir_s16_default) ? "ACTIVE" : "BYPASS",
				(cd_drc && cd_drc->enabled) ? "ACTIVE" : "BYPASS");
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

	if (terminal == PLAYBACK_TERM_ID) { /* Playback Terminal */
		g_status.playback_active = enabled;
		if (enabled) {
			LOG_INF("[SOF Pipeline] Playback Stream START (Terminal %u, Rate %u Hz, 2ch 16-bit)",
				terminal, g_status.sample_rate ? g_status.sample_rate : 48000);
			if (g_playback_pipe && g_playback_pipe->source_comp) {
				pipeline_trigger(g_playback_pipe, g_playback_pipe->source_comp, COMP_TRIGGER_PRE_START);
			}
		} else {
			LOG_INF("[SOF Pipeline] Playback Stream STOP (Terminal %u)", terminal);
			if (g_playback_pipe && g_playback_pipe->source_comp) {
				pipeline_trigger(g_playback_pipe, g_playback_pipe->source_comp, COMP_TRIGGER_STOP);
			}
		}
	} else if (terminal == CAPTURE_TERM_ID) { /* Capture Terminal */
		g_status.capture_active = enabled;
		if (enabled) {
			LOG_INF("[SOF Pipeline] Capture Stream START (Terminal %u, Rate %u Hz, 2ch 16-bit)",
				terminal, g_status.sample_rate ? g_status.sample_rate : 48000);
			if (g_capture_pipe && g_comp_usb_capture) {
				pipeline_trigger(g_capture_pipe, g_comp_usb_capture, COMP_TRIGGER_PRE_START);
			}
		} else {
			LOG_INF("[SOF Pipeline] Capture Stream STOP (Terminal %u)", terminal);
			if (g_capture_pipe && g_comp_usb_capture) {
				pipeline_trigger(g_capture_pipe, g_comp_usb_capture, COMP_TRIGGER_STOP);
			}
		}
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
		if (g_comp_vol_playback) {
			struct processing_module *mod = comp_mod(g_comp_vol_playback);
			if (mute) {
				volume_set_chan_mute(mod, 0);
				volume_set_chan_mute(mod, 1);
			} else {
				volume_set_chan_unmute(mod, 0);
				volume_set_chan_unmute(mod, 1);
			}
		}
	} else if (entity_id == PLAYBACK_EQ_FU_ID) {
		sof_static_pipeline_set_eq_bypass(false, mute);
	} else if (entity_id == PLAYBACK_DRC_FU_ID) {
		sof_static_pipeline_set_drc_bypass(mute);
	} else if (entity_id == CAPTURE_TDFB_FU_ID) {
		sof_static_pipeline_set_tdfb_bypass(mute);
	} else if (entity_id == CAPTURE_EQ_FU_ID) {
		sof_static_pipeline_set_eq_bypass(true, mute);
	} else if (entity_id == CAPTURE_FU_ID) {
		g_status.capture_mute = mute;
		if (g_comp_vol_capture) {
			struct processing_module *mod = comp_mod(g_comp_vol_capture);
			if (mute) {
				volume_set_chan_mute(mod, 0);
				volume_set_chan_mute(mod, 1);
			} else {
				volume_set_chan_unmute(mod, 0);
				volume_set_chan_unmute(mod, 1);
			}
		}
	}
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
	} else if (entity_id == PLAYBACK_EQ_FU_ID) {
		*mute = g_status.eq_playback_bypassed;
	} else if (entity_id == PLAYBACK_DRC_FU_ID) {
		*mute = g_status.drc_playback_bypassed;
	} else if (entity_id == CAPTURE_TDFB_FU_ID) {
		*mute = g_status.tdfb_capture_bypassed;
	} else if (entity_id == CAPTURE_EQ_FU_ID) {
		*mute = g_status.eq_capture_bypassed;
	} else if (entity_id == CAPTURE_FU_ID) {
		*mute = g_status.capture_mute;
	} else {
		*mute = false;
	}
	return 0;
}

#define SOF_VOL_ZERO_DB BIT(23)

static int32_t uac2_to_sof_volume(int16_t volume)
{
	/* UAC2 volume is in Q8.8 dB units (1 unit = 1/256 dB) */
	/* 0x0000 = 0 dB -> SOF_VOL_ZERO_DB */
	/* <= -90 dB -> 0 (mute) */
	if (volume <= -90 * 256) {
		return 0;
	}
	if (volume >= 0) {
		return SOF_VOL_ZERO_DB - 1;
	}
	/* -6.02 dB halves linear amplitude */
	int32_t db_x10 = (int32_t)(-volume) * 10 / 256;
	int shift = db_x10 / 60;
	if (shift >= 31) {
		return 0;
	}
	int rem = db_x10 % 60;
	uint64_t v = (uint64_t)(SOF_VOL_ZERO_DB - 1) >> shift;
	v = (v * (60 - rem)) / 60;
	return (int32_t)v;
}

int sof_uac2_set_feature_volume(const struct device *dev, uint8_t entity_id,
				uint8_t channel, int16_t volume, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	ARG_UNUSED(user_data);

	LOG_INF("Host set volume: entity=%u, ch=%u, vol=%d (0x%04x, %d dB)",
		entity_id, channel, volume, (uint16_t)volume, volume / 256);

	int32_t sof_vol = uac2_to_sof_volume(volume);

	if (entity_id == PLAYBACK_FU_ID) {
		g_status.playback_volume = volume;
		if (g_comp_vol_playback) {
			struct processing_module *mod = comp_mod(g_comp_vol_playback);
			volume_set_chan(mod, 0, sof_vol, true);
			volume_set_chan(mod, 1, sof_vol, true);
		}
	} else if (entity_id == CAPTURE_FU_ID) {
		g_status.capture_volume = volume;
		if (g_comp_vol_capture) {
			struct processing_module *mod = comp_mod(g_comp_vol_capture);
			volume_set_chan(mod, 0, sof_vol, true);
			volume_set_chan(mod, 1, sof_vol, true);
		}
	}
	return 0;
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

static void sof_static_init_buffer_params(struct comp_buffer *buf, uint32_t dir)
{
	if (!buf)
		return;

	struct sof_ipc_stream_params params;
	memset(&params, 0, sizeof(params));
	params.rate = 48000;
	params.channels = 2;
	params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	params.sample_container_bytes = 2;
	params.sample_valid_bytes = 2;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.host_period_bytes = 48 * 4;
	params.direction = dir;
	params.chmap[0] = SOF_CHMAP_FL;
	params.chmap[1] = SOF_CHMAP_FR;

	buffer_set_params(buf, &params, BUFFER_UPDATE_FORCE);
	audio_stream_set_valid_fmt(&buf->stream, SOF_IPC_FRAME_S16_LE);
	audio_stream_set_rate(&buf->stream, 48000);
	audio_stream_set_channels(&buf->stream, 2);
	audio_stream_set_frm_fmt(&buf->stream, SOF_IPC_FRAME_S16_LE);

	struct sof_sink *sink = audio_buffer_get_sink(&buf->audio_buffer);
	if (sink) {
		sink_set_valid_fmt(sink, SOF_IPC_FRAME_S16_LE);
		sink_set_rate(sink, 48000);
		sink_set_channels(sink, 2);
		sink_set_frm_fmt(sink, SOF_IPC_FRAME_S16_LE);
	}
	struct sof_source *source = audio_buffer_get_source(&buf->audio_buffer);
	if (source) {
		source_set_valid_fmt(source, SOF_IPC_FRAME_S16_LE);
		source_set_rate(source, 48000);
		source_set_channels(source, 2);
		source_set_frm_fmt(source, SOF_IPC_FRAME_S16_LE);
	}
}

/* Static Pipelines & Graphs Construction */
int sof_static_pipelines_init(struct sof *sof)
{
	LOG_INF("Building SOF static audio processing graphs (Playback & Capture)...");

	/* 1. Allocate Top-Level Pipeline Containers */
	g_playback_pipe = pipeline_new(NULL, 1, 0, 1, NULL);
	if (!g_playback_pipe) {
		LOG_ERR("Failed to allocate playback pipeline");
		return -ENOMEM;
	}
	g_playback_pipe->pipeline_id = 1;
	g_playback_pipe->period = 1000; /* 1ms period */
	g_playback_pipe->frames_per_sched = 48; /* 48 frames @ 48kHz */
	g_playback_pipe->time_domain = SOF_TIME_DOMAIN_TIMER;

	g_capture_pipe = pipeline_new(NULL, 2, 0, 6, NULL);
	if (!g_capture_pipe) {
		LOG_ERR("Failed to allocate capture pipeline");
		return -ENOMEM;
	}
	g_capture_pipe->pipeline_id = 2;
	g_capture_pipe->period = 1000;
	g_capture_pipe->frames_per_sched = 48;
	g_capture_pipe->time_domain = SOF_TIME_DOMAIN_TIMER;

	/* 2. Lookup Component Drivers */
	const struct comp_driver *drv_usb  = sof_static_find_driver(&usb_audio_uuid, SOF_COMP_HOST);
	const struct comp_driver *drv_vol  = sof_static_find_driver(&volume_uuid, SOF_COMP_MODULE_ADAPTER);
	const struct comp_driver *drv_eq   = sof_static_find_driver(&eq_iir_uuid, SOF_COMP_MODULE_ADAPTER);
	const struct comp_driver *drv_drc  = sof_static_find_driver(&drc_uuid, SOF_COMP_MODULE_ADAPTER);
	const struct comp_driver *drv_tdfb = sof_static_find_driver(&tdfb_uuid, SOF_COMP_MODULE_ADAPTER);
	const struct comp_driver *drv_dai  = sof_static_find_driver(NULL, SOF_COMP_DAI);

	LOG_INF("Component Drivers: USB=%p, VOL=%p, EQ=%p, DRC=%p, TDFB=%p, DAI=%p",
		drv_usb, drv_vol, drv_eq, drv_drc, drv_tdfb, drv_dai);

	/* 3. Construct Playback Pipeline (Pipeline 1: USB -> Vol -> EQ -> DRC -> DAI) */
	if (drv_usb) {
		struct comp_ipc_config cfg = {
			.id = 1,
			.pipeline_id = 1,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
			.type = SOF_COMP_HOST,
		};
		g_comp_usb_playback = drv_usb->ops.create(drv_usb, &cfg, NULL);
		if (g_comp_usb_playback) {
			g_comp_usb_playback->direction = SOF_IPC_STREAM_PLAYBACK;
			g_comp_usb_playback->pipeline = g_playback_pipe;
			g_comp_usb_playback->period = 1000;
			g_comp_usb_playback->frames = 48;
		}
	}

	if (drv_vol) {
		struct comp_ipc_config cfg = {
			.id = 2,
			.pipeline_id = 1,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_process spec = {
			.size = sizeof(s_default_ipc4_vol_cfg),
			.data = (const uint8_t *)&s_default_ipc4_vol_cfg,
		};
		g_comp_vol_playback = drv_vol->ops.create(drv_vol, &cfg, &spec);
		if (g_comp_vol_playback) {
			g_comp_vol_playback->direction = SOF_IPC_STREAM_PLAYBACK;
			g_comp_vol_playback->pipeline = g_playback_pipe;
			g_comp_vol_playback->period = 1000;
			g_comp_vol_playback->frames = 48;
		}
	}

	if (drv_eq) {
		struct comp_ipc_config cfg = {
			.id = 3,
			.pipeline_id = 1,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_process spec = {
			.size = sizeof(s_default_ipc4_base_cfg),
			.data = (const uint8_t *)&s_default_ipc4_base_cfg,
		};
		g_comp_eq_playback = drv_eq->ops.create(drv_eq, &cfg, &spec);
		if (g_comp_eq_playback) {
			g_comp_eq_playback->direction = SOF_IPC_STREAM_PLAYBACK;
			g_comp_eq_playback->pipeline = g_playback_pipe;
			g_comp_eq_playback->period = 1000;
			g_comp_eq_playback->frames = 48;
			sof_static_send_comp_config(g_comp_eq_playback, sof_default_iir_coef_2ch,
						    sizeof(sof_default_iir_coef_2ch));
		}
	}

	if (drv_drc) {
		struct comp_ipc_config cfg = {
			.id = 4,
			.pipeline_id = 1,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_process spec = {
			.size = sizeof(s_default_ipc4_base_cfg),
			.data = (const uint8_t *)&s_default_ipc4_base_cfg,
		};
		g_comp_drc_playback = drv_drc->ops.create(drv_drc, &cfg, &spec);
		if (g_comp_drc_playback) {
			g_comp_drc_playback->direction = SOF_IPC_STREAM_PLAYBACK;
			g_comp_drc_playback->pipeline = g_playback_pipe;
			g_comp_drc_playback->period = 1000;
			g_comp_drc_playback->frames = 48;
			sof_static_send_comp_config(g_comp_drc_playback, sof_default_drc_coef,
						    sizeof(sof_default_drc_coef));
		}
	}

	if (drv_dai) {
		struct comp_ipc_config cfg = {
			.id = 5,
			.pipeline_id = 1,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_dai dai_cfg = {
			.type = SOF_DAI_ESP32_I2S,
			.dai_index = 0,
			.direction = SOF_IPC_STREAM_PLAYBACK,
			.sampling_frequency = 48000,
			.dma_buffer_size = 1024,
			.format = SOF_DAI_FMT_I2S,
		};
		g_comp_dai_playback = drv_dai->ops.create(drv_dai, &cfg, &dai_cfg);
		if (g_comp_dai_playback) {
			g_comp_dai_playback->direction = SOF_IPC_STREAM_PLAYBACK;
			g_comp_dai_playback->pipeline = g_playback_pipe;
			g_comp_dai_playback->period = 1000;
			g_comp_dai_playback->frames = 48;
			struct sof_ipc_dai_config spec_cfg = {
				.type = SOF_DAI_ESP32_I2S,
				.dai_index = 0,
				.format = SOF_DAI_FMT_I2S,
			};
			comp_dai_config(g_comp_dai_playback, &dai_cfg, &spec_cfg);
		}
	}

	/* Allocate playback intermediate stream buffers */
	struct comp_buffer *buf_pb_1 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);
	struct comp_buffer *buf_pb_2 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);
	struct comp_buffer *buf_pb_3 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);
	struct comp_buffer *buf_pb_4 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);

	sof_static_init_buffer_params(buf_pb_1, SOF_IPC_STREAM_PLAYBACK);
	sof_static_init_buffer_params(buf_pb_2, SOF_IPC_STREAM_PLAYBACK);
	sof_static_init_buffer_params(buf_pb_3, SOF_IPC_STREAM_PLAYBACK);
	sof_static_init_buffer_params(buf_pb_4, SOF_IPC_STREAM_PLAYBACK);

	if (g_comp_usb_playback && buf_pb_1 && g_comp_vol_playback) {
		pipeline_connect(g_comp_usb_playback, buf_pb_1, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_vol_playback, buf_pb_1, PPL_CONN_DIR_BUFFER_TO_COMP);
	}
	if (g_comp_vol_playback && buf_pb_2 && g_comp_eq_playback) {
		pipeline_connect(g_comp_vol_playback, buf_pb_2, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_eq_playback,  buf_pb_2, PPL_CONN_DIR_BUFFER_TO_COMP);
	}
	if (g_comp_eq_playback && buf_pb_3 && g_comp_drc_playback) {
		pipeline_connect(g_comp_eq_playback,  buf_pb_3, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_drc_playback, buf_pb_3, PPL_CONN_DIR_BUFFER_TO_COMP);
	}
	if (g_comp_drc_playback && buf_pb_4) {
		pipeline_connect(g_comp_drc_playback, buf_pb_4, PPL_CONN_DIR_COMP_TO_BUFFER);
		if (g_comp_dai_playback) {
			pipeline_connect(g_comp_dai_playback, buf_pb_4, PPL_CONN_DIR_BUFFER_TO_COMP);
		}
	}

	g_playback_pipe->source_comp = g_comp_usb_playback;
	g_playback_pipe->sink_comp = g_comp_dai_playback ? g_comp_dai_playback : g_comp_drc_playback;
	g_playback_pipe->sched_comp = g_comp_usb_playback;

	/* 4. Construct Capture Pipeline (Pipeline 2: DAI -> TDFB -> EQ -> Vol -> USB) */
	if (drv_dai) {
		struct comp_ipc_config cfg = {
			.id = 6,
			.pipeline_id = 2,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_dai dai_cfg = {
			.type = SOF_DAI_ESP32_I2S,
			.dai_index = 0,
			.direction = SOF_IPC_STREAM_CAPTURE,
			.sampling_frequency = 48000,
			.dma_buffer_size = 1024,
			.format = SOF_DAI_FMT_I2S,
		};
		g_comp_dai_capture = drv_dai->ops.create(drv_dai, &cfg, &dai_cfg);
		if (g_comp_dai_capture) {
			g_comp_dai_capture->direction = SOF_IPC_STREAM_CAPTURE;
			g_comp_dai_capture->pipeline = g_capture_pipe;
			g_comp_dai_capture->period = 1000;
			g_comp_dai_capture->frames = 48;
			struct sof_ipc_dai_config spec_cfg = {
				.type = SOF_DAI_ESP32_I2S,
				.dai_index = 0,
				.format = SOF_DAI_FMT_I2S,
			};
			comp_dai_config(g_comp_dai_capture, &dai_cfg, &spec_cfg);
		}
	}

	if (drv_tdfb) {
		struct comp_ipc_config cfg = {
			.id = 7,
			.pipeline_id = 2,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_process spec = {
			.size = sizeof(s_default_ipc4_base_cfg),
			.data = (const uint8_t *)&s_default_ipc4_base_cfg,
		};
		g_comp_tdfb_capture = drv_tdfb->ops.create(drv_tdfb, &cfg, &spec);
		if (g_comp_tdfb_capture) {
			g_comp_tdfb_capture->direction = SOF_IPC_STREAM_CAPTURE;
			g_comp_tdfb_capture->pipeline = g_capture_pipe;
			g_comp_tdfb_capture->period = 1000;
			g_comp_tdfb_capture->frames = 48;
		}
	}

	if (drv_eq) {
		struct comp_ipc_config cfg = {
			.id = 8,
			.pipeline_id = 2,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_process spec = {
			.size = sizeof(s_default_ipc4_base_cfg),
			.data = (const uint8_t *)&s_default_ipc4_base_cfg,
		};
		g_comp_eq_capture = drv_eq->ops.create(drv_eq, &cfg, &spec);
		if (g_comp_eq_capture) {
			g_comp_eq_capture->direction = SOF_IPC_STREAM_CAPTURE;
			g_comp_eq_capture->pipeline = g_capture_pipe;
			g_comp_eq_capture->period = 1000;
			g_comp_eq_capture->frames = 48;
			sof_static_send_comp_config(g_comp_eq_capture, sof_default_iir_coef_2ch,
						    sizeof(sof_default_iir_coef_2ch));
		}
	}

	if (drv_vol) {
		struct comp_ipc_config cfg = {
			.id = 9,
			.pipeline_id = 2,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
		};
		struct ipc_config_process spec = {
			.size = sizeof(s_default_ipc4_vol_cfg),
			.data = (const uint8_t *)&s_default_ipc4_vol_cfg,
		};
		g_comp_vol_capture = drv_vol->ops.create(drv_vol, &cfg, &spec);
		if (g_comp_vol_capture) {
			g_comp_vol_capture->direction = SOF_IPC_STREAM_CAPTURE;
			g_comp_vol_capture->pipeline = g_capture_pipe;
			g_comp_vol_capture->period = 1000;
			g_comp_vol_capture->frames = 48;
		}
	}

	if (drv_usb) {
		struct comp_ipc_config cfg = {
			.id = 10,
			.pipeline_id = 2,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = SOF_IPC_FRAME_S16_LE,
			.type = SOF_COMP_HOST,
		};
		g_comp_usb_capture = drv_usb->ops.create(drv_usb, &cfg, NULL);
		if (g_comp_usb_capture) {
			g_comp_usb_capture->direction = SOF_IPC_STREAM_CAPTURE;
			g_comp_usb_capture->pipeline = g_capture_pipe;
			g_comp_usb_capture->period = 1000;
			g_comp_usb_capture->frames = 48;
		}
	}

	/* Allocate capture intermediate stream buffers */
	struct comp_buffer *buf_cap_1 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);
	struct comp_buffer *buf_cap_2 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);
	struct comp_buffer *buf_cap_3 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);
	struct comp_buffer *buf_cap_4 = buffer_alloc(NULL, 1024, SOF_MEM_FLAG_DMA | SOF_MEM_FLAG_USER, PLATFORM_DCACHE_ALIGN, false);

	sof_static_init_buffer_params(buf_cap_1, SOF_IPC_STREAM_CAPTURE);
	sof_static_init_buffer_params(buf_cap_2, SOF_IPC_STREAM_CAPTURE);
	sof_static_init_buffer_params(buf_cap_3, SOF_IPC_STREAM_CAPTURE);
	sof_static_init_buffer_params(buf_cap_4, SOF_IPC_STREAM_CAPTURE);

	if (g_comp_dai_capture && buf_cap_1 && g_comp_tdfb_capture) {
		pipeline_connect(g_comp_dai_capture,  buf_cap_1, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_tdfb_capture, buf_cap_1, PPL_CONN_DIR_BUFFER_TO_COMP);
	}
	if (g_comp_tdfb_capture && buf_cap_2 && g_comp_eq_capture) {
		pipeline_connect(g_comp_tdfb_capture, buf_cap_2, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_eq_capture,   buf_cap_2, PPL_CONN_DIR_BUFFER_TO_COMP);
	}
	if (g_comp_eq_capture && buf_cap_3 && g_comp_vol_capture) {
		pipeline_connect(g_comp_eq_capture,  buf_cap_3, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_vol_capture, buf_cap_3, PPL_CONN_DIR_BUFFER_TO_COMP);
	}
	if (g_comp_vol_capture && buf_cap_4 && g_comp_usb_capture) {
		pipeline_connect(g_comp_vol_capture, buf_cap_4, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(g_comp_usb_capture, buf_cap_4, PPL_CONN_DIR_BUFFER_TO_COMP);
	}

	g_capture_pipe->source_comp = g_comp_dai_capture ? g_comp_dai_capture : (g_comp_tdfb_capture ? g_comp_tdfb_capture : g_comp_vol_capture);
	g_capture_pipe->sink_comp = g_comp_usb_capture;
	g_capture_pipe->sched_comp = g_comp_usb_capture;

	/* 5. Parameterize and Prepare Pipelines */
	struct sof_ipc_pcm_params prms;

	if (g_playback_pipe->source_comp) {
		memset(&prms, 0, sizeof(prms));
		prms.params.rate = 48000;
		prms.params.channels = 2;
		prms.params.frame_fmt = SOF_IPC_FRAME_S16_LE;
		prms.params.sample_container_bytes = 2;
		prms.params.sample_valid_bytes = 2;
		prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		prms.params.host_period_bytes = 48 * 4;
		prms.comp_id = dev_comp_id(g_playback_pipe->source_comp);
		prms.params.direction = SOF_IPC_STREAM_PLAYBACK;
		prms.params.chmap[0] = SOF_CHMAP_FL;
		prms.params.chmap[1] = SOF_CHMAP_FR;
		pipeline_params(g_playback_pipe, g_playback_pipe->source_comp, &prms);
		pipeline_prepare(g_playback_pipe, g_playback_pipe->source_comp);
	}

	if (g_comp_usb_capture) {
		memset(&prms, 0, sizeof(prms));
		prms.params.rate = 48000;
		prms.params.channels = 2;
		prms.params.frame_fmt = SOF_IPC_FRAME_S16_LE;
		prms.params.sample_container_bytes = 2;
		prms.params.sample_valid_bytes = 2;
		prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		prms.params.host_period_bytes = 48 * 4;
		prms.comp_id = dev_comp_id(g_comp_usb_capture);
		prms.params.direction = SOF_IPC_STREAM_CAPTURE;
		prms.params.chmap[0] = SOF_CHMAP_FL;
		prms.params.chmap[1] = SOF_CHMAP_FR;
		pipeline_params(g_capture_pipe, g_comp_usb_capture, &prms);
		pipeline_prepare(g_capture_pipe, g_comp_usb_capture);
	}

	LOG_INF("SOF static audio graphs constructed, parameterized, and prepared successfully");
	return 0;
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
	struct comp_dev *dev = is_capture ? g_comp_eq_capture : g_comp_eq_playback;

	if (is_capture) {
		g_status.eq_capture_bypassed = bypass;
	} else {
		g_status.eq_playback_bypassed = bypass;
	}
	if (dev) {
		struct processing_module *mod = comp_mod(dev);
		struct comp_data *cd = module_get_private_data(mod);
		if (cd) {
			if (bypass) {
				cd->eq_iir_func = eq_iir_pass;
			} else if (cd->iir_delay_size) {
				cd->eq_iir_func = eq_iir_s16_default;
			}
		}
	}
	LOG_INF("%s EQ bypass set to %d", is_capture ? "Capture" : "Playback", bypass);
	return 0;
}

int sof_static_pipeline_set_drc_bypass(bool bypass)
{
	g_status.drc_playback_bypassed = bypass;
	if (g_comp_drc_playback) {
		struct processing_module *mod = comp_mod(g_comp_drc_playback);
		struct drc_comp_data *cd = module_get_private_data(mod);
		if (cd) {
			cd->enable_switch = !bypass;
		}
	}
	LOG_INF("DRC bypass set to %d", bypass);
	return 0;
}

int sof_static_pipeline_set_tdfb_bypass(bool bypass)
{
	g_status.tdfb_capture_bypassed = bypass;
	LOG_INF("TDFB bypass set to %d", bypass);
	return 0;
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

void sof_static_pipeline_get_status(struct sof_static_pipeline_status *status)
{
	if (status) {
		*status = g_status;
	}
}
