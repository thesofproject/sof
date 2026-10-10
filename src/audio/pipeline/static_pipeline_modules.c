// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

/**
 * \file audio/pipeline/static_pipeline_modules.c
 * \brief Generic Static Module Operations Registry & Infrastructure
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Implements generic module operations registration, lookup, and default
 * component initialization helpers for declarative static pipelines.
 * This file contains purely generic infrastructure with zero per-module logic.
 */

#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <ipc/topology.h>
#include <ipc/control.h>
#include <module/ipc4/base-config.h>
#include <rtos/alloc.h>
#include <string.h>
#include <errno.h>

#include "static_pipeline_modules.h"

LOG_MODULE_REGISTER(static_pipeline_modules, CONFIG_SOF_LOG_LEVEL);

/* =========================================================================
 * Static Module Operations Registry
 * ========================================================================= */

static struct list_item s_module_ops_list = LIST_INIT(s_module_ops_list);

/**
 * \brief Register static module operations for an audio component.
 * \param[in,out] ops Pointer to module operations structure.
 * \return 0 on success, negative error code on failure.
 */
int sof_static_register_module_ops(struct sof_static_module_ops *ops)
{
	if (!ops) {
		LOG_ERR("sof_static_register_module_ops: NULL ops pointer");
		return -EINVAL;
	}
	if (!ops->uuid) {
		LOG_ERR("sof_static_register_module_ops: NULL ops UUID");
		return -EINVAL;
	}

	list_item_append(&ops->list, &s_module_ops_list);
	return 0;
}

/**
 * \brief Find registered static module operations by component UUID.
 * \param[in] uuid UUID of target component.
 * \return Pointer to registered ops, or NULL if not found or on error.
 */
const struct sof_static_module_ops *sof_static_find_module_ops(const struct sof_uuid *uuid)
{
	if (!uuid) {
		LOG_ERR("sof_static_find_module_ops: NULL UUID pointer");
		return NULL;
	}

	struct list_item *item;
	list_for_item(item, &s_module_ops_list) {
		struct sof_static_module_ops *ops =
			container_of(item, struct sof_static_module_ops, list);
		if (!memcmp(ops->uuid, uuid, UUID_SIZE))
			return ops;
	}

	LOG_ERR("sof_static_find_module_ops: no ops registered for requested UUID");
	return NULL;
}

/* =========================================================================
 * Base IPC4 Configuration & Default Component Creation
 * ========================================================================= */

#if CONFIG_IPC4_BASE_CONFIG || CONFIG_IPC_MAJOR_4

/* Channel map slot macro: channel index 'ch' placed at slot index 'slot' */
#define SOF_CH_MAP_SLOT(slot, ch)	(((uint32_t)((ch) & 0xf)) << ((slot) * 4))

#define SOF_CH_MAP_1CH(c0) \
	(0xfffffff0 | SOF_CH_MAP_SLOT(0, (c0)))

#define SOF_CH_MAP_2CH(c0, c1) \
	(0xffffff00 | SOF_CH_MAP_SLOT(0, (c0)) | SOF_CH_MAP_SLOT(1, (c1)))

#define SOF_CH_MAP_3CH(c0, c1, c2) \
	(0xfffff000 | SOF_CH_MAP_SLOT(0, (c0)) | SOF_CH_MAP_SLOT(1, (c1)) | \
	 SOF_CH_MAP_SLOT(2, (c2)))

#define SOF_CH_MAP_4CH(c0, c1, c2, c3) \
	(0xffff0000 | SOF_CH_MAP_SLOT(0, (c0)) | SOF_CH_MAP_SLOT(1, (c1)) | \
	 SOF_CH_MAP_SLOT(2, (c2)) | SOF_CH_MAP_SLOT(3, (c3)))

#define SOF_CH_MAP_6CH(c0, c1, c2, c3, c4, c5) \
	(0xff000000 | SOF_CH_MAP_SLOT(0, (c0)) | SOF_CH_MAP_SLOT(1, (c1)) | \
	 SOF_CH_MAP_SLOT(2, (c2)) | SOF_CH_MAP_SLOT(3, (c3)) | \
	 SOF_CH_MAP_SLOT(4, (c4)) | SOF_CH_MAP_SLOT(5, (c5)))

#define SOF_CH_MAP_8CH(c0, c1, c2, c3, c4, c5, c6, c7) \
	(SOF_CH_MAP_SLOT(0, (c0)) | SOF_CH_MAP_SLOT(1, (c1)) | \
	 SOF_CH_MAP_SLOT(2, (c2)) | SOF_CH_MAP_SLOT(3, (c3)) | \
	 SOF_CH_MAP_SLOT(4, (c4)) | SOF_CH_MAP_SLOT(5, (c5)) | \
	 SOF_CH_MAP_SLOT(6, (c6)) | SOF_CH_MAP_SLOT(7, (c7)))

void sof_static_init_base_cfg(struct ipc4_base_module_cfg *base_cfg,
			      const struct sof_static_comp *cdesc,
			      uint32_t period_us)
{
	if (!base_cfg || !cdesc) {
		LOG_ERR("sof_static_init_base_cfg: NULL parameter");
		return;
	}

	memset(base_cfg, 0, sizeof(*base_cfg));

	uint32_t rate = cdesc->caps.default_rate ? cdesc->caps.default_rate : 48000;
	uint16_t channels = cdesc->caps.max_channels ? cdesc->caps.max_channels : 2;
	enum sof_ipc_frame fmt = cdesc->caps.default_fmt ? cdesc->caps.default_fmt : SOF_IPC_FRAME_S16_LE;

	uint32_t cont_bytes = get_sample_bytes(fmt);
	uint32_t valid_bits = get_sample_bitdepth(fmt);
	uint32_t frames = (rate * (period_us ? period_us : 1000)) / 1000000;

	/*
	 * Fallback assumption: if integer division resulted in zero frames
	 * (e.g. invalid rate or ultra-low scheduling period), default to 48 frames,
	 * representing a standard 1 ms period at 48 kHz (48000 * 1000 / 1000000 = 48).
	 * Non-zero buffer size is required by IPC4 module infrastructure.
	 */
	if (frames == 0)
		frames = 48;

	base_cfg->cpc = 0;
	base_cfg->is_pages = 1;
	base_cfg->ibs = frames * channels * cont_bytes;

	/* If sink_rate is specified (e.g. for SRC / ASRC), compute OBS from sink rate */
	if (cdesc->caps.sink_rate) {
		uint32_t out_frames = (cdesc->caps.sink_rate * (period_us ? period_us : 1000)) / 1000000;
		/* Fallback assumption: same 48-frame minimum for sink buffer */
		if (out_frames == 0)
			out_frames = 48;
		base_cfg->obs = out_frames * channels * cont_bytes;
	} else {
		base_cfg->obs = base_cfg->ibs;
	}

	base_cfg->audio_fmt.sampling_frequency = rate;
	base_cfg->audio_fmt.depth = (cont_bytes * 8);
	base_cfg->audio_fmt.valid_bit_depth = valid_bits;
	base_cfg->audio_fmt.channels_count = channels;
	base_cfg->audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	if (fmt == SOF_IPC_FRAME_FLOAT)
		base_cfg->audio_fmt.s_type = IPC4_TYPE_FLOAT;
	else if (fmt == SOF_IPC_FRAME_A_LAW)
		base_cfg->audio_fmt.s_type = IPC4_TYPE_A_LAW;
	else if (fmt == SOF_IPC_FRAME_MU_LAW)
		base_cfg->audio_fmt.s_type = IPC4_TYPE_MU_LAW;
	else
		base_cfg->audio_fmt.s_type = IPC4_TYPE_SIGNED_INTEGER;

	switch (channels) {
	case 1:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_MONO;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_1CH(CHANNEL_CENTER);
		break;
	case 2:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_STEREO;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_2CH(CHANNEL_LEFT, CHANNEL_RIGHT);
		break;
	case 3:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_3_POINT_0;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_3CH(CHANNEL_LEFT, CHANNEL_CENTER,
							     CHANNEL_RIGHT);
		break;
	case 4:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_QUATRO;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_4CH(CHANNEL_LEFT, CHANNEL_RIGHT,
							     CHANNEL_LEFT_SURROUND,
							     CHANNEL_RIGHT_SURROUND);
		break;
	case 6:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_5_POINT_1;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_6CH(CHANNEL_LEFT, CHANNEL_CENTER,
							     CHANNEL_RIGHT, CHANNEL_LEFT_SURROUND,
							     CHANNEL_RIGHT_SURROUND, CHANNEL_LFE);
		break;
	case 8:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_7_POINT_1;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_8CH(CHANNEL_LEFT, CHANNEL_CENTER,
							     CHANNEL_RIGHT, CHANNEL_LEFT_SURROUND,
							     CHANNEL_RIGHT_SURROUND, CHANNEL_LFE,
							     CHANNEL_LEFT_SIDE, CHANNEL_RIGHT_SIDE);
		break;
	default:
		base_cfg->audio_fmt.ch_cfg = IPC4_CHANNEL_CONFIG_STEREO;
		base_cfg->audio_fmt.ch_map = SOF_CH_MAP_2CH(CHANNEL_LEFT, CHANNEL_RIGHT);
		break;
	}
}
#endif

struct comp_dev *sof_static_module_create_default(const struct comp_driver *drv,
						  struct comp_ipc_config *cfg,
						  const struct sof_static_comp *cdesc,
						  uint32_t period_us)
{
	if (!drv || !cfg || !cdesc) {
		LOG_ERR("sof_static_module_create_default: NULL parameter");
		return NULL;
	}

#if CONFIG_IPC4_BASE_CONFIG || CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
	sof_static_init_base_cfg(&base_cfg, cdesc, period_us);
	struct ipc_config_process base_proc_spec = {
		.size = sizeof(base_cfg),
		.data = (const uint8_t *)&base_cfg,
	};
	return drv->ops.create(drv, cfg, &base_proc_spec);
#else
	static const uint8_t dummy_buf[16] = {0};
	struct ipc_config_process empty_proc_spec = {
		.size = 0,
		.data = dummy_buf,
	};
	return drv->ops.create(drv, cfg, &empty_proc_spec);
#endif
}
