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

/* e61bb28d-149a-4c1f-b709-46823ef5f5a3 */
DECLARE_SOF_RT_UUID("src", src_uuid, 0xe61bb28d, 0x149a, 0x4c1f,
		    0xb7, 0x09, 0x46, 0x82, 0x3e, 0xf5, 0xf5, 0xae);

DECLARE_TR_CTX(src_tr, SOF_UUID(src_uuid), LOG_LEVEL_INFO);

LOG_MODULE_DECLARE(src, CONFIG_SOF_LOG_LEVEL);

int src_rate_check(const void *spec)
{
	const struct ipc4_config_src *ipc_src = spec;

	if (ipc_src->base.audio_fmt.sampling_frequency == 0 || ipc_src->sink_rate == 0)
		return -EINVAL;

	return 0;
}

int src_stream_pcm_source_rate_check(struct ipc4_config_src cfg,
				     struct sof_ipc_stream_params *params)
{
	/* Nothing to check */
	return 0;
}

int src_stream_pcm_sink_rate_check(struct ipc4_config_src cfg,
				   struct sof_ipc_stream_params *params)
{
	if (cfg.sink_rate && params->rate != cfg.sink_rate)
		return -EINVAL;

	return 0;
}

/* In ipc4 case param is figured out by module config so we need to first
 * set up param then verify param. BTW for IPC3 path, the param is sent by
 * host driver.
 */
int src_set_params(struct processing_module *mod, struct sof_sink *sink)
{
	struct sof_ipc_stream_params src_params;
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	enum sof_ipc_frame frame_fmt, valid_fmt;
	struct comp_dev *dev = mod->dev;
	int ret;

	src_params = *params;
	src_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	src_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;
	src_params.rate = cd->ipc_config.sink_rate;

	/* Get frame_fmt and valid_fmt */
	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	src_params.frame_fmt = valid_fmt;
	ret = sink_set_params(sink, &src_params, true);

	/* if module is to be run as DP, calculate module period
	 * according to OBS size and data rate
	 * as SRC uses period value to calculate its internal buffers,
	 * it must be done here, right after setting sink parameters
	 */
	if (dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP)
		dev->period = 1000000 * sink_get_min_free_space(sink) /
		      (sink_get_frame_bytes(sink) * sink_get_rate(sink));

	comp_info(dev, "SRC DP period calculated as: %u", dev->period);

	component_set_nearest_period_frames(dev, src_params.rate);
	/* Update module stream_params */
	params->rate = cd->ipc_config.sink_rate;
	return ret;
}

void src_get_source_sink_params(struct comp_dev *dev, struct sof_source *source,
				struct sof_sink *sink)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct comp_data *cd = module_get_private_data(mod);
	enum sof_ipc_frame frame_fmt, valid_fmt;

	/* convert IPC4 config to format used by the module */
	audio_stream_fmt_conversion(cd->ipc_config.base.audio_fmt.depth,
				    cd->ipc_config.base.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    cd->ipc_config.base.audio_fmt.s_type);
	sink_set_frm_fmt(sink, frame_fmt);
	sink_set_valid_fmt(sink, valid_fmt);
	sink_set_channels(sink, cd->ipc_config.base.audio_fmt.channels_count);
	sink_set_buffer_fmt(sink, cd->ipc_config.base.audio_fmt.interleaving_style);
	sink_set_rate(sink, cd->ipc_config.sink_rate);
}

int src_prepare_general(struct processing_module *mod,
			struct sof_source *source,
			struct sof_sink *sink)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int ret = 0;

	/* set align requirements */
	src_set_alignment(source, sink);

	switch (cd->ipc_config.base.audio_fmt.depth) {
#if CONFIG_FORMAT_S16LE
	case IPC4_DEPTH_16BIT:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir_s16;
		break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case IPC4_DEPTH_24BIT:
		cd->data_shift = 8;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case IPC4_DEPTH_32BIT:
		cd->data_shift = 0;
		cd->polyphase_func = src_polyphase_stage_cir;
		break;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(dev, "src_prepare(): Invalid depth %d",
			 cd->ipc_config.base.audio_fmt.depth);
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

	comp_dbg(dev, "src_init(), channels_count = %d, depth = %d",
		 cd->ipc_config.base.audio_fmt.channels_count,
		 cd->ipc_config.base.audio_fmt.depth);
	comp_dbg(dev, "src_init(), sampling frequency = %d, sink rate = %d",
		 cd->ipc_config.base.audio_fmt.sampling_frequency, cd->ipc_config.sink_rate);
	cd->source_rate = cd->ipc_config.base.audio_fmt.sampling_frequency;
	cd->sink_rate = cd->ipc_config.sink_rate;
	cd->channels_count = cd->ipc_config.base.audio_fmt.channels_count;
	switch (cd->ipc_config.base.audio_fmt.depth) {
	case IPC4_DEPTH_16BIT:
		cd->sample_container_bytes = sizeof(int16_t);
		break;
	case IPC4_DEPTH_24BIT:
	case IPC4_DEPTH_32BIT:
		cd->sample_container_bytes = sizeof(int32_t);
		break;
	default:
		comp_err(dev, "src_init(): Illegal sample depth %d",
			 cd->ipc_config.base.audio_fmt.depth);
		rfree(cd);
		return -EINVAL;
	}

	return 0;
}

