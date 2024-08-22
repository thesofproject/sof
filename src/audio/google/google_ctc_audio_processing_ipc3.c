// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Google LLC.
//
// Author: Eddy Hsu <eddyhsu@google.com>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <rtos/init.h>

#include "google_ctc_audio_processing.h"

LOG_MODULE_DECLARE(google_ctc_audio_processing, CONFIG_SOF_LOG_LEVEL);

int ctc_set_config(struct processing_module *mod, uint32_t param_id,
		   enum module_cfg_fragment_position pos,
		   uint32_t data_offset_size,
		   const uint8_t *fragment,
		   size_t fragment_size, uint8_t *response,
		   size_t response_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret;
	struct google_ctc_config *config;
	int size;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = comp_data_blob_set_cmd(cd->tuning_handler, cdata);
		if (ret)
			return ret;
		if (comp_is_new_data_blob_available(cd->tuning_handler)) {
			config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
			if (size != CTC_BLOB_CONFIG_SIZE) {
				comp_err(mod->dev,
					 "ctc_set_config(): Invalid config size = %d",
					 size);
				return -EINVAL;
			}
			if (config->size != CTC_BLOB_CONFIG_SIZE) {
				comp_err(mod->dev,
					 "ctc_set_config(): Invalid config->size = %d",
					 config->size);
				return -EINVAL;
			}
			cd->reconfigure = true;
		}
		return 0;
	case SOF_CTRL_CMD_SWITCH:
		if (cdata->num_elems == 1) {
			cd->enabled = cdata->chanv[0].value;
			comp_info(dev, "ctc_set_config(), enabled = %d",
				  cd->enabled);
			return 0;
		}
		comp_err(mod->dev,
			 "ctc_set_config(): Illegal num_elems = %d",
			 cdata->num_elems);
		return -EINVAL;
	default:
		comp_err(mod->dev,
			 "ctc_set_config(): Only binary and switch controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}

int ctc_get_config(struct processing_module *mod,
		   uint32_t param_id, uint32_t *data_offset_size,
		   uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "ctc_get_config(): %u", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return comp_data_blob_get_cmd(cd->tuning_handler, cdata, fragment_size);
	case SOF_CTRL_CMD_SWITCH:
		return comp_data_blob_get_cmd(cd->tuning_handler, cdata, fragment_size);
	default:
		return -EINVAL;
	}
}
