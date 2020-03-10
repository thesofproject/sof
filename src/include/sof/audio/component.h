/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
  * \file include/sof/audio/component.h
  * \brief Component API definition
  * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
  * \author Keyon Jie <yang.jie@linux.intel.com>
  */

#ifndef __SOF_AUDIO_COMPONENT_H__
#define __SOF_AUDIO_COMPONENT_H__

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <sof/list.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/math/numbers.h>
#include <sof/schedule/schedule.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/trace.h>
#include <config.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct comp_dev;
struct sof_ipc_dai_config;
struct sof_ipc_stream_posn;
struct dai_hw_params;

/** \addtogroup component_api Component API
 *  Component API specification.
 *  @{
 */

/* NOTE: Keep the component state diagram up to date:
 * sof-docs/developer_guides/firmware/components/images/comp-dev-states.pu
 */

/** \name Audio Component States
 *  @{
 */
#define COMP_STATE_INIT		0	/**< Component being initialised */
#define COMP_STATE_READY	1	/**< Component inactive, but ready */
#define COMP_STATE_SUSPEND	2	/**< Component suspended */
#define COMP_STATE_PREPARE	3	/**< Component prepared */
#define COMP_STATE_PAUSED	4	/**< Component paused */
#define COMP_STATE_ACTIVE	5	/**< Component active */
/** @}*/

/** \name Standard Component Stream Commands
 *  TODO: use IPC versions after 1.1
 *  @{
 */
#define COMP_TRIGGER_STOP	0	/**< Stop component stream */
#define COMP_TRIGGER_START	1	/**< Start component stream */
#define COMP_TRIGGER_PAUSE	2	/**< Pause the component stream */
#define COMP_TRIGGER_RELEASE	3	/**< Release paused component stream */
#define COMP_TRIGGER_SUSPEND	4	/**< Suspend component */
#define COMP_TRIGGER_RESUME	5	/**< Resume component */
#define COMP_TRIGGER_RESET	6	/**< Reset component */
#define COMP_TRIGGER_PREPARE	7	/**< Prepare component */
#define COMP_TRIGGER_XRUN	8	/**< XRUN component */
/** @}*/

/** \name Standard Component Control Commands
 *  "Value" commands are standard ones, known to the driver while
 *  "Data" commands are opaque blobs transferred by the driver.
 *
 *  TODO: see also: ref to ipc data structures for commands.
 *  @{
 */
#define COMP_CMD_SET_VALUE	100	/**< Set value to component */
#define COMP_CMD_GET_VALUE	101     /**< Get value from component */
#define COMP_CMD_SET_DATA	102     /**< Set data to component */
#define COMP_CMD_GET_DATA	103     /**< Get data from component */
/** @}*/

/** \name MMAP IPC status
 *  @{
 */
#define COMP_CMD_IPC_MMAP_RPOS	200	/**< Host read position */
#define COMP_CMD_IPC_MMAP_PPOS	201	/**< DAI presentation position */

#define COMP_CMD_IPC_MMAP_VOL(chan)	(216 + chan)	/**< Volume */
/** @}*/

/** \name Component status
 *  @{
 */
#define COMP_STATUS_STATE_ALREADY_SET	1	/**< Comp set state status */
/** @}*/

/** \name Component attribute types
 *  @{
 */
#define COMP_ATTR_COPY_TYPE	0	/**< Comp copy type attribute */
#define COMP_ATTR_HOST_BUFFER	1	/**< Comp host buffer attribute */
/** @}*/

/** \name Trace macros
 *  @{
 */
#define trace_comp_drv_get_uid(drv_p) ((drv_p)->uid)
#define trace_comp_drv_get_id(drv_p) (-1)
#define trace_comp_drv_get_subid(drv_p) (-1)

#define trace_comp_get_uid(comp_p) ((comp_p)->drv->uid)
#define trace_comp_get_id(comp_p) ((comp_p)->comp.pipeline_id)
#define trace_comp_get_subid(comp_p) ((comp_p)->comp.id)

/* class (driver) level (no device object) tracing */

#define comp_cl_err(drv_p, __e, ...)		\
	trace_dev_err(TRACE_CLASS_COMP,		\
		      trace_comp_drv_get_uid,	\
		      trace_comp_drv_get_id,	\
		      trace_comp_drv_get_subid,	\
		      drv_p,			\
		      __e, ##__VA_ARGS__)

#define comp_cl_warn(drv_p, __e, ...)		\
	trace_dev_warn(TRACE_CLASS_COMP,	\
		       trace_comp_drv_get_uid,	\
		       trace_comp_drv_get_id,	\
		       trace_comp_drv_get_subid,\
		       drv_p,			\
		       __e, ##__VA_ARGS__)

#define comp_cl_info(drv_p, __e, ...)		\
	trace_dev_info(TRACE_CLASS_COMP,	\
		       trace_comp_drv_get_uid,	\
		       trace_comp_drv_get_id,	\
		       trace_comp_drv_get_subid,\
		       drv_p,			\
		       __e, ##__VA_ARGS__)

#define comp_cl_dbg(drv_p, __e, ...)		\
	trace_dev_dbg(TRACE_CLASS_COMP,		\
		      trace_comp_drv_get_uid,	\
		      trace_comp_drv_get_id,	\
		      trace_comp_drv_get_subid,	\
		      drv_p,			\
		      __e, ##__VA_ARGS__)

/* device tracing */

#define comp_err(comp_p, __e, ...)					\
	trace_dev_err(TRACE_CLASS_COMP, trace_comp_get_uid, trace_comp_get_id, \
		      trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

#define comp_warn(comp_p, __e, ...)					\
	trace_dev_warn(TRACE_CLASS_COMP, trace_comp_get_uid, trace_comp_get_id,\
		       trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

#define comp_info(comp_p, __e, ...)					\
	trace_dev_info(TRACE_CLASS_COMP, trace_comp_get_uid, trace_comp_get_id,\
		       trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

#define comp_dbg(comp_p, __e, ...)					\
	trace_dev_dbg(TRACE_CLASS_COMP, trace_comp_get_uid, trace_comp_get_id, \
		      trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)


/** @}*/

/* \brief Type of endpoint this component is connected to in a pipeline */
enum comp_endpoint_type {
	COMP_ENDPOINT_HOST,
	COMP_ENDPOINT_DAI,
	COMP_ENDPOINT_NODE
};

 /* \brief Type of component copy, which can be changed on runtime */
enum comp_copy_type {
	COMP_COPY_NORMAL = 0,
	COMP_COPY_BLOCKING,
	COMP_COPY_ONE_SHOT,
};

struct comp_driver;

/**
 * Audio component operations - all mandatory.
 *
 * All component operations must return 0 for success, negative values for
 * errors and 1 to stop the pipeline walk operation.
 */
struct comp_ops {
	/** component creation */
	struct comp_dev *(*new)(const struct comp_driver *drv,
				struct sof_ipc_comp *comp);

	/** component destruction */
	void (*free)(struct comp_dev *dev);

	/** set component audio stream parameters */
	int (*params)(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params);

	/** get dai hw parameters */
	int (*dai_get_hw_params)(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params, int dir);

	/** set component audio stream parameters */
	int (*dai_config)(struct comp_dev *dev,
			  struct sof_ipc_dai_config *dai_config);

	/** used to pass standard and bespoke commands (with optional data) */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size);

	/** atomic - used to start/stop/pause stream operations */
	int (*trigger)(struct comp_dev *dev, int cmd);

	/** prepare component after params are set */
	int (*prepare)(struct comp_dev *dev);

	/** reset component */
	int (*reset)(struct comp_dev *dev);

	/** copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *dev);

	/** position */
	int (*position)(struct comp_dev *dev,
		struct sof_ipc_stream_posn *posn);

	/** set attribute in component */
	int (*set_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/* Configure timestamping */
	int (*dai_ts_config)(struct comp_dev *dev);

	/* Start timestamping */
	int (*dai_ts_start)(struct comp_dev *dev);

	/* Stop timestamping */
	int (*dai_ts_stop)(struct comp_dev *dev);

	/* Get timestamp */
	int (*dai_ts_get)(struct comp_dev *dev,
			  struct timestamp_data *tsd);
};


/**
 * Audio component base driver "class"
 * - used by all other component types.
 */
struct comp_driver {
	uint32_t type;		/**< SOF_COMP_ for driver */
	uint32_t uid;		/**< Address of uuid_entry */
	struct comp_ops ops;	/**< component operations */
};

/* \brief Holds constant pointer to component driver */
struct comp_driver_info {
	const struct comp_driver *drv;	/**< pointer to component driver */
	struct list_item list;		/**< list of component drivers */
};

/* \brief Holds list of registered components' drivers */
struct comp_driver_list {
	struct list_item list;	/* list of component drivers */
};

/**
 * Audio component base device "class"
 * - used by other component types.
 */
struct comp_dev {

	/* runtime */
	uint16_t state;		   /**< COMP_STATE_ */
	uint64_t position;	   /**< component rendering position */
	uint32_t frames;	   /**< number of frames we copy to sink */
	struct pipeline *pipeline; /**< pipeline we belong to */

	uint32_t min_sink_bytes;   /**< min free sink buffer size measured in
				     *  bytes required to run component's
				     *  processing
				     */
	uint32_t min_source_bytes; /**< amount of data measured in bytes
				     *  available at source buffer required
				     *  to run component's processing
				     */

	struct task *task;	/**< component's processing task used only
				  *  for components running on different core
				  *  than the rest of the pipeline
				  */
	uint32_t size;		/**< component's allocated size */
	uint32_t period;	/**< component's processing period */
	uint32_t priority;	/**< component's processing priority */
	bool is_shared;		/**< indicates whether component is shared
				  *  across cores
				  */

	/** common runtime configuration for downstream/upstream */
	uint32_t direction;	/**< enum sof_ipc_stream_direction */

	/** driver */
	const struct comp_driver *drv;

	/* lists */
	struct list_item bsource_list;	/**< list of source buffers */
	struct list_item bsink_list;	/**< list of sink buffers */

	/* private data - core does not touch this */
	void *private;		/**< private data */

	/**
	 * IPC config object header - MUST be at end as it's
	 * variable size/type
	 */
	struct sof_ipc_comp comp;
};

/** \name Helpers.
 *  @{
 */

/** \brief Struct for use with comp_get_copy_limits() function. */
struct comp_copy_limits {
	int frames;
	int source_bytes;
	int sink_bytes;
	int source_frame_bytes;
	int sink_frame_bytes;
};

/** \brief Computes size of the component device including ipc config. */
#define COMP_SIZE(x) \
	(sizeof(struct comp_dev) - sizeof(struct sof_ipc_comp) + sizeof(x))

/** \brief Retrieves component device IPC configuration. */
#define COMP_GET_IPC(dev, type) \
	(struct type *)(&dev->comp)

/**
 * Retrieves component from device.
 * @param dev Device.
 * @return Pointer to the component.
 */
static inline struct sof_ipc_comp *dev_comp(struct comp_dev *dev)
{
	return &dev->comp;
}

/**
 * Retrieves Component id from device.
 * @param dev Device.
 * @return Component id.
 */
static inline uint32_t dev_comp_id(const struct comp_dev *dev)
{
	return dev->comp.id;
}

/**
 * Retrieves Component pipeline id from device.
 * @param dev Device.
 * @return Component pipeline id.
 */
static inline uint32_t dev_comp_pipe_id(const struct comp_dev *dev)
{
	return dev->comp.pipeline_id;
}

/**
 * Retrieves component type from device.
 * @param dev Device.
 * @return Component type.
 */
static inline enum sof_comp_type dev_comp_type(const struct comp_dev *dev)
{
	return dev->comp.type;
}

/**
 * Retrieves component config data from device.
 * @param dev Device.
 * @return Pointer to the component data.
 */
static inline struct sof_ipc_comp_config *dev_comp_config(struct comp_dev *dev)
{
	return (struct sof_ipc_comp_config *)(&dev->comp + 1);
}

/**
 * Retrieves component config data from component ipc.
 * @param comp Component ipc data.
 * @return Pointer to the component config data.
 */
static inline struct sof_ipc_comp_config *comp_config(struct sof_ipc_comp *comp)
{
	return (struct sof_ipc_comp_config *)(comp + 1);
}

/** \brief Sets the driver private data. */
#define comp_set_drvdata(c, data) \
	(c->private = data)

/** \brief Retrieves the driver private data. */
#define comp_get_drvdata(c) \
	c->private

/** \brief Retrieves the component device buffer list. */
#define comp_buffer_list(comp, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &comp->bsink_list : \
	 &comp->bsource_list)

/** @}*/

/** \name Declare module macro
 *  \brief Usage at the end of an independent module file:
 *         DECLARE_MODULE(sys_*_init);
 *  @{
 */
#ifdef UNIT_TEST
#define DECLARE_MODULE(init)
#elif CONFIG_LIBRARY
/* In case of shared libs components are initialised in dlopen */
#define DECLARE_MODULE(init) __attribute__((constructor)) \
	static void _module_init(void) { init(); }
#else
#define DECLARE_MODULE(init) __attribute__((__used__)) \
	__section(".module_init") static void(*f)(void) = init
#endif

/** @}*/

/** \name Component registration
 *  @{
 */

/**
 * Registers the component driver on the list of available components.
 * @param drv Component driver to be registered.
 * @return 0 if succeeded, error code otherwise.
 */
int comp_register(struct comp_driver_info *drv);

/**
 * Unregisters the component driver from the list of available components.
 * @param drv Component driver to be unregistered.
 */
void comp_unregister(struct comp_driver_info *drv);

/** @}*/

/** \name Component creation and destruction - mandatory
 *  @{
 */

/**
 * Creates a new component device.
 * @param comp Parameters of the new component device.
 * @return Pointer to the new component device.
 */
struct comp_dev *comp_new(struct sof_ipc_comp *comp);

/**
 * Called to delete the specified component device.
 * All data structures previously allocated on the run-time heap
 * must be freed by the implementation of <code>free()</code>.
 * @param dev Component device to be deleted.
 */
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

/** @}*/

/** \name Component API.
 *  @{
 */

/**
 * Commits component's memory if it's shared.
 * @param dev Component device.
 */
static inline void comp_shared_commit(struct comp_dev *dev)
{
	if (dev->is_shared)
		platform_shared_commit(dev, sizeof(*dev));
}

/**
 * Component state set.
 * @param dev Component device.
 * @param cmd Command, one of <i>COMP_TRIGGER_...</i>.
 * @return 0 if succeeded, error code otherwise.
 *
 * This function should be called by a component implementation at the beginning
 * of its state transition to verify whether the trigger is valid in the
 * current state and abort the transition otherwise.
 *
 * Typically the COMP_STATE_READY as the initial state is set directly
 * by the component's implementation of _new().
 *
 * COMP_TRIGGER_PREPARE is called from the component's prepare().
 *
 * COMP_TRIGGER_START/STOP is called from trigger().
 *
 * COMP_TRIGGER_RESET is called from reset().
 */
int comp_set_state(struct comp_dev *dev, int cmd);

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

/**
 * Component parameter init.
 * @param dev Component device.
 * @param params Audio (PCM) stream parameters to be set
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	int ret = 0;

	if (dev->drv->ops.params)
		ret = (dev->is_shared && !cpu_is_me(dev->comp.core)) ?
			comp_params_remote(dev, params) :
			dev->drv->ops.params(dev, params);

	comp_shared_commit(dev);

	return ret;
}

/**
 * Fetch hardware stream parameters - only mandatory for DAI components.
 * @param dev Component device.
 * @param params hw parameters
 * @return 0 if succeeded, error code otherwise.
 */
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

/**
 * Send component command.
 * @param dev Component device.
 * @param cmd Command.
 * @param data Command data.
 * @param max_data_size Max data size.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = -EINVAL;

	if (cmd == COMP_CMD_SET_DATA &&
	    (cdata->data->magic != SOF_ABI_MAGIC ||
	     SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi))) {
		comp_err(dev, "comp_cmd() error: invalid version, data->magic = %u, data->abi = %u",
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
 * Triggers command for component on other core.
 * @param dev Component device.
 * @param cmd Command to be triggered.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_trigger_remote(struct comp_dev *dev, int cmd)
{
	struct idc_msg msg = { IDC_MSG_TRIGGER,
		IDC_MSG_TRIGGER_EXT(dev->comp.id), dev->comp.core, sizeof(cmd),
		&cmd, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * Trigger component - mandatory and atomic.
 * @param dev Component device.
 * @param cmd Command.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_trigger(struct comp_dev *dev, int cmd)
{
	int ret = 0;

	assert(dev->drv->ops.trigger);

	ret = (dev->is_shared && !cpu_is_me(dev->comp.core)) ?
		comp_trigger_remote(dev, cmd) : dev->drv->ops.trigger(dev, cmd);

	comp_shared_commit(dev);

	return ret;
}

/**
 * Prepares component on other core.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_prepare_remote(struct comp_dev *dev)
{
	struct idc_msg msg = { IDC_MSG_PREPARE,
		IDC_MSG_PREPARE_EXT(dev->comp.id), dev->comp.core, };

	return idc_send_msg(&msg, IDC_BLOCKING);
}

/**
 * Prepare component.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_prepare(struct comp_dev *dev)
{
	int ret = 0;

	if (dev->drv->ops.prepare)
		ret = (dev->is_shared && !cpu_is_me(dev->comp.core)) ?
			comp_prepare_remote(dev) : dev->drv->ops.prepare(dev);

	comp_shared_commit(dev);

	return ret;
}

/**
 * Copy component buffers - mandatory.
 * @param dev Component device.
 * @return Number of frames copied.
 */
static inline int comp_copy(struct comp_dev *dev)
{
	int ret = 0;

	assert(dev->drv->ops.copy);

	/* copy only if we are the owner of the component */
	if (cpu_is_me(dev->comp.core))
		ret = dev->drv->ops.copy(dev);

	comp_shared_commit(dev);

	return ret;
}

/**
 * Resets component on other core.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
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

/**
 * DAI configuration - only mandatory for DAI components.
 * @param dev Component device.
 * @param config DAI configuration.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_dai_config(struct comp_dev *dev,
	struct sof_ipc_dai_config *config)
{
	int ret = 0;

	if (dev->drv->ops.dai_config)
		ret = dev->drv->ops.dai_config(dev, config);

	comp_shared_commit(dev);

	return ret;
}

/**
 * Retrieves component rendering position.
 * @param dev Component device.
 * @param posn Position reported by the component device.
 * @return 0 if succeeded, error code otherwise.
 */
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
 * Sets component attribute.
 * @param dev Component device.
 * @param type Attribute type.
 * @param value Attribute value.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_set_attribute(struct comp_dev *dev, uint32_t type,
				     void *value)
{
	int ret = 0;

	if (dev->drv->ops.set_attribute)
		ret = dev->drv->ops.set_attribute(dev, type, value);

	comp_shared_commit(dev);

	return ret;
}

/** @}*/

/** \name Components API for infrastructure.
 *  @{
 */

/**
 * Allocates and initializes audio component list.
 * To be called once at boot time.
 */
void sys_comp_init(struct sof *sof);

/** @}*/

/** \name Helpers.
 *  @{
 */

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

/* \brief Returns comp_endpoint_type of given component */
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

/* \brief Set component period frames */
static inline void component_set_period_frames(struct comp_dev *current,
					       uint32_t rate)
{
	/* Samplerate is in Hz and period in microseconds.
	 * As we don't have floats use scale divider 1000000.
	 * Also integer round up the result.
	 */
	current->frames = ceil_divide(rate * current->period, 1000000);
}

/** @}*/

/** \name XRUN handling.
 *  @{
 */

/**
 * Called by the component device when underrun is detected.
 * @param dev Component device.
 * @param source Source buffer.
 * @param copy_bytes Requested size of data to be available.
 */
static inline void comp_underrun(struct comp_dev *dev,
				 struct comp_buffer *source,
				 uint32_t copy_bytes)
{
	int32_t bytes = (int32_t)source->stream.avail - copy_bytes;

	comp_err(dev, "comp_underrun() error: dev->comp.id = %u, source->avail = %u, copy_bytes = %u",
		 dev_comp_id(dev),
		 source->stream.avail,
		 copy_bytes);

	pipeline_xrun(dev->pipeline, dev, bytes);
}

/**
 * Called by component device when overrun is detected.
 * @param dev Component device.
 * @param sink Sink buffer.
 * @param copy_bytes Requested size of free space to be available.
 */
static inline void comp_overrun(struct comp_dev *dev, struct comp_buffer *sink,
				uint32_t copy_bytes)
{
	int32_t bytes = (int32_t)copy_bytes - sink->stream.free;

	comp_err(dev, "comp_overrun() error: sink->free = %u, copy_bytes = %u",
		 sink->stream.free, copy_bytes);

	pipeline_xrun(dev->pipeline, dev, bytes);
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
 * Called by component in copy.
 * @param source Source buffer.
 * @param sink Sink buffer.
 * @param cl Struct of parameters for use in copy function.
 */
void comp_get_copy_limits(struct comp_buffer *source, struct comp_buffer *sink,
			  struct comp_copy_limits *cl);

/**
 * Called by component in  params() function in order to set and update some of
 * downstream (playback) or upstream (capture) buffer parameters with pcm
 * parameters. There is a possibility to specify which of parameters won't be
 * overwritten (e.g. SRC component should not overwrite rate parameter, because
 * it is able to change it).
 * @param dev Component device
 * @param flag Specifies which parameter should not be updated
 * @param params pcm params
 */
int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params);
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

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_H__ */
