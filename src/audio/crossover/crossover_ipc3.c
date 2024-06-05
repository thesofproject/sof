// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <module/module/base.h>
#include <sof/trace/trace.h>
#include <sof/audio/buffer.h>
#include <ipc/control.h>
#include <sof/common.h>
#include <sof/list.h>
#include <errno.h>

#include "crossover_user.h"
#include "crossover.h"

LOG_MODULE_DECLARE(crossover, CONFIG_SOF_LOG_LEVEL);

int crossover_get_sink_id(struct comp_data *cd, uint32_t pipeline_id, uint32_t index)
{
	return pipeline_id;
}

int crossover_output_pin_init(struct processing_module *mod)
{
	return 0;
}

/**
 * \brief Check sink streams configuration for matching pipeline IDs
 */
int crossover_check_sink_assign(struct processing_module *mod,
				struct sof_crossover_config *config)
{
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	struct list_item *sink_list;
	int num_assigned_sinks = 0;
	uint8_t assigned_sinks[SOF_CROSSOVER_MAX_STREAMS] = {0};
	int i;

	list_for_item(sink_list, &dev->bsink_list) {
		unsigned int pipeline_id;

		sink = container_of(sink_list, struct comp_buffer, source_list);
		pipeline_id = buffer_pipeline_id(sink);

		i = crossover_get_stream_index(mod, config, pipeline_id);
		if (i < 0) {
			comp_warn(dev, "crossover_check_sink_assign(), could not assign sink %d",
				  pipeline_id);
			break;
		}

		if (assigned_sinks[i]) {
			comp_warn(dev, "crossover_check_sink_assign(), multiple sinks from pipeline %d are assigned",
				  pipeline_id);
			break;
		}

		assigned_sinks[i] = true;
		num_assigned_sinks++;
	}

	return num_assigned_sinks;
}

int crossover_check_config(struct processing_module *mod, const uint8_t *fragment)
{
	/* TODO: This check seems to work only for IPC3, FW crash happens from reject from
	 * topology embedded blob.
	 */
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	if (cdata->cmd != SOF_CTRL_CMD_BINARY) {
		comp_err(mod->dev, "crossover_set/get_config(), invalid command");
		return -EINVAL;
	}

	return 0;
}

/**
 * \brief IPC4 specific component prepare, updates source and sink buffers formats from base_cfg
 */
void crossover_params(struct processing_module *mod)
{
}
