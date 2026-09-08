// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/static_pipeline.h>
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
#include "../tdfb/tdfb.h"
#include "../tdfb/tdfb_comp.h"
#include <module/ipc4/base-config.h>
#include "../volume/peak_volume.h"
#include <rtos/sof.h>
#include <rtos/alloc.h>
#include "../level_multiplier/level_multiplier.h"
#include <user/selector.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(static_pipeline_loader, CONFIG_SOF_LOG_LEVEL);

extern const struct sof_uuid volume_uuid;
extern const struct sof_uuid eq_iir_uuid;
extern const struct sof_uuid drc_uuid;
extern const struct sof_uuid tdfb_uuid;
extern const struct sof_uuid level_multiplier_uuid;
extern const struct sof_uuid selector_uuid;

struct drc_state;
extern void drc_reset_state(struct processing_module *mod, struct drc_state *state);
void * const g_drc_force __used = (void *)drc_reset_state;

#define MAX_STATIC_PIPELINES    16
#define MAX_STATIC_COMPS        64
#define MAX_STATIC_BUFFERS      64
#define MAX_STATIC_CONTROLS     64

static const struct sof_static_topology *s_active_topo;

static struct pipeline *s_pipelines[MAX_STATIC_PIPELINES];
static uint32_t s_pipeline_ids[MAX_STATIC_PIPELINES];
static size_t s_num_pipelines;

static struct comp_dev *s_comps[MAX_STATIC_COMPS];
static uint32_t s_comp_ids[MAX_STATIC_COMPS];
static size_t s_num_comps;

static struct comp_buffer *s_buffers[MAX_STATIC_BUFFERS];
static uint32_t s_buffer_ids[MAX_STATIC_BUFFERS];
static size_t s_num_buffers;

static int32_t s_control_vals[MAX_STATIC_CONTROLS];
static uint32_t s_sample_rate = 48000;

int volume_set_chan(struct processing_module *mod, int chan, int32_t vol, bool constant_rate_ramp);
void volume_set_chan_mute(struct processing_module *mod, int chan);
void volume_set_chan_unmute(struct processing_module *mod, int chan);

/* Lookup registered SOF component driver */
static const struct comp_driver *find_driver(const struct sof_uuid *uuid, uint32_t type)
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

/* Send initial ABI config blob (EQ, DRC, etc.) */
static int send_comp_config(struct comp_dev *dev, const void *abi_blob, size_t abi_blob_total_size)
{
	if (!dev || !abi_blob || abi_blob_total_size == 0)
		return -EINVAL;

	const struct sof_abi_hdr *hdr = (const struct sof_abi_hdr *)abi_blob;
	struct processing_module *mod = comp_mod(dev);
	if (!mod || !mod->dev || !mod->dev->drv || !mod->dev->drv->adapter_ops ||
	    !mod->dev->drv->adapter_ops->set_configuration) {
		LOG_ERR("No adapter_ops set_configuration for comp %d", dev->ipc_config.id);
		return -EINVAL;
	}

#if CONFIG_IPC_MAJOR_4
	const uint8_t *raw_data = (const uint8_t *)abi_blob + sizeof(struct sof_abi_hdr);
	int ret = mod->dev->drv->adapter_ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
								hdr->size, raw_data,
								hdr->size, NULL, 0);
#else
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
	rfree(cdata);
#endif

	if (ret < 0) {
		LOG_ERR("set_configuration failed for comp %d: ret %d", dev->ipc_config.id, ret);
	} else {
		LOG_INF("Configuration blob loaded for comp %d (%u bytes)", dev->ipc_config.id, hdr->size);
	}

	return ret;
}

static void init_buffer_params(struct comp_buffer *buf, uint32_t dir, enum sof_ipc_frame fmt,
			       uint32_t rate, uint32_t channels)
{
	if (!buf)
		return;

	uint32_t sample_bytes = (fmt == SOF_IPC_FRAME_FLOAT || fmt == SOF_IPC_FRAME_S32_LE) ? 4 : 2;
	uint32_t frames = rate / 1000; /* 1ms frames */

	struct sof_ipc_stream_params params;
	memset(&params, 0, sizeof(params));
	params.rate = rate;
	params.channels = channels;
	params.frame_fmt = fmt;
	params.sample_container_bytes = sample_bytes;
	params.sample_valid_bytes = sample_bytes;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.host_period_bytes = frames * channels * sample_bytes;
	params.direction = dir;
	params.chmap[0] = SOF_CHMAP_FL;
	params.chmap[1] = (channels > 1) ? SOF_CHMAP_FR : SOF_CHMAP_NA;

	buffer_set_params(buf, &params, BUFFER_UPDATE_FORCE);
	audio_stream_set_valid_fmt(&buf->stream, fmt);
	audio_stream_set_rate(&buf->stream, rate);
	audio_stream_set_channels(&buf->stream, channels);
	audio_stream_set_frm_fmt(&buf->stream, fmt);
	audio_stream_set_align(SOF_FRAME_BYTE_ALIGN, sample_bytes, &buf->stream);
	audio_stream_reset(&buf->stream);

	struct sof_sink *sink = audio_buffer_get_sink(&buf->audio_buffer);
	if (sink) {
		sink_set_valid_fmt(sink, fmt);
		sink_set_rate(sink, rate);
		sink_set_channels(sink, channels);
		sink_set_frm_fmt(sink, fmt);
		sink_set_alignment_constants(sink, SOF_FRAME_BYTE_ALIGN, sample_bytes);
	}
	struct sof_source *source = audio_buffer_get_source(&buf->audio_buffer);
	if (source) {
		source_set_valid_fmt(source, fmt);
		source_set_rate(source, rate);
		source_set_channels(source, channels);
		source_set_frm_fmt(source, fmt);
		source_set_alignment_constants(source, SOF_FRAME_BYTE_ALIGN, sample_bytes);
	}
}

/* Default IPC configs for volume & modules */
#if CONFIG_IPC_MAJOR_4
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

static const struct static_ipc4_vol_init_cfg s_default_vol_cfg = {
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
#else
static const struct ipc_config_volume s_default_vol_cfg = {
	.channels = 2,
	.min_value = 0,
	.max_value = INT32_MAX,
	.ramp = SOF_VOLUME_LINEAR,
	.initial_ramp = 0,
};
#endif

int sof_static_topology_init(const struct sof_static_topology *topo)
{
	if (!topo) {
		LOG_ERR("Invalid topology descriptor");
		return -EINVAL;
	}

	LOG_INF("=== Loading Static Audio Topology: '%s' ===", topo->name ? topo->name : "Unnamed");
	s_active_topo = topo;

	/* 1. Allocate Top-Level Pipelines */
	for (size_t i = 0; i < topo->num_pipelines && i < MAX_STATIC_PIPELINES; i++) {
		const struct sof_static_pipeline_desc *pdesc = &topo->pipelines[i];
		struct pipeline *pipe = pipeline_new(NULL, pdesc->pipeline_id, pdesc->priority,
						     pdesc->pipeline_id, NULL);
		if (!pipe) {
			LOG_ERR("Failed to allocate pipeline %u (%s)", pdesc->pipeline_id, pdesc->name);
			return -ENOMEM;
		}
		pipe->pipeline_id = pdesc->pipeline_id;
		pipe->period = pdesc->period ? pdesc->period : 1000;
		pipe->frames_per_sched = pdesc->frames_per_sched ? pdesc->frames_per_sched : 48;
		pipe->time_domain = pdesc->time_domain ? pdesc->time_domain : SOF_TIME_DOMAIN_TIMER;

		s_pipelines[s_num_pipelines] = pipe;
		s_pipeline_ids[s_num_pipelines] = pdesc->pipeline_id;
		s_num_pipelines++;

		LOG_INF("Pipeline %u ('%s') initialized (period %u us, %u frames)",
			pdesc->pipeline_id, pdesc->name ? pdesc->name : "", pipe->period, pipe->frames_per_sched);
	}

	/* 2. Instantiate Components */
	for (size_t i = 0; i < topo->num_comps && i < MAX_STATIC_COMPS; i++) {
		const struct sof_static_comp *cdesc = &topo->comps[i];
		uint32_t drv_type = (cdesc->type == SOF_STATIC_COMP_HOST) ? SOF_COMP_HOST :
				    (cdesc->type == SOF_STATIC_COMP_DAI) ? SOF_COMP_DAI :
				    SOF_COMP_MODULE_ADAPTER;

		const struct comp_driver *drv = find_driver(cdesc->uuid, drv_type);
		if (!drv) {
			LOG_ERR("Component driver not found for comp %u ('%s')", cdesc->id, cdesc->name);
			return -ENODEV;
		}

		struct comp_ipc_config cfg = {
			.id = cdesc->id,
			.pipeline_id = cdesc->pipeline_id,
			.core = 0,
			.proc_domain = COMP_PROCESSING_DOMAIN_LL,
			.frame_fmt = cdesc->caps.default_fmt,
			.type = drv_type,
		};

		struct comp_dev *dev = NULL;

		if (cdesc->type == SOF_STATIC_COMP_DAI) {
			struct ipc_config_dai dai_cfg = {
				.type = cdesc->ep.dai.dai_type,
				.dai_index = cdesc->ep.dai.dai_index,
				.direction = cdesc->direction,
				.sampling_frequency = cdesc->caps.default_rate ? cdesc->caps.default_rate : 48000,
				.dma_buffer_size = 1024,
				.format = cdesc->ep.dai.format,
			};
			dev = drv->ops.create(drv, &cfg, &dai_cfg);
			if (dev) {
				struct sof_ipc_dai_config spec_cfg = {
					.type = cdesc->ep.dai.dai_type,
					.dai_index = cdesc->ep.dai.dai_index,
					.format = cdesc->ep.dai.format,
				};
				comp_dai_config(dev, &dai_cfg, &spec_cfg);
			}
		} else if (cdesc->uuid && !memcmp(cdesc->uuid, &volume_uuid, UUID_SIZE)) {
			struct ipc_config_process spec = {
				.size = sizeof(s_default_vol_cfg),
				.data = (const uint8_t *)&s_default_vol_cfg,
			};
			dev = drv->ops.create(drv, &cfg, &spec);
		} else if (cdesc->uuid && !memcmp(cdesc->uuid, &selector_uuid, UUID_SIZE)) {
			static const struct sof_sel_config s_default_sel_cfg = {
				.in_channels_count = 2,
				.out_channels_count = 2,
				.sel_channel = 0,
			};
			struct ipc_config_process sel_spec = {
				.size = sizeof(s_default_sel_cfg),
				.data = (const uint8_t *)&s_default_sel_cfg,
			};
			dev = drv->ops.create(drv, &cfg, &sel_spec);
		} else {
#if CONFIG_IPC_MAJOR_4
			struct ipc4_base_module_cfg mod_base_cfg = s_default_ipc4_base_cfg;
			if (cdesc->caps.default_fmt == SOF_IPC_FRAME_FLOAT) {
				mod_base_cfg.audio_fmt.depth = IPC4_DEPTH_32BIT;
				mod_base_cfg.audio_fmt.valid_bit_depth = IPC4_DEPTH_32BIT;
				mod_base_cfg.audio_fmt.s_type = IPC4_TYPE_FLOAT;
				mod_base_cfg.ibs = 48 * 2 * 4;
				mod_base_cfg.obs = 48 * 2 * 4;
			} else if (cdesc->caps.default_fmt == SOF_IPC_FRAME_S32_LE) {
				mod_base_cfg.audio_fmt.depth = IPC4_DEPTH_32BIT;
				mod_base_cfg.audio_fmt.valid_bit_depth = IPC4_DEPTH_32BIT;
				mod_base_cfg.audio_fmt.s_type = IPC4_TYPE_SIGNED_INTEGER;
				mod_base_cfg.ibs = 48 * 2 * 4;
				mod_base_cfg.obs = 48 * 2 * 4;
			}
			struct ipc_config_process base_proc_spec = {
				.size = sizeof(mod_base_cfg),
				.data = (const uint8_t *)&mod_base_cfg,
			};
			dev = drv->ops.create(drv, &cfg, &base_proc_spec);
#else
			static const uint8_t dummy_buf[16] = {0};
			struct ipc_config_process empty_proc_spec = {
				.size = 0,
				.data = dummy_buf,
			};
			dev = drv->ops.create(drv, &cfg, &empty_proc_spec);
#endif
		}

		if (!dev) {
			LOG_ERR("Failed to create component %u ('%s')", cdesc->id, cdesc->name);
			return -ENOMEM;
		}

		dev->direction = cdesc->direction;
		dev->pipeline = sof_static_pipeline_get(cdesc->pipeline_id);
		dev->period = 1000;
		dev->frames = 48;

		/* Send initial configuration blob if specified */
		if (cdesc->init_blob && cdesc->init_blob_size > 0) {
			send_comp_config(dev, cdesc->init_blob, cdesc->init_blob_size);
		}

		s_comps[s_num_comps] = dev;
		s_comp_ids[s_num_comps] = cdesc->id;
		s_num_comps++;

		LOG_INF("Component %u ('%s') created in pipeline %u", cdesc->id, cdesc->name, cdesc->pipeline_id);
	}

	/* 3. Allocate Audio Buffers */
	for (size_t i = 0; i < topo->num_buffers && i < MAX_STATIC_BUFFERS; i++) {
		const struct sof_static_buffer *bdesc = &topo->buffers[i];
		struct comp_buffer *buf = buffer_alloc(NULL, bdesc->size, bdesc->flags,
						       PLATFORM_DCACHE_ALIGN, false);
		if (!buf) {
			LOG_ERR("Failed to allocate buffer %u (size %zu)", bdesc->id, bdesc->size);
			return -ENOMEM;
		}

		init_buffer_params(buf, SOF_IPC_STREAM_PLAYBACK, bdesc->fmt, 48000, 2);

		s_buffers[s_num_buffers] = buf;
		s_buffer_ids[s_num_buffers] = bdesc->id;
		s_num_buffers++;
	}

	/* 4. Connect Pipeline Graph Routes */
	for (size_t i = 0; i < topo->num_routes; i++) {
		const struct sof_static_route *r = &topo->routes[i];
		struct comp_dev *src = sof_static_comp_get(r->src_comp_id);
		struct comp_buffer *buf = sof_static_buffer_get(r->buffer_id);
		struct comp_dev *sink = sof_static_comp_get(r->sink_comp_id);

		if (!src || !buf || !sink) {
			LOG_ERR("Failed route %u -> [buf %u] -> %u", r->src_comp_id, r->buffer_id, r->sink_comp_id);
			return -EINVAL;
		}

		pipeline_connect(src, buf, PPL_CONN_DIR_COMP_TO_BUFFER);
		pipeline_connect(sink, buf, PPL_CONN_DIR_BUFFER_TO_COMP);
		LOG_DBG("Connected: %u -> [buf %u] -> %u", r->src_comp_id, r->buffer_id, r->sink_comp_id);
	}

	/* 5. Set Pipeline Endpoints & Parameters */
	for (size_t i = 0; i < topo->num_pipelines; i++) {
		const struct sof_static_pipeline_desc *pdesc = &topo->pipelines[i];
		struct pipeline *pipe = sof_static_pipeline_get(pdesc->pipeline_id);
		if (!pipe)
			continue;

		pipe->source_comp = sof_static_comp_get(pdesc->source_comp_id);
		pipe->sink_comp = sof_static_comp_get(pdesc->sink_comp_id);
		pipe->sched_comp = sof_static_comp_get(pdesc->sched_comp_id);

		struct comp_dev *host_or_sched = pipe->sched_comp ? pipe->sched_comp : pipe->source_comp;
		if (host_or_sched) {
			struct sof_ipc_pcm_params prms;
			memset(&prms, 0, sizeof(prms));
			prms.params.rate = 48000;
			prms.params.channels = 2;
			prms.params.frame_fmt = host_or_sched->ipc_config.frame_fmt;
			uint32_t sbytes = (prms.params.frame_fmt == SOF_IPC_FRAME_FLOAT ||
					   prms.params.frame_fmt == SOF_IPC_FRAME_S32_LE) ? 4 : 2;
			prms.params.sample_container_bytes = sbytes;
			prms.params.sample_valid_bytes = sbytes;
			prms.params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
			prms.params.host_period_bytes = 48 * 2 * sbytes;
			prms.comp_id = dev_comp_id(host_or_sched);
			prms.params.direction = pdesc->direction;
			prms.params.chmap[0] = SOF_CHMAP_FL;
			prms.params.chmap[1] = SOF_CHMAP_FR;

			pipeline_params(pipe, host_or_sched, &prms);
			pipeline_prepare(pipe, host_or_sched);
		}
	}

	/* 6. Initialize Kcontrols */
	for (size_t i = 0; i < topo->num_controls && i < MAX_STATIC_CONTROLS; i++) {
		const struct sof_static_kcontrol *ctl = &topo->controls[i];
		s_control_vals[i] = ctl->def;
		LOG_INF("Kcontrol [%u] '%s' (comp %u, type %d, def %d)",
			ctl->id, ctl->name, ctl->target_comp_id, ctl->type, ctl->def);
	}

	LOG_INF("Static audio topology initialized successfully (%zu pipelines, %zu comps, %zu buffers, %zu controls)",
		topo->num_pipelines, topo->num_comps, topo->num_buffers, topo->num_controls);
	return 0;
}

const struct sof_static_topology *sof_static_topology_get(void)
{
	return s_active_topo;
}

struct pipeline *sof_static_pipeline_get(uint32_t pipeline_id)
{
	for (size_t i = 0; i < s_num_pipelines; i++) {
		if (s_pipeline_ids[i] == pipeline_id)
			return s_pipelines[i];
	}
	return NULL;
}

struct comp_dev *sof_static_comp_get(uint32_t comp_id)
{
	for (size_t i = 0; i < s_num_comps; i++) {
		if (s_comp_ids[i] == comp_id)
			return s_comps[i];
	}
	return NULL;
}

struct comp_buffer *sof_static_buffer_get(uint32_t buffer_id)
{
	for (size_t i = 0; i < s_num_buffers; i++) {
		if (s_buffer_ids[i] == buffer_id)
			return s_buffers[i];
	}
	return NULL;
}

int sof_static_kcontrol_set(uint32_t ctrl_id, int32_t val)
{
	if (!s_active_topo)
		return -ENODEV;

	const struct sof_static_kcontrol *ctl = NULL;
	size_t ctl_idx = 0;
	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].id == ctrl_id) {
			ctl = &s_active_topo->controls[i];
			ctl_idx = i;
			break;
		}
	}
	if (!ctl)
		return -ENOENT;

	struct comp_dev *dev = sof_static_comp_get(ctl->target_comp_id);
	if (!dev)
		return -ENODEV;

	struct processing_module *mod = comp_mod(dev);

	switch (ctl->type) {
	case SOF_STATIC_CTRL_VOLUME:
		if (mod) {
			if (dev->drv->uid && !memcmp(dev->drv->uid, &level_multiplier_uuid, UUID_SIZE)) {
				struct level_multiplier_comp_data *cd = module_get_private_data(mod);
				if (cd) {
					cd->gain = val;
#if CONFIG_FORMAT_FLOAT
					cd->gain_f = (float)val / 2147483648.0f;
#endif
				}
			} else {
				for (uint32_t ch = 0; ch < ctl->channels; ch++) {
					volume_set_chan(mod, ch, val, true);
				}
			}
		}
		break;

	case SOF_STATIC_CTRL_SWITCH:
		if (mod) {
			if (dev->drv->uid && !memcmp(dev->drv->uid, &level_multiplier_uuid, UUID_SIZE)) {
				struct level_multiplier_comp_data *cd = module_get_private_data(mod);
				if (cd) {
					if (val == 0) {
						cd->gain = 0;
#if CONFIG_FORMAT_FLOAT
						cd->gain_f = 0.0f;
#endif
					} else {
						cd->gain = LEVEL_MULTIPLIER_GAIN_ONE;
#if CONFIG_FORMAT_FLOAT
						cd->gain_f = 1.0f;
#endif
					}
				}
			} else if (dev->drv->uid && !memcmp(dev->drv->uid, &volume_uuid, UUID_SIZE)) {
				for (uint32_t ch = 0; ch < ctl->channels; ch++) {
					if (val == 0) {
						volume_set_chan_mute(mod, ch);
					} else {
						volume_set_chan_unmute(mod, ch);
					}
				}
			} else if (dev->drv->uid && !memcmp(dev->drv->uid, &eq_iir_uuid, UUID_SIZE)) {
				struct comp_data *cd = module_get_private_data(mod);
				if (cd) {
					if (val == 0) { /* 0 = bypass */
						cd->eq_iir_func = eq_iir_pass;
					} else if (cd->iir_delay_size) {
#if CONFIG_FORMAT_FLOAT
						struct comp_buffer *sourceb = comp_dev_get_first_data_producer(dev);
						if (sourceb && audio_stream_get_frm_fmt(&sourceb->stream) == SOF_IPC_FRAME_FLOAT) {
							cd->eq_iir_func = eq_iir_float_default;
						} else {
							cd->eq_iir_func = eq_iir_s16_default;
						}
#else
						cd->eq_iir_func = eq_iir_s16_default;
#endif
					}
				}
			} else if (dev->drv->uid && !memcmp(dev->drv->uid, &drc_uuid, UUID_SIZE)) {
				struct drc_comp_data *cd = module_get_private_data(mod);
				if (cd) {
					cd->enable_switch = (val != 0);
				}
			} else if (dev->drv->uid && !memcmp(dev->drv->uid, &tdfb_uuid, UUID_SIZE)) {
				struct tdfb_comp_data *cd = module_get_private_data(mod);
				if (cd) {
					cd->beam_on = (val != 0);
				}
			}
		}
		break;

	case SOF_STATIC_CTRL_ENUM:
		if (dev->drv->uid && !memcmp(dev->drv->uid, &selector_uuid, UUID_SIZE)) {
			uint8_t cbuf[sizeof(struct sof_ipc_ctrl_data) + sizeof(struct sof_ipc_ctrl_value_chan)] = {0};
			struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)cbuf;
			cdata->cmd = SOF_CTRL_CMD_ENUM;
			cdata->type = SOF_CTRL_TYPE_VALUE_CHAN_SET;
			cdata->num_elems = 1;
			cdata->chanv[0].channel = 0;
			cdata->chanv[0].value = val;
			comp_cmd(dev, COMP_CMD_SET_VALUE, cdata, sizeof(cbuf));
		}
		break;

	default:
		break;
	}

	s_control_vals[ctl_idx] = val;
	LOG_INF("Kcontrol [%u] '%s' set to %d", ctl->id, ctl->name, val);
	return 0;
}

int sof_static_kcontrol_get(uint32_t ctrl_id, int32_t *val)
{
	if (!s_active_topo || !val)
		return -EINVAL;

	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].id == ctrl_id) {
			*val = s_control_vals[i];
			return 0;
		}
	}
	return -ENOENT;
}

int sof_static_kcontrol_find_by_name(const char *name)
{
	if (!s_active_topo || !name)
		return -EINVAL;

	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		if (s_active_topo->controls[i].name &&
		    strcmp(s_active_topo->controls[i].name, name) == 0) {
			return (int)s_active_topo->controls[i].id;
		}
	}
	return -ENOENT;
}

#define SOF_VOL_ZERO_DB BIT(23)

static int32_t uac2_to_sof_volume(int16_t volume)
{
	if (volume <= -90 * 256)
		return 0;
	if (volume >= 0)
		return SOF_VOL_ZERO_DB - 1;

	int32_t db_x10 = (int32_t)(-volume) * 10 / 256;
	int shift = db_x10 / 60;
	if (shift >= 31)
		return 0;

	int rem = db_x10 % 60;
	uint64_t v = (uint64_t)(SOF_VOL_ZERO_DB - 1) >> shift;
	v = (v * (60 - rem)) / 60;
	return (int32_t)v;
}

int sof_static_kcontrol_set_by_uac2(uint8_t entity_id, uint8_t channel, int32_t val, bool is_volume)
{
	if (!s_active_topo)
		return -ENODEV;

	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		const struct sof_static_kcontrol *ctl = &s_active_topo->controls[i];
		if (ctl->uac2_entity_id == entity_id) {
			if (is_volume && ctl->type == SOF_STATIC_CTRL_VOLUME) {
				int32_t sof_vol = uac2_to_sof_volume((int16_t)val);
				return sof_static_kcontrol_set(ctl->id, sof_vol);
			} else if (!is_volume && ctl->type == SOF_STATIC_CTRL_SWITCH) {
				/* For UAC2 mute: val=1 means muted, so enabled=0 */
				return sof_static_kcontrol_set(ctl->id, val ? 0 : 1);
			}
		}
	}
	return -ENOENT;
}

int sof_static_kcontrol_get_by_uac2(uint8_t entity_id, uint8_t channel, int32_t *val, bool is_volume)
{
	if (!s_active_topo || !val)
		return -EINVAL;

	for (size_t i = 0; i < s_active_topo->num_controls; i++) {
		const struct sof_static_kcontrol *ctl = &s_active_topo->controls[i];
		if (ctl->uac2_entity_id == entity_id) {
			if (is_volume && ctl->type == SOF_STATIC_CTRL_VOLUME) {
				*val = s_control_vals[i];
				return 0;
			} else if (!is_volume && ctl->type == SOF_STATIC_CTRL_SWITCH) {
				/* For UAC2 mute: muted if value == 0 */
				*val = (s_control_vals[i] == 0) ? 1 : 0;
				return 0;
			}
		}
	}
	return -ENOENT;
}

int sof_static_pipeline_trigger_by_uac2_term(uint8_t terminal_id, bool start)
{
	if (!s_active_topo)
		return -ENODEV;

	/* Find comp with terminal_id */
	for (size_t i = 0; i < s_active_topo->num_comps; i++) {
		const struct sof_static_comp *cdesc = &s_active_topo->comps[i];
		if (cdesc->type == SOF_STATIC_COMP_HOST && cdesc->ep.usb.terminal_id == terminal_id) {
			struct pipeline *pipe = sof_static_pipeline_get(cdesc->pipeline_id);
			struct comp_dev *dev = sof_static_comp_get(cdesc->id);
			if (pipe && dev) {
				if (start) {
					if (pipe->status != COMP_STATE_ACTIVE) {
						pipeline_trigger(pipe, dev, COMP_TRIGGER_PRE_START);
						pipeline_trigger(pipe, dev, COMP_TRIGGER_START);
					}
				} else {
					if (pipe->status == COMP_STATE_ACTIVE || pipe->status == COMP_STATE_PAUSED) {
						pipeline_trigger(pipe, dev, COMP_TRIGGER_STOP);
					}
				}
				LOG_INF("Pipeline %u %s via UAC2 terminal %u", cdesc->pipeline_id,
					start ? "STARTED" : "STOPPED", terminal_id);
				return 0;
			}
		}
	}
	return -ENOENT;
}

int sof_static_pipeline_trigger(uint32_t pipeline_id, bool start)
{
	struct pipeline *pipe = sof_static_pipeline_get(pipeline_id);
	if (!pipe)
		return -ENOENT;

	struct comp_dev *dev = pipe->sched_comp ? pipe->sched_comp : pipe->source_comp;
	if (!dev)
		return -ENODEV;

	if (start) {
		if (pipe->status != COMP_STATE_ACTIVE) {
			pipeline_trigger(pipe, dev, COMP_TRIGGER_PRE_START);
			pipeline_trigger(pipe, dev, COMP_TRIGGER_START);
		}
	} else {
		if (pipe->status == COMP_STATE_ACTIVE || pipe->status == COMP_STATE_PAUSED) {
			pipeline_trigger(pipe, dev, COMP_TRIGGER_STOP);
		}
	}
	LOG_INF("Pipeline %u %s", pipeline_id, start ? "STARTED" : "STOPPED");
	return 0;
}

int sof_static_set_sample_rate(uint32_t rate)
{
	s_sample_rate = rate;
	return 0;
}

uint32_t sof_static_get_sample_rate(void)
{
	return s_sample_rate ? s_sample_rate : 48000;
}
