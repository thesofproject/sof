// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/common.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_source_utils.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "src.h"
#include "src_config.h"

/* c1c5326d-8390-46b4-aa47-95c3beca6550 */
DECLARE_SOF_RT_UUID("src", src_uuid, 0xc1c5326d, 0x8390, 0x46b4,
		    0xaa, 0x47, 0x95, 0xc3, 0xbe, 0xca, 0x65, 0x50);

DECLARE_TR_CTX(src_tr, SOF_UUID(src_uuid), LOG_LEVEL_INFO);

LOG_MODULE_DECLARE(src, CONFIG_SOF_LOG_LEVEL);

int src_rate_check(const void *spec)
{
	const struct ipc_config_src *ipc_src = spec;

	if (ipc_src->source_rate == 0 && ipc_src->sink_rate == 0)
		return -EINVAL;

	return 0;
}

int src_stream_pcm_sink_rate_check(struct ipc_config_src cfg,
				   struct sof_ipc_stream_params *params)
{
	/* In playback, module adapter mod->stream_params from prepare() is for sink side */
	if (cfg.sink_rate && params->rate != cfg.sink_rate)
		return -EINVAL;

	return 0;
}

int src_stream_pcm_source_rate_check(struct ipc_config_src cfg,
				     struct sof_ipc_stream_params *params)
{
	/* In capture, module adapter mod->stream_params from prepare() is for source side */
	if (cfg.source_rate && params->rate != cfg.source_rate)
		return -EINVAL;

	return 0;
}

int src_set_params(struct processing_module *mod, struct sof_sink *sink)
{
	return 0;
}

void src_get_source_sink_params(struct comp_dev *dev, struct sof_source *source,
				struct sof_sink *sink)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_data *cd = module_get_private_data(mod);

	/* Set source/sink_rate/frames */
	cd->channels_count = source_get_channels(source);
	cd->source_rate = source_get_rate(source);
	cd->sink_rate = sink_get_rate(sink);
	cd->sample_container_bytes = mod->stream_params->sample_container_bytes;
}

int src_prepare_general(struct processing_module *mod,
			struct sof_source *source,
			struct sof_sink *sink)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret = 0;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;

	/* set align requirements */
	src_set_alignment(source, sink);

	/* get source/sink data format */
	source_format = source_get_frm_fmt(source);
	sink_format = sink_get_frm_fmt(sink);

	/* SRC supports S16_LE, S24_4LE and S32_LE formats */
	if (source_format != sink_format) {
		comp_err(dev, "src_prepare(): Source fmt %d and sink fmt %d are different.",
			 source_format, sink_format);
		ret = -EINVAL;
		goto out;
	}

	switch (source_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		cd->data_shift = 8;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "src_prepare(): invalid format %d", source_format);
		ret = -EINVAL;
		goto out;
	}

out:
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

int src_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct module_config *cfg = &md->cfg;
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = NULL;

	comp_dbg(dev, "src_init()");

	if (dev->ipc_config.type != SOF_COMP_SRC) {
		comp_err(dev, "src_init(): Wrong IPC config type %u",
			 dev->ipc_config.type);
		return -EINVAL;
	}

	if (!cfg->init_data || cfg->size != sizeof(cd->ipc_config)) {
		comp_err(dev, "src_init(): Missing or bad size (%u) init data",
			 cfg->size);
		return -EINVAL;
	}

	/* validate init data - either SRC sink or source rate must be set */
	if (src_rate_check(cfg->init_data) < 0) {
		comp_err(dev, "src_init(): SRC sink and source rate are not set");
		return -EINVAL;
	}

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;
	memcpy_s(&cd->ipc_config, sizeof(cd->ipc_config), cfg->init_data, sizeof(cd->ipc_config));

	cd->delay_lines = NULL;
	cd->src_func = src_fallback;
	cd->polyphase_func = NULL;
	src_polyphase_reset(&cd->src);

	mod->verify_params_flags = BUFF_PARAMS_RATE;

	return 0;
}

