// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/trace/trace.h>
#include <module/module/base.h>
#include <ipc/control.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <module/module/interface.h>
#include <ipc/stream.h>
#include <sof/audio/buffer.h>
#include <module/ipc4/base-config.h>
#include <sof/list.h>
#include <errno.h>

#include "dcblock.h"

LOG_MODULE_DECLARE(dcblock, CONFIG_SOF_LOG_LEVEL);

/**
 * \brief Handles incoming get commands for DC Blocking Filter component.
 */
int dcblock_get_ipc_config(struct processing_module *mod,
			   uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "dcblock_get_ipc_config()");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

/**
 * \brief Handles incoming set commands for DC Blocking Filter component.
 */
int dcblock_set_ipc_config(struct processing_module *mod,
			   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			   const uint8_t *fragment, size_t fragment_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "dcblock_set_ipc_config()");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

void dcblock_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "dcblock_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, Xsource_list);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, Xsink_list);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);
}

