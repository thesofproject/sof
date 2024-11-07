// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

/**
 * \file
 * \brief Module Adapter ipc3: module adapter ipc3 specific code
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
	int ret;

	const unsigned char *data = NULL;
	uint32_t size = 0;

	switch (config->type) {
	case SOF_COMP_VOLUME:
	{
		const struct ipc_config_volume *ipc_volume = spec;

		size = sizeof(*ipc_volume);
		data = spec;
		break;
	}
	case SOF_COMP_SRC:
	{
		const struct ipc_config_src *ipc_src = spec;

		size = sizeof(*ipc_src);
		data = spec;
		break;
	}
	case SOF_COMP_ASRC:
	{
		const struct ipc_config_asrc *ipc_asrc = spec;

		size = sizeof(*ipc_asrc);
		data = spec;
		break;
	}
	case SOF_COMP_MIXER:
		break;
	case SOF_COMP_EQ_IIR:
	case SOF_COMP_EQ_FIR:
	case SOF_COMP_KEYWORD_DETECT:
	case SOF_COMP_KPB:
	case SOF_COMP_SELECTOR:
	case SOF_COMP_DEMUX:
	case SOF_COMP_MUX:
	case SOF_COMP_DCBLOCK:
	case SOF_COMP_SMART_AMP:
	case SOF_COMP_MODULE_ADAPTER:
	case SOF_COMP_FILEREAD:
	case SOF_COMP_FILEWRITE:
	case SOF_COMP_NONE:
	{
		const struct ipc_config_process *ipc_module_adapter = spec;

		size = ipc_module_adapter->size;
		data = ipc_module_adapter->data;
		break;
	}
	default:
		comp_err(dev, "module_adapter_init_data() unsupported comp type %d", config->type);
		return -EINVAL;
	}

	/* Copy initial config */
	if (size) {
		ret = module_load_config(dev, data, size);
		if (ret < 0) {
			comp_err(dev, "module_adapter_init_data() error %d: config loading has failed.",
				 ret);
			return ret;
		}
		dst->init_data = dst->data;
	}

	return 0;
}

void module_adapter_reset_data(struct module_config *dst)
{
}

void module_adapter_check_data(struct processing_module *mod, struct comp_dev *dev,
			       struct comp_buffer *sink)
{
	/* Check if audio stream client has only one source and one sink buffer to use a
	 * simplified copy function.
	 */
	if (IS_PROCESSING_MODE_AUDIO_STREAM(mod) && mod->num_of_sources == 1 &&
	    mod->num_of_sinks == 1) {
		mod->source_comp_buffer = list_first_item(&dev->bsource_list,
							  struct comp_buffer, sink_list);
		mod->sink_comp_buffer = sink;
		mod->stream_copy_single_to_single = true;
	}
}

void module_adapter_set_params(struct processing_module *mod, struct sof_ipc_stream_params *params)
{
}

static int module_source_state_count(struct comp_dev *dev, uint32_t state)
{
	struct list_item *blist;
	int count = 0;

	/* count source with state == status */
	list_for_item(blist, &dev->bsource_list) {
		/*
		 * FIXME: this is racy, state can be changed by another core.
		 * This is implicitly protected by serialised IPCs. Even when
		 * IPCs are processed in the pipeline thread, the next IPC will
		 * not be sent until the thread has processed and replied to the
		 * current one.
		 */
		struct comp_buffer *source = container_of(blist, struct comp_buffer,
							  sink_list);

		if (source->source && source->source->state == state)
			count++;
	}

	return count;
}

int module_adapter_set_state(struct processing_module *mod, struct comp_dev *dev,
			     int cmd)
{
	if (mod->num_of_sources > 1) {
		bool sources_active;
		int ret;

		sources_active = module_source_state_count(dev, COMP_STATE_ACTIVE) ||
				 module_source_state_count(dev, COMP_STATE_PAUSED);

		/* don't stop/start module if one of the sources is active/paused */
		if ((cmd == COMP_TRIGGER_STOP || cmd == COMP_TRIGGER_PRE_START) && sources_active) {
			dev->state = COMP_STATE_ACTIVE;
			return PPL_STATUS_PATH_STOP;
		}

		ret = comp_set_state(dev, cmd);
		if (ret == COMP_STATUS_STATE_ALREADY_SET)
			return PPL_STATUS_PATH_STOP;

		return ret;
	}

	return comp_set_state(dev, cmd);
}

static int module_adapter_get_set_params(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
					 bool set)
{
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;
	enum module_cfg_fragment_position pos;
	uint32_t data_offset_size;
	static uint32_t size;

	comp_dbg(dev, "module_adapter_set_params(): num_of_elem %d, elem remain %d msg_index %u",
		 cdata->num_elems, cdata->elems_remaining, cdata->msg_index);

	/* set the fragment position, data offset and config data size */
	if (!cdata->msg_index) {
		size = cdata->num_elems + cdata->elems_remaining;
		data_offset_size = size;
		if (cdata->elems_remaining)
			pos = MODULE_CFG_FRAGMENT_FIRST;
		else
			pos = MODULE_CFG_FRAGMENT_SINGLE;
	} else {
		data_offset_size = size - (cdata->num_elems + cdata->elems_remaining);
		if (cdata->elems_remaining)
			pos = MODULE_CFG_FRAGMENT_MIDDLE;
		else
			pos = MODULE_CFG_FRAGMENT_LAST;
	}

	if (set) {
		/*
		 * The type member in struct sof_abi_hdr is used for component's specific blob type
		 * for IPC3, just like it is used for component's specific blob param_id for IPC4.
		 */
		if (interface->set_configuration)
			return interface->set_configuration(mod, cdata->data[0].type, pos,
							    data_offset_size,
							    (const uint8_t *)cdata,
							    cdata->num_elems, NULL, 0);

		comp_warn(dev, "module_adapter_get_set_params(): no configuration op set for %d",
			  dev_comp_id(dev));
		return 0;
	}

	if (interface->get_configuration)
		return interface->get_configuration(mod, pos, &data_offset_size,
						    (uint8_t *)cdata, cdata->num_elems);

	comp_err(dev, "module_adapter_get_set_params(): no configuration op get for %d",
		 dev_comp_id(dev));
	return -EIO; /* non-implemented error */
}

static int module_adapter_ctrl_get_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
					    bool set)
{
	int ret;
	struct processing_module __maybe_unused *mod = comp_mod(dev);

	comp_dbg(dev, "module_adapter_ctrl_set_data() start, state %d, cmd %d",
		 mod->priv.state, cdata->cmd);

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "module_adapter_ctrl_set_data(): ABI mismatch!");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_err(dev, "module_adapter_ctrl_set_data(): set enum is not implemented");
		ret = -EIO;
		break;
	case SOF_CTRL_CMD_BINARY:
		ret = module_adapter_get_set_params(dev, cdata, set);
		break;
	default:
		comp_err(dev, "module_adapter_ctrl_set_data error: unknown set data command");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* Used to pass standard and bespoke commands (with data) to component */
int module_adapter_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	struct processing_module *mod = comp_mod(dev);
	const struct module_interface *const interface = mod->dev->drv->adapter_ops;
	int ret = 0;

	comp_dbg(dev, "module_adapter_cmd() %d start", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		ret = module_adapter_ctrl_get_set_data(dev, cdata, true);
		break;
	case COMP_CMD_GET_DATA:
		ret = module_adapter_ctrl_get_set_data(dev, cdata, false);
		break;
	case COMP_CMD_SET_VALUE:
		/*
		 * IPC3 does not use config_id, so pass 0 for config ID as it will be ignored
		 * anyway. Also, pass the 0 as the fragment size as it is not relevant for the
		 * SET_VALUE command.
		 */
		if (interface->set_configuration)
			ret = interface->set_configuration(mod, 0,
							   MODULE_CFG_FRAGMENT_SINGLE, 0,
							   (const uint8_t *)cdata, 0, NULL,
							   0);
		break;
	case COMP_CMD_GET_VALUE:
		/*
		 * Return error if getter is not implemented. Otherwise, the host will suppose
		 * the GET_VALUE command is successful, but the received cdata is not filled.
		 */
		ret = -EIO;

		/*
		 * IPC3 does not use config_id, so pass 0 for config ID as it will be ignored
		 * anyway. Also, pass the 0 as the fragment size and data offset as they are not
		 * relevant for the GET_VALUE command.
		 */
		if (interface->get_configuration)
			ret = interface->get_configuration(mod, 0, 0, (uint8_t *)cdata, 0);
		break;
	default:
		comp_err(dev, "module_adapter_cmd() error: unknown command");
		ret = -EINVAL;
		break;
	}

	comp_dbg(dev, "module_adapter_cmd() done");
	return ret;
}

int module_adapter_sink_src_prepare(struct comp_dev *dev)
{
	struct processing_module *mod = comp_mod(dev);
	struct list_item *blist;
	int ret;
	int i;

	/* acquire all sink and source buffers, get handlers to sink/source API */
	i = 0;
	list_for_item(blist, &dev->bsink_list) {
		struct comp_buffer *sink_buffer =
				container_of(blist, struct comp_buffer, source_list);
		mod->sinks[i] = audio_buffer_get_sink(&sink_buffer->audio_buffer);
		i++;
	}
	mod->num_of_sinks = i;

	i = 0;
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source_buffer =
				container_of(blist, struct comp_buffer, sink_list);

		mod->sources[i] = audio_buffer_get_source(&source_buffer->audio_buffer);
		i++;
	}
	mod->num_of_sources = i;

	/* Prepare module */
	ret = module_prepare(mod, mod->sources, mod->num_of_sources, mod->sinks, mod->num_of_sinks);

	return ret;
}
