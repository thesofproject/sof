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
#include <ipc/topology.h>
/* TODO: the cross-core helpers below send IDC messages, which is kernel only */
#include <rtos/idc.h>
#include <sof/schedule/schedule.h>

/** \addtogroup component_infra_api
 *  @{
 */

/** \brief Holds list of registered components' drivers */
struct comp_driver_list {
	struct list_item list;	/**< list of component drivers */
};

/** \brief Retrieves the component device buffer list based on direction.
 * \param[in] comp Component device pointer.
 * \param[in] dir Direction (PPL_DIR_DOWNSTREAM or PPL_DIR_UPSTREAM).
 * \return Pointer to sink list or source list.
 */
#define comp_buffer_list(comp, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &(comp)->bsink_list :	\
	 &(comp)->bsource_list)

#if CONFIG_IPC_MAJOR_3
/**
 * \brief Instantiate a new component device from IPC3 descriptor.
 * \param[in] comp IPC3 component descriptor.
 * \return Pointer to new component device, or NULL on failure.
 */
struct comp_dev *comp_new(struct sof_ipc_comp *comp);
#elif CONFIG_IPC_MAJOR_4
/**
 * \brief Instantiate a new component device from IPC4 module initialization descriptor.
 * \param[in] module_init IPC4 module initialization data.
 * \return Pointer to new component device, or NULL on failure.
 */
struct comp_dev *comp_new_ipc4(const struct ipc4_module_init_instance *module_init);
#endif

/**
 * \brief Free component device via driver ops.
 * \param[in,out] dev Component device to free.
 */
static inline void comp_free(struct comp_dev *dev)
{
	const struct comp_driver *drv = dev->drv;

	assert(drv->ops.free);

	/*
	 * In DP case this will run in DP thread context, so the task can only
	 * be freed after this.
	 */
	drv->ops.free(dev);
}

/**
 * \brief Parameter initialization for component on remote core.
 * \param[in,out] dev Component device.
 * \param[in] params Stream parameters to be set.
 * \return 0 if succeeded, error code otherwise.
 */
static inline int comp_params_remote(struct comp_dev *dev,
				     struct sof_ipc_stream_params *params)
{
	struct idc_msg msg = { IDC_MSG_PARAMS, IDC_MSG_PARAMS_EXT(dev->ipc_config.id),
		dev->ipc_config.core, sizeof(*params), params, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * \brief Configure audio stream parameters for component device.
 * \param[in,out] dev Component device.
 * \param[in,out] params Audio (PCM) stream parameters to configure.
 * \return 0 on success, negative error code on failure.
 */
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

/**
 * \brief Retrieve DAI hardware parameters from component device.
 * \param[in] dev Component device.
 * \param[out] params Stream parameters to populate.
 * \param[in] dir Stream direction (enum sof_ipc_stream_direction).
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_dai_get_hw_params(struct comp_dev *dev,
					 struct sof_ipc_stream_params *params,
					 int dir)
{
	if (dev->drv->ops.dai_get_hw_params)
		return dev->drv->ops.dai_get_hw_params(dev, params, dir);

	return -EINVAL;
}

#if CONFIG_IPC_MAJOR_3
/**
 * \brief Dispatch IPC3 command to component device.
 * \param[in,out] dev Component device.
 * \param[in] cmd Command code.
 * \param[in,out] data Command data buffer.
 * \param[in] max_data_size Maximum acceptable size of returned data.
 * \return 0 on success, negative error code on failure.
 */
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
#endif

/**
 * \brief Run trigger command on remote core for target component.
 * \param[in,out] dev Component device.
 * \param[in] cmd Trigger command (COMP_TRIGGER_*).
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_trigger_remote(struct comp_dev *dev, int cmd)
{
	struct idc_msg msg = { IDC_MSG_TRIGGER,
		IDC_MSG_TRIGGER_EXT(dev->ipc_config.id), dev->ipc_config.core, sizeof(cmd),
		&cmd, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * \brief Run trigger command locally on current core for target component.
 * \param[in,out] dev Component device.
 * \param[in] cmd Trigger command (COMP_TRIGGER_*).
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_trigger_local(struct comp_dev *dev, int cmd)
{
	int ret;

	ret = dev->drv->ops.trigger(dev, cmd);
	if (ret)
		return ret;

	/* start a thread in case of shared component or DP scheduling */
	if (dev->task) {
		/* schedule or cancel task */
		switch (cmd) {
		case COMP_TRIGGER_START:
			ret = schedule_task(dev->task, 0, dev->period);
			break;
		case COMP_TRIGGER_RELEASE:
		case COMP_TRIGGER_XRUN:
		case COMP_TRIGGER_PAUSE:
		case COMP_TRIGGER_STOP:
			break;
		}
	}

	return ret;
}

/**
 * \brief Dispatch trigger command to component (local or remote).
 * \param[in,out] dev Component device.
 * \param[in] cmd Trigger command (COMP_TRIGGER_*).
 * \return 0 on success, negative error code on failure.
 */
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

/**
 * \brief Run prepare command on remote core for target component.
 * \param[in,out] dev Component device.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_prepare_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_PREPARE,
		IDC_MSG_PREPARE_EXT(dev->ipc_config.id), dev->ipc_config.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * \brief Prepare component device for streaming (local or remote).
 * \param[in,out] dev Component device.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_prepare(struct comp_dev *dev)
{
	if (dev->drv->ops.prepare)
		return (dev->is_shared && !cpu_is_me(dev->ipc_config.core)) ?
			comp_prepare_remote(dev) : dev->drv->ops.prepare(dev);

	return 0;
}

/**
 * \brief Process and copy audio data through component.
 * \param[in,out] dev Component device.
 * \return Number of copied frames, or negative error code on failure.
 */
int comp_copy(struct comp_dev *dev);

#if CONFIG_IPC_MAJOR_4
/**
 * \brief Remote payload for component get_attribute IPC4 message.
 */
struct get_attribute_remote_payload {
	uint32_t type; /**< Attribute type (COMP_ATTR_*) */
	void *value;   /**< Pointer to attribute value storage */
};

/**
 * \brief Retrieve component attribute from a remote core (IPC4).
 * \param[in,out] dev Component device.
 * \param[in] type Attribute type (COMP_ATTR_*).
 * \param[out] value Pointer to buffer where attribute value is copied.
 * \return 0 on success, negative error code on failure.
 */
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

	base_cfg = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, sizeof(*base_cfg));
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

/**
 * \brief Get component attribute.
 * \param[in,out] dev Component device.
 * \param[in] type Attribute type (COMP_ATTR_*).
 * \param[out] value Pointer to buffer where attribute value is returned.
 * \return 0 on success, negative error code on failure.
 */
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

/**
 * \brief Set component attribute.
 * \param[in,out] dev Component device.
 * \param[in] type Attribute type (COMP_ATTR_*).
 * \param[in] value Pointer to attribute value to set.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_set_attribute(struct comp_dev *dev, uint32_t type,
				     void *value)
{
	if (dev->drv->ops.set_attribute)
		return dev->drv->ops.set_attribute(dev, type, value);

	return 0;
}

/**
 * \brief Run reset command on remote core for target component.
 * \param[in,out] dev Component device.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_reset_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_RESET,
		IDC_MSG_RESET_EXT(dev->ipc_config.id), dev->ipc_config.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * \brief Reset component and free runtime resources (local or remote).
 * \param[in,out] dev Component device.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_reset(struct comp_dev *dev)
{
	if (dev->drv->ops.reset)
		return (dev->is_shared && !cpu_is_me(dev->ipc_config.core)) ?
			comp_reset_remote(dev) : dev->drv->ops.reset(dev);

	return 0;
}

/**
 * \brief Retrieve stream position from component.
 * \param[in,out] dev Component device.
 * \param[out] posn Stream position structure to populate.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_position(struct comp_dev *dev,
				struct sof_ipc_stream_posn *posn)
{
	if (dev->drv->ops.position)
		return dev->drv->ops.position(dev, posn);

	return 0;
}

/**
 * \brief Allocates and initializes audio component list.
 *
 * To be called once at boot time.
 *
 * \param[in,out] sof SOF global system context.
 */
void sys_comp_init(struct sof *sof);

/**
 * \brief Checks if two component devices belong to the same parent pipeline.
 * \param[in] current Component device.
 * \param[in] previous Another component device.
 * \return 1 if children of the same pipeline, 0 otherwise.
 */
static inline int comp_is_single_pipeline(struct comp_dev *current,
					  struct comp_dev *previous)
{
	return dev_comp_pipe_id(current) == dev_comp_pipe_id(previous);
}

/**
 * \brief Checks if component device is active.
 * \param[in] current Component device.
 * \return 1 if active, 0 otherwise.
 */
static inline int comp_is_active(struct comp_dev *current)
{
	return current->state == COMP_STATE_ACTIVE;
}

/**
 * \brief Returns component state based on requested command.
 * \param[in] cmd Request command (COMP_TRIGGER_*).
 * \return Component state (COMP_STATE_*).
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
 * \param[in] dev Component device.
 * \return Endpoint type, one of comp_endpoint_type (COMP_ENDPOINT_*).
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
 * \brief Called to check whether component schedules its pipeline.
 * \param[in] dev Component device.
 * \return True if this is scheduling component, false otherwise.
 */
static inline bool comp_is_scheduling_source(struct comp_dev *dev)
{
	return dev == dev->pipeline->sched_comp;
}

/**
 * \brief Called to mark component as shared between cores.
 * \param[in,out] dev Component device.
 */
static inline void comp_make_shared(struct comp_dev *dev)
{
	dev->is_shared = true;
}

/**
 * \brief Get the global component driver list.
 * \return Pointer to the component driver list.
 */
#ifdef CONFIG_SOF_USERSPACE_LL
struct comp_driver_list *comp_drivers_get(void);
#else
static inline struct comp_driver_list *comp_drivers_get(void)
{
	return sof_get()->comp_drivers;
}
#endif

#if CONFIG_IPC_MAJOR_4
/**
 * \brief Send bind message to component on remote core (IPC4).
 * \param[in,out] dev Component device.
 * \param[in] bind_data Binding information.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_ipc4_bind_remote(struct comp_dev *dev, const struct bind_info *bind_data)
{
	struct idc_msg msg = { IDC_MSG_BIND,
		IDC_EXTENSION(dev->ipc_config.id), dev->ipc_config.core,
		sizeof(*bind_data), bind_data};

	return idc_send_msg(&msg, IDC_BLOCKING);
}
#endif

/**
 * \brief Bind component to downstream/upstream component or buffer.
 * \param[in,out] dev Component device.
 * \param[in] bind_data Binding information.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_bind(struct comp_dev *dev, const struct bind_info *bind_data)
{
#if CONFIG_IPC_MAJOR_4
	if (dev->drv->ops.bind)
		return cpu_is_me(dev->ipc_config.core) ?
			dev->drv->ops.bind(dev, bind_data) : comp_ipc4_bind_remote(dev, bind_data);

	return 0;
#else
	int ret = 0;

	if (dev->drv->ops.bind)
		ret = dev->drv->ops.bind(dev, bind_data);

	return ret;
#endif
}

#if CONFIG_IPC_MAJOR_4
/**
 * \brief Send unbind message to component on remote core (IPC4).
 * \param[in,out] dev Component device.
 * \param[in] unbind_data Unbinding information.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_ipc4_unbind_remote(struct comp_dev *dev, const struct bind_info *unbind_data)
{
	struct idc_msg msg = { IDC_MSG_UNBIND,
		IDC_EXTENSION(dev->ipc_config.id), dev->ipc_config.core,
		sizeof(*unbind_data), unbind_data};

	return idc_send_msg(&msg, IDC_BLOCKING);
}
#endif

/**
 * \brief Unbind component from downstream/upstream component or buffer.
 * \param[in,out] dev Component device.
 * \param[in] unbind_data Unbinding information.
 * \return 0 on success, negative error code on failure.
 */
static inline int comp_unbind(struct comp_dev *dev, const struct bind_info *unbind_data)
{
#if CONFIG_IPC_MAJOR_4
	if (dev->drv->ops.unbind)
		return cpu_is_me(dev->ipc_config.core) ?
			dev->drv->ops.unbind(dev, unbind_data) :
			comp_ipc4_unbind_remote(dev, unbind_data);

	return 0;
#else
	int ret = 0;

	if (dev->drv->ops.unbind)
		ret = dev->drv->ops.unbind(dev, unbind_data);

	return ret;
#endif
}

/**
 * \brief Retrieve total data processed by component.
 * \param[in] dev Component device.
 * \param[in] stream_no Stream index.
 * \param[in] input True for input data, false for output data.
 * \return Total processed data in bytes.
 */
static inline uint64_t comp_get_total_data_processed(struct comp_dev *dev, uint32_t stream_no,
						     bool input)
{
	uint64_t ret = 0;

	if (dev->drv->ops.get_total_data_processed)
		ret = dev->drv->ops.get_total_data_processed(dev, stream_no, input);

	return ret;
}

/**
 * \brief Returns true if the component's pipeline matches the specified direction.
 * \param[in] comp Component device.
 * \param[in] dir Stream direction (SOF_IPC_STREAM_PLAYBACK or SOF_IPC_STREAM_CAPTURE).
 * \return True if matching direction, false otherwise.
 */
static inline bool comp_same_dir(struct comp_dev *comp, enum sof_ipc_stream_direction dir)
{
	int end_type = comp_get_endpoint_type(comp->pipeline->sink_comp);

	if (dir == SOF_IPC_STREAM_PLAYBACK && end_type != COMP_ENDPOINT_DAI)
		return false;
	if (dir == SOF_IPC_STREAM_CAPTURE && end_type != COMP_ENDPOINT_HOST)
		return false;
	return true;
}

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_INT_H__ */
