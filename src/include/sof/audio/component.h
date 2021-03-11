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
#include <sof/lib/perf_cnt.h>
#include <sof/math/numbers.h>
#include <sof/schedule/schedule.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <kernel/abi.h>
#include <user/trace.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct comp_dev;
struct sof_ipc_dai_config;
struct sof_ipc_stream_posn;
struct dai_hw_params;

/** \addtogroup component_api Component API
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

/** \brief Retrieves trace context from the component driver */
#define trace_comp_drv_get_tr_ctx(drv_p) ((drv_p)->tctx)

/** \brief Retrieves id (-1 = undefined) from the component driver */
#define trace_comp_drv_get_id(drv_p) (-1)

/** \brief Retrieves subid (-1 = undefined) from the component driver */
#define trace_comp_drv_get_subid(drv_p) (-1)

/** \brief Retrieves trace context from the component device */
#define trace_comp_get_tr_ctx(comp_p) (&(comp_p)->tctx)

/** \brief Retrieves id (pipe id) from the component device */
#define trace_comp_get_id(comp_p) ((comp_p)->comp.pipeline_id)

/** \brief Retrieves subid (comp id) from the component device */
#define trace_comp_get_subid(comp_p) ((comp_p)->comp.id)

/* class (driver) level (no device object) tracing */

/** \brief Trace error message from component driver (no comp instance) */
#define comp_cl_err(drv_p, __e, ...)			\
	trace_dev_err(trace_comp_drv_get_tr_ctx,	\
		      trace_comp_drv_get_id,		\
		      trace_comp_drv_get_subid,		\
		      drv_p,				\
		      __e, ##__VA_ARGS__)

/** \brief Trace warning message from component driver (no comp instance) */
#define comp_cl_warn(drv_p, __e, ...)			\
	trace_dev_warn(trace_comp_drv_get_tr_ctx,	\
		       trace_comp_drv_get_id,		\
		       trace_comp_drv_get_subid,	\
		       drv_p,				\
		       __e, ##__VA_ARGS__)

/** \brief Trace info message from component driver (no comp instance) */
#define comp_cl_info(drv_p, __e, ...)			\
	trace_dev_info(trace_comp_drv_get_tr_ctx,	\
		       trace_comp_drv_get_id,		\
		       trace_comp_drv_get_subid,	\
		       drv_p,				\
		       __e, ##__VA_ARGS__)

/** \brief Trace debug message from component driver (no comp instance) */
#define comp_cl_dbg(drv_p, __e, ...)			\
	trace_dev_dbg(trace_comp_drv_get_tr_ctx,	\
		      trace_comp_drv_get_id,		\
		      trace_comp_drv_get_subid,		\
		      drv_p,				\
		      __e, ##__VA_ARGS__)

/* device tracing */

/** \brief Trace error message from component device */
#define comp_err(comp_p, __e, ...)					\
	trace_dev_err(trace_comp_get_tr_ctx, trace_comp_get_id,		\
		      trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

/** \brief Trace warning message from component device */
#define comp_warn(comp_p, __e, ...)					\
	trace_dev_warn(trace_comp_get_tr_ctx, trace_comp_get_id,	\
		       trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

/** \brief Trace info message from component device */
#define comp_info(comp_p, __e, ...)					\
	trace_dev_info(trace_comp_get_tr_ctx, trace_comp_get_id,	\
		       trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

/** \brief Trace debug message from component device */
#define comp_dbg(comp_p, __e, ...)					\
	trace_dev_dbg(trace_comp_get_tr_ctx, trace_comp_get_id,		\
		      trace_comp_get_subid, comp_p, __e, ##__VA_ARGS__)

#define comp_perf_info(pcd, comp_p)					\
	comp_info(comp_p, "perf comp_copy peak plat %d cpu %d",		\
		  (uint32_t)((pcd)->plat_delta_peak),			\
		  (uint32_t)((pcd)->cpu_delta_peak))

/** @}*/

/** \brief Type of endpoint this component is connected to in a pipeline */
enum comp_endpoint_type {
	COMP_ENDPOINT_HOST,	/**< Connected to host dma */
	COMP_ENDPOINT_DAI,	/**< Connected to dai dma */
	COMP_ENDPOINT_NODE	/**< No dma connection */
};

/**
 * \brief Type of next dma copy mode, changed on runtime.
 *
 * Supported by host as COMP_ATTR_COPY_TYPE parameter
 * to comp_set_attribute().
 */
enum comp_copy_type {
	COMP_COPY_INVALID = -1,	/**< Invalid */
	COMP_COPY_NORMAL = 0,	/**< Normal */
	COMP_COPY_BLOCKING,	/**< Blocking */
	COMP_COPY_ONE_SHOT,	/**< One-shot */
};

struct comp_driver;

/**
 * Audio component operations.
 *
 * All component operations must return 0 for success, negative values for
 * errors and 1 to stop the pipeline walk operation unless specified otherwise
 * in the operation documentation.
 */
struct comp_ops {
	/**
	 * Creates a new component device.
	 * @param drv Parent component driver.
	 * @param comp Component parameters.
	 * @return Pointer to the new component device.
	 *
	 * All required data objects should be allocated from the run-time
	 * heap (SOF_MEM_ZONE_RUNTIME).
	 * Any component-specific private data is allocated separately and
	 * pointer is connected to the comp_dev::private by using
	 * comp_set_drvdata() and later retrieved by comp_get_drvdata().
	 *
	 * All parameters should be initialized to their default values.
	 */
	struct comp_dev *(*create)(const struct comp_driver *drv,
				   struct sof_ipc_comp *comp);

	/**
	 * Called to delete the specified component device.
	 * @param dev Component device to be deleted.
	 *
	 * All data structures previously allocated on the run-time heap
	 * must be freed by the implementation of <code>free()</code>.
	 */
	void (*free)(struct comp_dev *dev);

	/**
	 * Sets component audio stream parameters.
	 * @param dev Component device.
	 * @param params Audio (PCM) stream parameters to be set.
	 *
	 * Infrastructure calls comp_verify_params() if this handler is not
	 * defined, therefore it should be left NULL if no extra steps are
	 * required.
	 */
	int (*params)(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params);

	/**
	 * Fetches hardware stream parameters.
	 * @param dev Component device.
	 * @param params Receives copy of stream parameters retrieved from
	 *	DAI hw settings.
	 * @param dir Stream direction (see enum sof_ipc_stream_direction).
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_get_hw_params)(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params, int dir);

	/**
	 * Configures attached DAI.
	 * @param dev Component device.
	 * @param dai_config DAI configuration.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_config)(struct comp_dev *dev,
			  struct sof_ipc_dai_config *dai_config);

	/**
	 * Used to pass standard and bespoke commands (with optional data).
	 * @param dev Component device.
	 * @param cmd Command.
	 * @param data Command data.
	 * @param max_data_size Max size of returned data acceptable by the
	 *	caller in case of 'get' commands.
	 */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size);

	/**
	 * Trigger, atomic - used to start/stop/pause stream operations.
	 * @param dev Component device.
	 * @param cmd Command, one of COMP_TRIGGER_...
	 */
	int (*trigger)(struct comp_dev *dev, int cmd);

	/**
	 * Prepares component after params are set.
	 * @param dev Component device.
	 *
	 * Prepare should be used to get the component ready for starting
	 * processing after it's hw_params are known or after an XRUN.
	 */
	int (*prepare)(struct comp_dev *dev);

	/**
	 * Resets component.
	 * @param dev Component device.
	 *
	 * Resets the component state and any hw_params to default component
	 * state. Should also free any resources acquired during hw_params.
	 * TODO: Some components are not compliant here wrt reset(). Fix this
	 * in v1.8.
	 */
	int (*reset)(struct comp_dev *dev);

	/**
	 * Copy and process stream data from source to sink buffers.
	 * @param dev Component device.
	 * @return Number of copied frames.
	 */
	int (*copy)(struct comp_dev *dev);

	/**
	 * Retrieves component rendering position.
	 * @param dev Component device.
	 * @param posn Receives reported position.
	 */
	int (*position)(struct comp_dev *dev,
		struct sof_ipc_stream_posn *posn);

	/**
	 * Gets attribute in component.
	 * @param dev Component device.
	 * @param type Attribute type.
	 * @param value Attribute value.
	 * @return 0 if succeeded, error code otherwise.
	 */
	int (*get_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/**
	 * Sets attribute in component.
	 * @param dev Component device.
	 * @param type Attribute type.
	 * @param value Attribute value.
	 * @return 0 if succeeded, error code otherwise.
	 */
	int (*set_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/**
	 * Configures timestamping in attached DAI.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_config)(struct comp_dev *dev);

	/**
	 * Starts timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_start)(struct comp_dev *dev);

	/**
	 * Stops timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_stop)(struct comp_dev *dev);

	/**
	 * Gets timestamp.
	 * @param dev Component device.
	 * @param tsd Receives timestamp data.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_get)(struct comp_dev *dev,
			  struct timestamp_data *tsd);
};

/**
 * Audio component base driver "class"
 * - used by all other component types.
 */
struct comp_driver {
	uint32_t type;			/**< SOF_COMP_ for driver */
	const struct sof_uuid *uid;	/**< Address to UUID value */
	struct tr_ctx *tctx;		/**< Pointer to trace context */
	struct comp_ops ops;		/**< component operations */
};

/** \brief Holds constant pointer to component driver */
struct comp_driver_info {
	const struct comp_driver *drv;	/**< pointer to component driver */
	struct list_item list;		/**< list of component drivers */
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
	struct tr_ctx tctx;	/**< trace settings */

	/* common runtime configuration for downstream/upstream */
	uint32_t direction;	/**< enum sof_ipc_stream_direction */

	const struct comp_driver *drv;	/**< driver */

	/* lists */
	struct list_item bsource_list;	/**< list of source buffers */
	struct list_item bsink_list;	/**< list of sink buffers */

	/* private data - core does not touch this */
	void *priv_data;	/**< private data */

#if CONFIG_PERFORMANCE_COUNTERS
	struct perf_cnt_data pcd;
#endif

	/**
	 * IPC config object header - MUST be at end as it's
	 * variable size/type
	 */
	struct sof_ipc_comp comp;
};

/** @}*/

/* Common helper function used internally by the component implementations
 * begin here.
 */

/** \addtogroup component_common_int Component's Common Helpers
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
 * Allocates memory for the component device and initializes common part.
 * @param drv Parent component driver.
 * @param bytes Size of the component device in bytes.
 * @return Pointer to the component device.
 */
static inline struct comp_dev *comp_alloc(const struct comp_driver *drv,
					  size_t bytes)
{
	struct comp_dev *dev = NULL;

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, bytes);
	if (!dev)
		return NULL;
	dev->size = bytes;
	dev->drv = drv;
	dev->state = COMP_STATE_INIT;
	memcpy_s(&dev->tctx, sizeof(struct tr_ctx),
		 trace_comp_drv_get_tr_ctx(dev->drv), sizeof(struct tr_ctx));

	return dev;
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

/**
 * \brief Assigns private data to component device.
 * @param c Component device.
 * @param data Private data.
 */
#define comp_set_drvdata(c, data) \
	(c->priv_data = data)

/**
 * \brief Retrieves driver private data from component device.
 * @param c Component device.
 * @return Private data.
 */
#define comp_get_drvdata(c) \
	c->priv_data

#if defined UNIT_TEST || defined __ZEPHYR__
#define DECLARE_MODULE(init)
#elif CONFIG_LIBRARY
/* In case of shared libs components are initialised in dlopen */
#define DECLARE_MODULE(init) __attribute__((constructor)) \
	static void _module_init(void) { init(); }
#else
/** \brief Usage at the end of an independent module file:
 *	DECLARE_MODULE(sys_*_init);
 */
#define DECLARE_MODULE(init) __attribute__((__used__)) \
	__section(".module_init") static void(*f)(void) = init
#endif

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
	int32_t bytes = (int32_t)audio_stream_get_avail_bytes(&source->stream) -
			copy_bytes;

	comp_err(dev, "comp_underrun(): dev->comp.id = %u, source->avail = %u, copy_bytes = %u",
		 dev_comp_id(dev),
		 audio_stream_get_avail_bytes(&source->stream),
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
	int32_t bytes = (int32_t)copy_bytes -
			audio_stream_get_free_bytes(&sink->stream);

	comp_err(dev, "comp_overrun(): sink->free = %u, copy_bytes = %u",
		 audio_stream_get_free_bytes(&sink->stream), copy_bytes);

	pipeline_xrun(dev->pipeline, dev, bytes);
}

/** @}*/

/**
 * Computes source to sink copy operation boundaries including maximum number
 * of frames that can be transferred (data available in source vs. free space
 * available in sink).
 *
 * @param[in] source Source buffer.
 * @param[in] sink Sink buffer.
 * @param[out] cl Current copy limits.
 */
void comp_get_copy_limits(struct comp_buffer *source, struct comp_buffer *sink,
			  struct comp_copy_limits *cl);

/**
 * Version of comp_get_copy_limits that locks both buffers to guarantee
 * consistent state readings.
 *
 * @param[in] source Source buffer.
 * @param[in] sink Sink buffer
 * @param[out] cl Current copy limits.
 */
static inline
void comp_get_copy_limits_with_lock(struct comp_buffer *source,
				    struct comp_buffer *sink,
				    struct comp_copy_limits *cl)
{
	uint32_t source_flags = 0;
	uint32_t sink_flags = 0;

	buffer_lock(source, &source_flags);
	buffer_lock(sink, &sink_flags);

	comp_get_copy_limits(source, sink, cl);

	buffer_unlock(sink, sink_flags);
	buffer_unlock(source, source_flags);
}

/**
 * Invalidates component to ensure current state and params readout.
 * @param dev Component to invalidate
 */
static inline void comp_invalidate(struct comp_dev *dev)
{
	if (!dev->is_shared)
		dcache_invalidate_region(dev, sizeof(struct comp_dev));
}

/**
 * Writeback component to ensure current state and params readout.
 * @param dev Component to writeback
 */
static inline void comp_writeback(struct comp_dev *dev)
{
	if (!dev->is_shared)
		dcache_writeback_region(dev, sizeof(struct comp_dev));
}

/**
 * Get component state.
 *
 * @param req_dev Requesting component
 * @param dev Component from which user wants to read status.
 */
static inline int comp_get_state(struct comp_dev *req_dev, struct comp_dev *dev)
{
	/* we should not invalidate data when components are on the same
	 * core, because we could invalidate data not previously writebacked
	 */
	if (req_dev->comp.core != dev->comp.core)
		comp_invalidate(dev);

	return dev->state;
}

struct comp_data_blob_handler;

/**
 * Returns data blob. In case when new data blob is available it returns new
 * one. Function returns also data blob size in case when size pointer is given.
 *
 * @param blob_handler Data blob handler
 * @param size Pointer to data blob size variable
 * @param crc Pointer to data blolb crc value
 */
void *comp_get_data_blob(struct comp_data_blob_handler *blob_handler,
			 size_t *size,  uint32_t *crc);

/**
 * Checks whether new data blob is available. Function allows to check (even
 * during streaming - in copy() function) whether new config is available and
 * if it is, component can perform reconfiguration.
 *
 * @param blob_handler Data blob handler
 */
bool comp_is_new_data_blob_available(struct comp_data_blob_handler
					*blob_handler);

/**
 * Initilizes data blob with given value. If init_data is not specified,
 * function will zero data blob.
 *
 * @param blob_handler Data blob handler
 * @param size Data blob size
 * @param init_data Initial data blob values
 */
int comp_init_data_blob(struct comp_data_blob_handler *blob_handler,
			uint32_t size, void *init_data);

/**
 * Handles IPC set command.
 *
 * @param blob_handler Data blob handler
 * @param cdata IPC ctrl data
 */
int comp_data_blob_set_cmd(struct comp_data_blob_handler *blob_handler,
			   struct sof_ipc_ctrl_data *cdata);
/**
 * Handles IPC get command.
 *
 * @param blob_handler Data blob handler
 * @param cdata IPC ctrl data
 * @param size Required size
 */
int comp_data_blob_get_cmd(struct comp_data_blob_handler *blob_handler,
			   struct sof_ipc_ctrl_data *cdata, int size);

/**
 * Returns new data blob handler.
 *
 * @param dev Component device
 */
struct comp_data_blob_handler *comp_data_blob_handler_new(struct comp_dev *dev);

/**
 * Free data blob handler.
 *
 * @param blob_handler Data blob handler
 */
void comp_data_blob_handler_free(struct comp_data_blob_handler *blob_handler);

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

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_H__ */
