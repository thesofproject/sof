// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

/**
 * \file
 * \brief Module Adapter ipc4: module adapter ipc4 specific code
 * \author Baofeng Tian <baofeng.tian@intel.com>
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/platform.h>
#include <sof/ut.h>
#include <rtos/interrupt.h>
#include <rtos/symbol.h>
#include <limits.h>
#include <stdint.h>

LOG_MODULE_DECLARE(module_adapter, CONFIG_SOF_LOG_LEVEL);

/*
 * \module adapter data initialize.
 * \param[in] dev - device.
 * \param[in] config - component ipc descriptor pointer.
 * \param[in] dst - module adapter config data.
 * \param[in] spec - passdowned data from driver.
 *
 * \return: 0 - no error; < 0, error happened.
 */
int module_adapter_init_data(struct comp_dev *dev,
			     struct module_config *dst,
			     const struct comp_ipc_config *config,
			     const void *spec)
{
	const struct ipc_config_process *args = spec;
	const struct ipc4_base_module_extended_cfg *cfg = (void *)args->data;
	size_t cfgsz = args->size;

	assert(dev->drv->type == SOF_COMP_MODULE_ADAPTER);
	if (cfgsz < sizeof(cfg->base_cfg))
		return -EINVAL;

	dst->base_cfg = cfg->base_cfg;
	dst->size = cfgsz;

	if (cfgsz >= sizeof(*cfg)) {
		int n_in = cfg->base_cfg_ext.nb_input_pins;
		int n_out = cfg->base_cfg_ext.nb_output_pins;
		size_t pinsz = (n_in * sizeof(*dst->input_pins))
			     + (n_out * sizeof(*dst->output_pins));

		if (cfgsz == (sizeof(*cfg) + pinsz)) {
			dst->nb_input_pins = n_in;
			dst->nb_output_pins = n_out;
			dst->input_pins = rmalloc(SOF_MEM_ZONE_RUNTIME_SHARED,
						  0, SOF_MEM_CAPS_RAM, pinsz);
			if (!dst->input_pins)
				return -ENOMEM;

			dst->output_pins = (void *)&dst->input_pins[n_in];
			memcpy_s(dst->input_pins, pinsz,
				 &cfg->base_cfg_ext.pin_formats[0], pinsz);
		}
	}

	dst->init_data = cfg; /* legacy API */
	dst->avail = true;
	return 0;
}

void module_adapter_reset_data(struct module_config *dst)
{
	dst->init_data = NULL;
}

void module_adapter_check_data(struct processing_module *mod, struct comp_dev *dev,
			       struct comp_buffer *sink)
{
}

void module_adapter_set_params(struct processing_module *mod, struct sof_ipc_stream_params *params)
{
	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
}

int module_adapter_set_state(struct processing_module *mod, struct comp_dev *dev,
			     int cmd)
{
	return comp_set_state(dev, cmd);
}

int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset_size, const char *data)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;
	struct module_data *md = &mod->priv;
	enum module_cfg_fragment_position pos;
	size_t fragment_size;

	/* set fragment position */
	pos = first_last_block_to_frag_pos(first_block, last_block);

	switch (pos) {
	case MODULE_CFG_FRAGMENT_SINGLE:
		fragment_size = data_offset_size;
		break;
	case MODULE_CFG_FRAGMENT_MIDDLE:
		fragment_size = MAILBOX_DSPBOX_SIZE;
		break;
	case MODULE_CFG_FRAGMENT_FIRST:
		md->new_cfg_size = data_offset_size;
		fragment_size = MAILBOX_DSPBOX_SIZE;
		break;
	case MODULE_CFG_FRAGMENT_LAST:
		fragment_size = md->new_cfg_size - data_offset_size;
		break;
	default:
		comp_err(dev, "module_set_large_config(): invalid fragment position");
		return -EINVAL;
	}

	if (interface->set_configuration)
		return interface->set_configuration(mod, param_id, pos, data_offset_size,
						    (const uint8_t *)data, fragment_size,
						    NULL, 0);
	return 0;
}
EXPORT_SYMBOL(module_set_large_config);

int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset_size, char *data)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;
	struct module_data *md = &mod->priv;
	size_t fragment_size;

	/* set fragment size */
	if (first_block) {
		if (last_block)
			fragment_size = md->cfg.size;
		else
			fragment_size = SOF_IPC_MSG_MAX_SIZE;
	} else {
		if (!last_block)
			fragment_size = SOF_IPC_MSG_MAX_SIZE;
		else
			fragment_size = md->cfg.size - *data_offset_size;
	}

	if (interface->get_configuration)
		return interface->get_configuration(mod, param_id, data_offset_size,
						    (uint8_t *)data, fragment_size);
	/*
	 * Return error if getter is not implemented. Otherwise, the host will suppose
	 * the GET_VALUE command is successful, but the received cdata is not filled.
	 */
	return -EIO;
}
EXPORT_SYMBOL(module_get_large_config);

int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct processing_module *mod = comp_mod(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		memcpy_s(value, sizeof(struct ipc4_base_module_cfg),
			 &mod->priv.cfg.base_cfg, sizeof(mod->priv.cfg.base_cfg));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(module_adapter_get_attribute);

static bool module_adapter_multi_sink_source_prepare(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	struct comp_buffer *sink_buffer;
	struct comp_buffer *source_buffer;
	int i;

	/* acquire all sink and source buffers, get handlers to sink/source API */
	i = 0;
	comp_dev_for_each_consumer(dev, sink_buffer) {
		mod->sinks[i] = audio_buffer_get_sink(&sink_buffer->audio_buffer);
		i++;
	}
	mod->num_of_sinks = i;

	i = 0;
	comp_dev_for_each_producer(dev, source_buffer) {
		mod->sources[i] = audio_buffer_get_source(&source_buffer->audio_buffer);
		i++;
	}
	mod->num_of_sources = i;

	comp_dbg(dev, "num_sources=%d num_sinks=%d", mod->num_of_sinks, mod->num_of_sources);

	if (mod->num_of_sinks != 1 || mod->num_of_sources != 1)
		return true;

	/* re-assign the source/sink modules */
	mod->sink_comp_buffer = comp_dev_get_first_data_consumer(dev);;
	mod->source_comp_buffer = comp_dev_get_first_data_producer(dev);

	return false;
}

int module_adapter_bind(struct comp_dev *dev, void *data)
{
	struct processing_module *mod = comp_mod(dev);
	int ret;

	ret = module_bind(mod, data);
	if (ret < 0)
		return ret;

	mod->stream_copy_single_to_single = !module_adapter_multi_sink_source_prepare(dev);

	return 0;
}
EXPORT_SYMBOL(module_adapter_bind);

int module_adapter_unbind(struct comp_dev *dev, void *data)
{
	struct processing_module *mod = comp_mod(dev);
	int ret;

	ret = module_unbind(mod, data);
	if (ret < 0)
		return ret;

	mod->stream_copy_single_to_single = !module_adapter_multi_sink_source_prepare(dev);

	return 0;
}
EXPORT_SYMBOL(module_adapter_unbind);

uint64_t module_adapter_get_total_data_processed(struct comp_dev *dev,
						 uint32_t stream_no, bool input)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;

	if (interface->endpoint_ops && interface->endpoint_ops->get_total_data_processed)
		return interface->endpoint_ops->get_total_data_processed(dev, stream_no, input);

	if (input)
		return mod->total_data_produced;
	else
		return mod->total_data_consumed;
}
EXPORT_SYMBOL(module_adapter_get_total_data_processed);

int module_adapter_sink_src_prepare(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);

	/* Prepare module */
	return module_prepare(mod, mod->sources, mod->num_of_sources,
			      mod->sinks, mod->num_of_sinks);
}
