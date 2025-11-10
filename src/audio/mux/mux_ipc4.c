// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#if CONFIG_COMP_MUX

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/data_blob.h>
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <ipc/stream.h>
#include <module/module/base.h>
#include <module/ipc4/base-config.h>
#include <errno.h>

#include "mux.h"

LOG_MODULE_DECLARE(muxdemux, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(mux4);

DECLARE_TR_CTX(mux_tr, SOF_UUID(mux4_uuid), LOG_LEVEL_INFO);

SOF_DEFINE_REG_UUID(demux);

DECLARE_TR_CTX(demux_tr, SOF_UUID(demux_uuid), LOG_LEVEL_INFO);

static int build_config(struct processing_module *mod, struct mux_data *cfg)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	int mask = 1;
	int i;

	cd->config.num_streams = MUX_MAX_STREAMS;

	/* clear masks */
	for (i = 0; i < cd->config.num_streams; i++)
		memset(cd->config.streams[i].mask, 0, sizeof(cd->config.streams[i].mask));

	/* Setting masks for streams */
	for (i = 0; i < cfg->base_cfg.audio_fmt.channels_count; i++) {
		cd->config.streams[0].mask[i] = mask;
		mask <<= 1;
	}

	for (i = 0; i < cfg->reference_format.channels_count; i++) {
		cd->config.streams[1].mask[i] = mask;
		mask <<= 1;
	}

	/* validation of matrix mixing */
	if (mux_mix_check(&cd->config)) {
		comp_err(dev, "mux component is not able to mix channels");
		return -EINVAL;
	}
	return 0;
}

/* In ipc4 case param is figured out by module config so we need to first
 * set up param then verify param. BTW for IPC3 path, the param is sent by
 * host driver.
 */
static void set_mux_params(struct processing_module *mod, struct mux_data *cfg)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink, *source;
	int j;

	params->direction = dev->direction;
	params->channels =  cfg->base_cfg.audio_fmt.channels_count;
	params->rate = cfg->base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cfg->base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes = cfg->base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cfg->base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cfg->base_cfg.ibs;
	params->no_stream_position = 1;

	/* There are two input pins and one output pin in the mux.
	 * For the first input we assign parameters from base_cfg,
	 * for the second from reference_format
	 * and for sink output_format.
	 */

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		sink = comp_dev_get_first_data_consumer(dev);

		if (!audio_buffer_hw_params_configured(&sink->audio_buffer)) {
			ipc4_update_buffer_format(sink, &cfg->output_format);
			params->frame_fmt = audio_stream_get_frm_fmt(&sink->stream);
		}
	}

	/* update each source format */
	if (!list_is_empty(&dev->bsource_list)) {
		struct ipc4_audio_format *audio_fmt;

		comp_dev_for_each_producer(dev, source) {
			j = IPC4_SINK_QUEUE_ID(buf_get_id(source));
			cd->config.streams[j].pipeline_id = buffer_pipeline_id(source);
			if (j == BASE_CFG_QUEUED_ID)
				audio_fmt = &cfg->base_cfg.audio_fmt;
			else
				audio_fmt = &cfg->reference_format;

			ipc4_update_buffer_format(source, audio_fmt);
		}
	}

	mux_prepare_look_up_table(mod);
}

int mux_params(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct mux_data *cfg;
	size_t blob_size;
	int ret;

	cfg = comp_get_data_blob(cd->model_handler, &blob_size, NULL);
	if (!cfg || blob_size > MUX_BLOB_MAX_SIZE) {
		comp_err(mod->dev, "illegal blob size %zu", blob_size);
		return -EINVAL;
	}

	ret = build_config(mod, cfg);
	if (ret < 0)
		return ret;

	set_mux_params(mod, cfg);

	return ret;
}
#endif /* CONFIG_COMP_MUX */
