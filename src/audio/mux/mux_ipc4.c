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
#include <sof/trace/trace.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <ipc/stream.h>
#include <module/module/base.h>
#include <module/ipc4/base-config.h>
#include <errno.h>

#include "mux.h"

LOG_MODULE_DECLARE(muxdemux, CONFIG_SOF_LOG_LEVEL);

/* 64ce6e35-857a-4878-ace8-e2a2f42e3069 */
SOF_DEFINE_UUID("mux4", mux4_uuid, 0x64ce6e35, 0x857a, 0x4878,
		    0xac, 0xe8, 0xe2, 0xa2, 0xf4, 0x2e, 0x30, 0x69);

DECLARE_TR_CTX(mux_tr, SOF_UUID(mux4_uuid), LOG_LEVEL_INFO);

/* c4b26868-1430-470e-a089-15d1c77f851a */
SOF_DEFINE_UUID("demux", demux_uuid, 0xc4b26868, 0x1430, 0x470e,
		    0xa0, 0x89, 0x15, 0xd1, 0xc7, 0x7f, 0x85, 0x1a);

DECLARE_TR_CTX(demux_tr, SOF_UUID(demux_uuid), LOG_LEVEL_INFO);

static int build_config(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd = module_get_private_data(mod);
	int mask = 1;
	int i;

	dev->ipc_config.type = SOF_COMP_MUX;
	cd->config.num_streams = MUX_MAX_STREAMS;

	/* clear masks */
	for (i = 0; i < cd->config.num_streams; i++)
		memset(cd->config.streams[i].mask, 0, sizeof(cd->config.streams[i].mask));

	/* Setting masks for streams */
	for (i = 0; i < cd->md.base_cfg.audio_fmt.channels_count; i++) {
		cd->config.streams[0].mask[i] = mask;
		mask <<= 1;
	}

	for (i = 0; i < cd->md.reference_format.channels_count; i++) {
		cd->config.streams[1].mask[i] = mask;
		mask <<= 1;
	}

	/* validation of matrix mixing */
	if (mux_mix_check(&cd->config)) {
		comp_err(dev, "build_config(): mux component is not able to mix channels");
		return -EINVAL;
	}
	return 0;
}

/* In ipc4 case param is figured out by module config so we need to first
 * set up param then verify param. BTW for IPC3 path, the param is sent by
 * host driver.
 */
static void set_mux_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink, *source;
	struct list_item *source_list;
	int j;

	params->direction = dev->direction;
	params->channels =  cd->md.base_cfg.audio_fmt.channels_count;
	params->rate = cd->md.base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->md.base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes = cd->md.base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cd->md.base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cd->md.base_cfg.ibs;
	params->no_stream_position = 1;

	/* There are two input pins and one output pin in the mux.
	 * For the first input we assign parameters from base_cfg,
	 * for the second from reference_format
	 * and for sink output_format.
	 */

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

		if (!sink->hw_params_configured) {
			ipc4_update_buffer_format(sink, &cd->md.output_format);
			params->frame_fmt = audio_stream_get_frm_fmt(&sink->stream);
		}
	}

	/* update each source format */
	if (!list_is_empty(&dev->bsource_list)) {
		struct ipc4_audio_format *audio_fmt;

		list_for_item(source_list, &dev->bsource_list)
		{
			source = container_of(source_list, struct comp_buffer, sink_list);
			j = buf_get_id(source);
			cd->config.streams[j].pipeline_id = buffer_pipeline_id(source);
			if (j == BASE_CFG_QUEUED_ID)
				audio_fmt = &cd->md.base_cfg.audio_fmt;
			else
				audio_fmt = &cd->md.reference_format;

			ipc4_update_buffer_format(source, audio_fmt);
		}
	}

	mux_prepare_look_up_table(mod);
}

int mux_params(struct processing_module *mod)
{
	int ret;

	ret = build_config(mod);
	if (ret < 0)
		return ret;

	set_mux_params(mod);

	return ret;
}
#endif /* CONFIG_COMP_MUX */
