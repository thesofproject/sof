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

	const unsigned char *data;
	uint32_t size;

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
	default:
	{
		const struct ipc_config_process *ipc_module_adapter = spec;

		size = ipc_module_adapter->size;
		data = ipc_module_adapter->data;
		break;
	}
	}

	/* Copy initial config */
	if (size) {
		ret = module_load_config(dev, data, size);
		if (ret < 0) {
			comp_err(dev, "module_adapter_new() error %d: config loading has failed.",
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

static int module_source_status_count(struct comp_dev *dev, uint32_t status)
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

		if (source->source && source->source->state == status)
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

		sources_active = module_source_status_count(dev, COMP_STATE_ACTIVE) ||
				 module_source_status_count(dev, COMP_STATE_PAUSED);

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

