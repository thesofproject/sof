/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/sof/audio/component_ext.h
 * \brief Component API for Infrastructure
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_COMPONENT_INT_H__
#define __SOF_AUDIO_COMPONENT_INT_H__

#include <sof/audio/component.h>
#include <sof/drivers/idc.h>
#include <sof/list.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <stdbool.h>

/** \addtogroup component_api_helpers Component Mgmt API
 *  @{
 */

/** \brief Holds list of registered components' drivers */
struct comp_driver_list {
	struct list_item list;	/**< list of component drivers */
};

/** \brief Retrieves the component device buffer list. */
#define comp_buffer_list(comp, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &comp->bsink_list : \
	 &comp->bsource_list)

/** See comp_ops::new */
struct comp_dev *comp_new(struct sof_ipc_comp *comp);

/** See comp_ops::free */
static inline void comp_free(struct comp_dev *dev)
{
	assert(dev->drv->ops.free);

	/* free task if shared component */
	if (dev->is_shared && dev->task) {
		schedule_task_free(dev->task);
		rfree(dev->task);
	}

	dev->drv->ops.free(dev);
}

/**
 * Commits component's memory if it's shared.
 * @param dev Component device.
 */
static inline void comp_shared_commit(struct comp_dev *dev)
{
}

/**
 * Parameter init for component on other core.
 * @param dev Component device.
 * @param params Parameters to be set.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_params_remote(struct comp_dev *dev,
				     struct sof_ipc_stream_params *params)
{
	struct idc_msg msg = { IDC_MSG_PARAMS, IDC_MSG_PARAMS_EXT(dev->comp.id),
		dev->comp.core, sizeof(*params), params, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/** See comp_ops::params */
static inline int comp_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	int ret = 0;

	if (dev->is_shared && !cpu_is_me(dev->comp.core)) {
		ret = comp_params_remote(dev, params);
	} else {
		if (dev->drv->ops.params) {
			ret = dev->drv->ops.params(dev, params);
		} else {
			/* not defined, run the default handler */
			ret = comp_verify_params(dev, 0, params);
			if (ret < 0)
				comp_err(dev, "pcm params verification failed");
		}
	}

	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::dai_get_hw_params */
static inline int comp_dai_get_hw_params(struct comp_dev *dev,
					 struct sof_ipc_stream_params *params,
					 int dir)
{
	int ret = -EINVAL;

	if (dev->drv->ops.dai_get_hw_params)
		ret = dev->drv->ops.dai_get_hw_params(dev, params, dir);

	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::cmd */
static inline int comp_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);
	int ret = -EINVAL;

	if (cmd == COMP_CMD_SET_DATA &&
	    (cdata->data->magic != SOF_ABI_MAGIC ||
	     SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi))) {
		comp_err(dev, "comp_cmd(): invalid version, data->magic = %u, data->abi = %u",
			 cdata->data->magic, cdata->data->abi);
		goto out;
	}

	if (dev->drv->ops.cmd)
		ret = dev->drv->ops.cmd(dev, cmd, data, max_data_size);

out:
	comp_shared_commit(dev);

	return ret;
}

/**
 * Runs comp_ops::trigger on the core the target component is assigned to.
 */
static inline int comp_trigger_remote(struct comp_dev *dev, int cmd)
{
	struct idc_msg msg = { IDC_MSG_TRIGGER,
		IDC_MSG_TRIGGER_EXT(dev->comp.id), dev->comp.core, sizeof(cmd),
		&cmd, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/** See comp_ops::trigger */
static inline int comp_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	assert(dev->drv->ops.trigger);

	ret = (dev->is_shared && !cpu_is_me(dev->comp.core)) ?
		comp_trigger_remote(dev, cmd) : dev->drv->ops.trigger(dev, cmd);

	comp_shared_commit(dev);

	return ret;
}

/** Runs comp_ops::prepare on the target component's core */
static inline int comp_prepare_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_PREPARE,
		IDC_MSG_PREPARE_EXT(dev->comp.id), dev->comp.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/** See comp_ops::prepare */
static inline int comp_prepare(struct comp_dev *dev)
{
	int ret = 0;

	if (dev->drv->ops.prepare)
		ret = (dev->is_shared && !cpu_is_me(dev->comp.core)) ?
			comp_prepare_remote(dev) : dev->drv->ops.prepare(dev);

	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::copy */
static inline int comp_copy(struct comp_dev *dev)
{
	int ret = 0;

	assert(dev->drv->ops.copy);

	/* copy only if we are the owner of the component */
	if (cpu_is_me(dev->comp.core)) {
		perf_cnt_init(&dev->pcd);
		ret = dev->drv->ops.copy(dev);
		perf_cnt_stamp(&dev->pcd, comp_perf_info, dev);
	}
	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::get_attribute */
static inline int comp_get_attribute(struct comp_dev *dev, uint32_t type,
				     void *value)
{
	int ret = 0;

	if (dev->drv->ops.get_attribute)
		ret = dev->drv->ops.get_attribute(dev, type, value);

	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::set_attribute */
static inline int comp_set_attribute(struct comp_dev *dev, uint32_t type,
				     void *value)
{
	int ret = 0;

	if (dev->drv->ops.set_attribute)
		ret = dev->drv->ops.set_attribute(dev, type, value);

	comp_shared_commit(dev);

	return ret;
}

/** Runs comp_ops::reset on the target component's core */
static inline int comp_reset_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_RESET,
		IDC_MSG_RESET_EXT(dev->comp.id), dev->comp.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * Component reset and free runtime resources.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_reset(struct comp_dev *dev)
{
	int ret = 0;

	if (dev->drv->ops.reset)
		ret = (dev->is_shared && !cpu_is_me(dev->comp.core)) ?
			comp_reset_remote(dev) : dev->drv->ops.reset(dev);

	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::dai_config */
static inline int comp_dai_config(struct comp_dev *dev,
				  struct sof_ipc_dai_config *config)
{
	int ret = 0;

	if (dev->drv->ops.dai_config)
		ret = dev->drv->ops.dai_config(dev, config);

	comp_shared_commit(dev);

	return ret;
}

/** See comp_ops::position */
static inline int comp_position(struct comp_dev *dev,
				struct sof_ipc_stream_posn *posn)
{
	int ret = 0;

	if (dev->drv->ops.position)
		ret = dev->drv->ops.position(dev, posn);

	comp_shared_commit(dev);

	return ret;
}

/**
 * Allocates and initializes audio component list.
 * To be called once at boot time.
 */
void sys_comp_init(struct sof *sof);

/**
 * Checks if two component devices belong to the same parent pipeline.
 * @param current Component device.
 * @param previous Another component device.
 * @return 1 if children of the same pipeline, 0 otherwise.
 */
static inline int comp_is_single_pipeline(struct comp_dev *current,
					  struct comp_dev *previous)
{
	return dev_comp_pipe_id(current) == dev_comp_pipe_id(previous);
}

/**
 * Checks if component device is active.
 * @param current Component device.
 * @return 1 if active, 0 otherwise.
 */
static inline int comp_is_active(struct comp_dev *current)
{
	return current->state == COMP_STATE_ACTIVE;
}

/**
 * Returns component state based on requested command.
 * @param cmd Request command.
 * @return Component state.
 */
static inline int comp_get_requested_state(int cmd)
{
	int state = COMP_STATE_INIT;

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		state = COMP_STATE_ACTIVE;
		break;
	case COMP_TRIGGER_PREPARE:
	case COMP_TRIGGER_STOP:
		state = COMP_STATE_PREPARE;
		break;
	case COMP_TRIGGER_PAUSE:
		state = COMP_STATE_PAUSED;
		break;
	case COMP_TRIGGER_XRUN:
	case COMP_TRIGGER_RESET:
		state = COMP_STATE_READY;
		break;
	default:
		break;
	}

	return state;
}

/**
 * \brief Returns endpoint type of given component.
 * @param dev Component device
 * @return Endpoint type, one of comp_endpoint_type.
 */
static inline int comp_get_endpoint_type(struct comp_dev *dev)
{
	switch (dev_comp_type(dev)) {
	case SOF_COMP_HOST:
		return COMP_ENDPOINT_HOST;
	case SOF_COMP_DAI:
		return COMP_ENDPOINT_DAI;
	default:
		return COMP_ENDPOINT_NODE;
	}
}

/**
 * Called to check whether component schedules its pipeline.
 * @param dev Component device.
 * @return True if this is scheduling component, false otherwise.
 */
static inline bool comp_is_scheduling_source(struct comp_dev *dev)
{
	return dev == dev->pipeline->sched_comp;
}

/**
 * Called to reallocate component in shared memory.
 * @param dev Component device.
 * @return Pointer to reallocated component device.
 */
struct comp_dev *comp_make_shared(struct comp_dev *dev);

static inline struct comp_driver_list *comp_drivers_get(void)
{
	return sof_get()->comp_drivers;
}

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_INT_H__ */
