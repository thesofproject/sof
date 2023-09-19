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
#include <rtos/idc.h>
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
	struct k_spinlock lock;	/**< list lock */
};

/** \brief Retrieves the component device buffer list. */
#define comp_buffer_list(comp, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &(comp)->bsink_list :	\
	 &(comp)->bsource_list)

/** See comp_ops::new */
#if CONFIG_IPC_MAJOR_3
struct comp_dev *comp_new(struct sof_ipc_comp *comp);
#elif CONFIG_IPC_MAJOR_4
struct comp_dev *comp_new_ipc4(struct ipc4_module_init_instance *module_init);
#endif

/** See comp_ops::free */
static inline void comp_free(struct comp_dev *dev)
{
	assert(dev->drv->ops.free);

	/* free task if shared component or DP task*/
	if ((dev->is_shared || dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) &&
	    dev->task) {
		schedule_task_free(dev->task);
		rfree(dev->task);
		dev->task = NULL;
	}

	dev->drv->ops.free(dev);
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
	struct idc_msg msg = { IDC_MSG_PARAMS, IDC_MSG_PARAMS_EXT(dev->ipc_config.id),
		dev->ipc_config.core, sizeof(*params), params, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/** See comp_ops::params */
static inline int comp_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	int ret = 0;

	if (dev->is_shared && !cpu_is_me(dev->ipc_config.core)) {
		ret = comp_params_remote(dev, params);
	} else {
		if (dev->drv->ops.params) {
			ret = dev->drv->ops.params(dev, params);
		} else {
			/* not defined, run the default handler */
			ret = comp_verify_params(dev, 0, params);

/* BugLink: https://github.com/zephyrproject-rtos/zephyr/issues/43786
 * TODO: Remove this once the bug gets fixed.
 */
#ifndef __ZEPHYR__
			if (ret < 0)
				comp_err(dev, "pcm params verification failed");
#endif
		}
	}

	return ret;
}

/** See comp_ops::dai_get_hw_params */
static inline int comp_dai_get_hw_params(struct comp_dev *dev,
					 struct sof_ipc_stream_params *params,
					 int dir)
{
	if (dev->drv->ops.dai_get_hw_params)
		return dev->drv->ops.dai_get_hw_params(dev, params, dir);

	return -EINVAL;
}

/** See comp_ops::cmd */
static inline int comp_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	LOG_MODULE_DECLARE(component, CONFIG_SOF_LOG_LEVEL);

	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	if (cmd == COMP_CMD_SET_DATA &&
	    (cdata->data->magic != SOF_ABI_MAGIC ||
	     SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi))) {
		comp_err(dev, "comp_cmd(): invalid version, data->magic = %u, data->abi = %u",
			 cdata->data->magic, cdata->data->abi);
		return -EINVAL;
	}

	if (dev->drv->ops.cmd)
		return dev->drv->ops.cmd(dev, cmd, data, max_data_size);

	return -EINVAL;
}

/**
 * Runs comp_ops::trigger on the core the target component is assigned to.
 */
static inline int comp_trigger_remote(struct comp_dev *dev, int cmd)
{
	struct idc_msg msg = { IDC_MSG_TRIGGER,
		IDC_MSG_TRIGGER_EXT(dev->ipc_config.id), dev->ipc_config.core, sizeof(cmd),
		&cmd, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

static inline int comp_trigger_local(struct comp_dev *dev, int cmd)
{
	int ret;

	ret = dev->drv->ops.trigger(dev, cmd);

	/* start a thread in case of shared component or DP scheduling */
	if (dev->task) {
		/* schedule or cancel task */
		switch (cmd) {
		case COMP_TRIGGER_START:
		case COMP_TRIGGER_RELEASE:
			schedule_task(dev->task, 0, dev->period);
			break;
		case COMP_TRIGGER_XRUN:
		case COMP_TRIGGER_PAUSE:
		case COMP_TRIGGER_STOP:
			schedule_task_cancel(dev->task);
			break;
		}
	}

	return ret;
}

/** See comp_ops::trigger */
static inline int comp_trigger(struct comp_dev *dev, int cmd)
{
	int ret;
	assert(dev->drv->ops.trigger);

	if (dev->is_shared && !cpu_is_me(dev->ipc_config.core))
		ret = comp_trigger_remote(dev, cmd);
	else
		ret = comp_trigger_local(dev, cmd);

	return ret;
}

/** Runs comp_ops::prepare on the target component's core */
static inline int comp_prepare_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_PREPARE,
		IDC_MSG_PREPARE_EXT(dev->ipc_config.id), dev->ipc_config.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/** See comp_ops::prepare */
static inline int comp_prepare(struct comp_dev *dev)
{
	if (dev->drv->ops.prepare)
		return (dev->is_shared && !cpu_is_me(dev->ipc_config.core)) ?
			comp_prepare_remote(dev) : dev->drv->ops.prepare(dev);

	return 0;
}

int comp_copy(struct comp_dev *dev);

#if CONFIG_IPC_MAJOR_4
struct get_attribute_remote_payload {
	uint32_t type;
	void *value;
};

static inline int comp_ipc4_get_attribute_remote(struct comp_dev *dev, uint32_t type,
						 void *value)
{
	struct ipc4_base_module_cfg *base_cfg;
	struct get_attribute_remote_payload payload = {};
	struct idc_msg msg = { IDC_MSG_GET_ATTRIBUTE,
		IDC_EXTENSION(dev->ipc_config.id), dev->ipc_config.core,
		sizeof(payload), &payload};
	int ret;

	/* Only COMP_ATTR_BASE_CONFIG is supported for remote access */
	if (type != COMP_ATTR_BASE_CONFIG)
		return -EINVAL;

	base_cfg = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*base_cfg));
	if (!base_cfg)
		return -ENOMEM;

	payload.type = type;
	payload.value = base_cfg;

	ret = idc_send_msg(&msg, IDC_BLOCKING);

	if (ret == 0)
		memcpy_s(value, sizeof(struct ipc4_base_module_cfg),
			 base_cfg, sizeof(struct ipc4_base_module_cfg));

	rfree(base_cfg);
	return ret;
}
#endif	/* CONFIG_IPC_MAJOR_4 */

/** See comp_ops::get_attribute */
static inline int comp_get_attribute(struct comp_dev *dev, uint32_t type,
				     void *value)
{
#if CONFIG_IPC_MAJOR_4
	if (dev->drv->ops.get_attribute)
		return cpu_is_me(dev->ipc_config.core) ?
			dev->drv->ops.get_attribute(dev, type, value) :
			comp_ipc4_get_attribute_remote(dev, type, value);

	return 0;
#else
	if (dev->drv->ops.get_attribute)
		return dev->drv->ops.get_attribute(dev, type, value);

	return 0;
#endif
}

/** See comp_ops::set_attribute */
static inline int comp_set_attribute(struct comp_dev *dev, uint32_t type,
				     void *value)
{
	if (dev->drv->ops.set_attribute)
		return dev->drv->ops.set_attribute(dev, type, value);

	return 0;
}

/** Runs comp_ops::reset on the target component's core */
static inline int comp_reset_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_RESET,
		IDC_MSG_RESET_EXT(dev->ipc_config.id), dev->ipc_config.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * Component reset and free runtime resources.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_reset(struct comp_dev *dev)
{
	if (dev->drv->ops.reset)
		return (dev->is_shared && !cpu_is_me(dev->ipc_config.core)) ?
			comp_reset_remote(dev) : dev->drv->ops.reset(dev);

	return 0;
}

#if CONFIG_IPC_MAJOR_3
/** See comp_ops::dai_config */
static inline int comp_dai_config(struct comp_dev *dev, struct ipc_config_dai *config,
				  const void *spec_config)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dev->drv->ops.dai_config)
		return dev->drv->ops.dai_config(dd, dev, config, spec_config);

	return 0;
}
#elif CONFIG_IPC_MAJOR_4
static inline int comp_dai_config(struct dai_data *dd, struct comp_dev *dev,
				  struct ipc_config_dai *config, const void *spec_config)
{
	return dai_config(dd, dev, config, spec_config);
}
#else
#error Unknown IPC major version
#endif
/** See comp_ops::position */
static inline int comp_position(struct comp_dev *dev,
				struct sof_ipc_stream_posn *posn)
{
	if (dev->drv->ops.position)
		return dev->drv->ops.position(dev, posn);

	return 0;
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
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		state = COMP_STATE_PRE_ACTIVE;
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
 * Called to mark component as shared between cores
 * @param dev Component device.
 */
static inline void comp_make_shared(struct comp_dev *dev)
{
	dev->is_shared = true;
}

static inline struct comp_driver_list *comp_drivers_get(void)
{
	return sof_get()->comp_drivers;
}

#if CONFIG_IPC_MAJOR_4
static inline int comp_ipc4_bind_remote(struct comp_dev *dev, void *data)
{
	struct idc_msg msg = { IDC_MSG_BIND,
		IDC_EXTENSION(dev->ipc_config.id), dev->ipc_config.core,
		sizeof(struct ipc4_module_bind_unbind), data};

	return idc_send_msg(&msg, IDC_BLOCKING);
}
#endif

static inline int comp_bind(struct comp_dev *dev, void *data)
{
#if CONFIG_IPC_MAJOR_4
	if (dev->drv->ops.bind)
		return cpu_is_me(dev->ipc_config.core) ?
			dev->drv->ops.bind(dev, data) : comp_ipc4_bind_remote(dev, data);

	return 0;
#else
	int ret = 0;

	if (dev->drv->ops.bind)
		ret = dev->drv->ops.bind(dev, data);

	return ret;
#endif
}

#if CONFIG_IPC_MAJOR_4
static inline int comp_ipc4_unbind_remote(struct comp_dev *dev, void *data)
{
	struct idc_msg msg = { IDC_MSG_UNBIND,
		IDC_EXTENSION(dev->ipc_config.id), dev->ipc_config.core,
		sizeof(struct ipc4_module_bind_unbind), data};

	return idc_send_msg(&msg, IDC_BLOCKING);
}
#endif

static inline int comp_unbind(struct comp_dev *dev, void *data)
{
#if CONFIG_IPC_MAJOR_4
	if (dev->drv->ops.unbind)
		return cpu_is_me(dev->ipc_config.core) ?
			dev->drv->ops.unbind(dev, data) : comp_ipc4_unbind_remote(dev, data);

	return 0;
#else
	int ret = 0;

	if (dev->drv->ops.unbind)
		ret = dev->drv->ops.unbind(dev, data);

	return ret;
#endif
}

static inline uint64_t comp_get_total_data_processed(struct comp_dev *dev, uint32_t stream_no,
						     bool input)
{
	uint64_t ret = 0;

	if (dev->drv->ops.get_total_data_processed)
		ret = dev->drv->ops.get_total_data_processed(dev, stream_no, input);

	return ret;
}

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_INT_H__ */
