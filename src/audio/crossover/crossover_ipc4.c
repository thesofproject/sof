// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Sebastiano Carlucci <scarlucci@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <module/ipc4/base-config.h>
#include <sof/audio/component.h>
#include <module/module/base.h>
#include <sof/trace/trace.h>
#include <sof/audio/buffer.h>
#include <sof/common.h>
#include <sof/list.h>
#include <ipc/stream.h>
#include <errno.h>

#include "crossover_user.h"
#include "crossover.h"

LOG_MODULE_DECLARE(crossover, CONFIG_SOF_LOG_LEVEL);

int crossover_get_sink_id(struct comp_data *cd, uint32_t pipeline_id, uint32_t index)
{
	return cd->output_pin_index[index];
}

/* Note: Crossover needs to have in the rimage manifest the init_config set to 1 to let
 * kernel know that the base_cfg_ext needs to be appended to the IPC payload. The
 * Extension is needed to know the output pin indices.
 */
int crossover_init_output_pins(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const struct ipc4_base_module_extended_cfg *base_cfg = mod->priv.cfg.init_data;
	uint16_t num_input_pins = base_cfg->base_cfg_ext.nb_input_pins;
	uint16_t num_output_pins = base_cfg->base_cfg_ext.nb_output_pins;
	struct ipc4_input_pin_format *input_pin;
	struct ipc4_output_pin_format *output_pin;
	int i;

	comp_dbg(dev, "Number of input pins %u, output pins %u", num_input_pins, num_output_pins);

	if (num_input_pins != 1 || num_output_pins > SOF_CROSSOVER_MAX_STREAMS) {
		comp_err(dev, "Illegal number of pins %u %u", num_input_pins, num_output_pins);
		return -EINVAL;
	}

	input_pin = (struct ipc4_input_pin_format *)base_cfg->base_cfg_ext.pin_formats;
	output_pin = (struct ipc4_output_pin_format *)(input_pin + 1);
	cd->num_output_pins = num_output_pins;
	comp_dbg(dev, "input pin index = %u", input_pin->pin_index);
	for (i = 0; i < num_output_pins; i++) {
		comp_dbg(dev, "output pin %d index = %u", i, output_pin[i].pin_index);
		cd->output_pin_index[i] = output_pin[i].pin_index;
	}

	return 0;
}

int crossover_output_pin_init(struct processing_module *mod)
{
	return crossover_init_output_pins(mod);
}

/**
 * \brief Check sink streams configuration for matching pin index for output pins
 */
int crossover_check_sink_assign(struct processing_module *mod,
				struct sof_crossover_config *config)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t pin_index;
	int num_assigned_sinks = 0;
	int i, j;
	uint8_t assigned_sinks[SOF_CROSSOVER_MAX_STREAMS] = {0};

	for (j = 0; j < cd->num_output_pins; j++) {
		pin_index = cd->output_pin_index[j];
		i = crossover_get_stream_index(mod, config, pin_index);
		if (i < 0) {
			comp_warn(dev, "crossover_check_sink_assign(), could not assign sink %u",
				  pin_index);
			break;
		}

		if (assigned_sinks[i]) {
			comp_warn(dev, "crossover_check_sink_assign(), multiple sinks from pin %u are assigned",
				  pin_index);
			break;
		}

		assigned_sinks[i] = true;
		num_assigned_sinks++;
	}

	return num_assigned_sinks;
}

int crossover_check_config(struct processing_module *mod, const uint8_t *fragment)
{
	return 0;
}

/**
 * \brief IPC4 specific component prepare, updates source and sink buffers formats from base_cfg
 */
void crossover_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "crossover_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);

	comp_dev_for_each_consumer(dev, sinkb) {
		ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);
	}
}
