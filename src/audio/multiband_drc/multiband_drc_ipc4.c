// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/module_interface.h>
#include <sof/trace/trace.h>
#include <module/module/base.h>
#include <module/module/interface.h>
#include <sof/audio/component.h>
#include <ipc4/header.h>
#include <sof/audio/data_blob.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/audio_stream.h>
#include <sof/lib/memory.h>
#include <sof/list.h>

#include "multiband_drc.h"

LOG_MODULE_DECLARE(multiband_drc, CONFIG_SOF_LOG_LEVEL);

void multiband_drc_process_enable(bool *process_enabled)
{
	*process_enabled = true;
}

__cold int multiband_drc_set_ipc_config(struct processing_module *mod, uint32_t param_id,
					const uint8_t *fragment,
					enum module_cfg_fragment_position pos,
					uint32_t data_offset_size, size_t fragment_size)
{
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_dbg(dev, "SOF_IPC4_SWITCH_CONTROL_PARAM_ID id = %d, num_elems = %d",
			 ctl->id, ctl->num_elems);

		if (ctl->id == 0 && ctl->num_elems == 1) {
			cd->process_enabled = ctl->chanv[0].value;
			comp_info(dev, "process_enabled = %d", cd->process_enabled);
		} else {
			comp_err(dev, "Illegal control id = %d, num_elems = %d",
				 ctl->id, ctl->num_elems);
			return -EINVAL;
		}

		return 0;

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "multiband_drc_set_ipc_config(), illegal control.");
		return -EINVAL;
	}

	comp_dbg(mod->dev, "multiband_drc_set_ipc_config(), SOF_CTRL_CMD_BINARY");
	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

__cold int multiband_drc_get_ipc_config(struct processing_module *mod,
					struct sof_ipc_ctrl_data *cdata, size_t fragment_size)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "multiband_drc_get_ipc_config(), SOF_CTRL_CMD_BINARY");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

__cold int multiband_drc_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params comp_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i, ret;

	comp_dbg(dev, "multiband_drc_params()");

	comp_params = *params;
	comp_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_params.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	comp_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	comp_params.frame_fmt = frame_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		comp_params.chmap[i] = (mod->priv.cfg.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, comp_params.rate);
	sinkb = comp_dev_get_first_data_consumer(dev);
	ret = buffer_set_params(sinkb, &comp_params, true);

	return ret;
}

