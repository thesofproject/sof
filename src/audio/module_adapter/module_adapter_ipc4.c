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
	if (dev->drv->type == SOF_COMP_MODULE_ADAPTER) {
		const struct ipc_config_process *ipc_module_adapter = spec;

		dst->init_data = ipc_module_adapter->data;
		dst->size = ipc_module_adapter->size;
		dst->avail = true;

		memcpy_s(&dst->base_cfg,  sizeof(dst->base_cfg), ipc_module_adapter->data,
			 sizeof(dst->base_cfg));
	} else {
		dst->init_data = spec;
	}

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
	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, mod->stream_params);
}

int module_adapter_set_state(struct processing_module *mod, struct comp_dev *dev,
			     int cmd)
{
	return comp_set_state(dev, cmd);
}

int module_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t data_offset_size, const char *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
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

	if (md->ops->set_configuration)
		return md->ops->set_configuration(mod, param_id, pos, data_offset_size,
						  (const uint8_t *)data,
						  fragment_size, NULL, 0);
	return 0;
}

int module_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
			    bool last_block, uint32_t *data_offset_size, char *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
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

	if (md->ops->get_configuration)
		return md->ops->get_configuration(mod, param_id, data_offset_size,
						  (uint8_t *)data, fragment_size);
	return 0;
}

int module_adapter_get_attribute(struct comp_dev *dev, uint32_t type, void *value)
{
	struct processing_module *mod = comp_get_drvdata(dev);

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

static bool module_adapter_multi_sink_source_check(struct comp_dev *dev)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct list_item *blist;
	int num_sources = 0;
	int num_sinks = 0;

	list_for_item(blist, &dev->bsource_list)
		num_sources++;

	list_for_item(blist, &dev->bsink_list)
		num_sinks++;

	comp_dbg(dev, "num_sources=%d num_sinks=%d", num_sources, num_sinks);

	if (num_sources != 1 || num_sinks != 1)
		return true;

	/* re-assign the source/sink modules */
	mod->sink_comp_buffer = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	mod->source_comp_buffer = list_first_item(&dev->bsource_list,
						  struct comp_buffer, sink_list);

	return false;
}

int module_adapter_bind(struct comp_dev *dev, void *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	int ret;

	ret = module_bind(mod, data);
	if (ret < 0)
		return ret;

	mod->stream_copy_single_to_single = !module_adapter_multi_sink_source_check(dev);

	return 0;
}

int module_adapter_unbind(struct comp_dev *dev, void *data)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	int ret;

	ret = module_unbind(mod, data);
	if (ret < 0)
		return ret;

	mod->stream_copy_single_to_single = !module_adapter_multi_sink_source_check(dev);

	return 0;
}

uint64_t module_adapter_get_total_data_processed(struct comp_dev *dev,
						 uint32_t stream_no, bool input)
{
	struct processing_module *mod = comp_get_drvdata(dev);
	struct module_data *md = &mod->priv;

	if (md->ops->endpoint_ops && md->ops->endpoint_ops->get_total_data_processed)
		return md->ops->endpoint_ops->get_total_data_processed(dev, stream_no, input);

	if (input)
		return mod->total_data_produced;
	else
		return mod->total_data_consumed;
}

