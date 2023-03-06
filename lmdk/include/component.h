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

#include <../include/format.h>
#include <../include/list.h>
#include <../include/math/numbers.h>
#include <../include/ipc/stream.h>
#include <../include/ipc/topology.h>

#include <../include/ipc-config.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct comp_dev;
struct sof_ipc_stream_posn;
struct dai_hw_params;
struct timestamp_data;

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
#define COMP_STATE_PRE_ACTIVE	6	/**< Component after early initialisation */
/** @}*/


/**
 * Trace context.
 */
struct tr_ctx {
	const struct sof_uuid_entry *uuid_p;	/**< UUID pointer, use SOF_UUID() to init */
	uint32_t level;				/**< Default log level */
};

/** \name Standard Component Stream Commands
 *  TODO: use IPC versions after 1.1
 *
 * Most component stream commands match one-to-one IPC stream trigger commands.
 * However we add two PRE_ and two POST_ commands to the set. They are issued
 * internally without matching IPC commands. A single START IPC command is
 * translated into a sequence of PRE_START and START component commands, etc.
 * POST_* commands aren't used so far.
 *
 *  @{
 */
enum {
	COMP_TRIGGER_STOP,		/**< Stop component stream */
	COMP_TRIGGER_START,		/**< Start component stream */
	COMP_TRIGGER_PAUSE,		/**< Pause the component stream */
	COMP_TRIGGER_RELEASE,		/**< Release paused component stream */
	COMP_TRIGGER_RESET,		/**< Reset component */
	COMP_TRIGGER_PREPARE,		/**< Prepare component */
	COMP_TRIGGER_XRUN,		/**< XRUN component */
	COMP_TRIGGER_PRE_START,		/**< Prepare to start component stream */
	COMP_TRIGGER_PRE_RELEASE,	/**< Prepare to release paused component stream */
	COMP_TRIGGER_POST_STOP,		/**< Finalize stop component stream */
	COMP_TRIGGER_POST_PAUSE,	/**< Finalize pause component stream */
	COMP_TRIGGER_NO_ACTION,		/**< No action required */
};
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
#define COMP_ATTR_COPY_DIR	2	/**< Comp copy direction */
#define COMP_ATTR_VDMA_INDEX	3	/**< Comp index of the virtual DMA at the gateway. */
#define COMP_ATTR_BASE_CONFIG	4	/**< Component base config */
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
#define trace_comp_get_id(comp_p) ((comp_p)->ipc_config.pipeline_id)

/** \brief Retrieves subid (comp id) from the component device */
#define trace_comp_get_subid(comp_p) ((comp_p)->ipc_config.id)

#if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG)
/* class (driver) level (no device object) tracing */
#define comp_cl_err(drv_p, __e, ...) LOG_ERR(__e, ##__VA_ARGS__)

#define comp_cl_warn(drv_p, __e, ...) LOG_WRN(__e, ##__VA_ARGS__)

#define comp_cl_info(drv_p, __e, ...) LOG_INF(__e, ##__VA_ARGS__)

#define comp_cl_dbg(drv_p, __e, ...) LOG_DBG(__e, ##__VA_ARGS__)

/* device level tracing */

#if CONFIG_IPC_MAJOR_4
#define __COMP_FMT "comp:%u %#x "
#else
#define __COMP_FMT "comp:%u.%u "
#endif

#define comp_err(comp_p, __e, ...) LOG_ERR(__COMP_FMT __e, trace_comp_get_id(comp_p), \
					   trace_comp_get_subid(comp_p), ##__VA_ARGS__)

#define comp_warn(comp_p, __e, ...) LOG_WRN(__COMP_FMT __e, trace_comp_get_id(comp_p), \
					    trace_comp_get_subid(comp_p), ##__VA_ARGS__)

#define comp_info(comp_p, __e, ...) LOG_INF(__COMP_FMT __e, trace_comp_get_id(comp_p), \
					    trace_comp_get_subid(comp_p), ##__VA_ARGS__)

#define comp_dbg(comp_p, __e, ...) LOG_DBG(__COMP_FMT __e, trace_comp_get_id(comp_p), \
					   trace_comp_get_subid(comp_p), ##__VA_ARGS__)

#else
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

#endif /* #if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG) */

#define comp_perf_info(pcd, comp_p)					\
	comp_info(comp_p, "perf comp_copy peak plat %u cpu %u",		\
		  (uint32_t)((pcd)->plat_delta_peak),			\
		  (uint32_t)((pcd)->cpu_delta_peak))

#define comp_perf_avg_info(pcd, comp_p)					\
	comp_info(comp_p, "perf comp_copy samples %u period %u cpu avg %u peak %u",\
		  (uint32_t)((comp_p)->frames),            \
		  (uint32_t)((comp_p)->period),			    \
		  (uint32_t)((pcd)->cpu_delta_sum),			\
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
struct comp_ipc_config;
union ipc_config_specific;

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
				   const struct comp_ipc_config *ipc_config,
				   const void *ipc_specific_config);

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
	int (*dai_config)(struct comp_dev *dev, struct ipc_config_dai *dai_config,
			  const void *dai_spec_config);

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

	/**
	 * Bind, atomic - used to notify component of bind event.
	 * @param dev Component device.
	 * @param data Bind info
	 */
	int (*bind)(struct comp_dev *dev, void *data);

	/**
	 * Unbind, atomic - used to notify component of unbind event.
	 * @param dev Component device.
	 * @param data unBind info
	 */
	int (*unbind)(struct comp_dev *dev, void *data);

	/**
	 * Gets config in component.
	 * @param dev Component device
	 * @param param_id param id for each component
	 * @param first_block first block of large block.
	 * @param last_block last block of large block.
	 * @param data_offset block offset filled by callee.
	 * @param data block data.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * Callee fills up *data with config data and save the config
	 * size in *data_offset for host to reconstruct the config
	 */
	int (*get_large_config)(struct comp_dev *dev, uint32_t param_id,
				bool first_block,
				bool last_block,
				uint32_t *data_offset,
				char *data);

	/**
	 * Set config in component.
	 * @param dev Component device
	 * @param param_id param id for each component
	 * @param first_block first block of large block.
	 * @param last_block last block of large block.
	 * @param data_offset block offset in large block.
	 * @param data block data.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * Host divides large block into small blocks and sends them
	 * to fw. The data_offset indicates the offset in the large
	 * block data.
	 */
	int (*set_large_config)(struct comp_dev *dev, uint32_t param_id,
				bool first_block,
				bool last_block,
				uint32_t data_offset,
				const char *data);

	/**
	 * Returns total data processed in number bytes.
	 * @param dev Component device
	 * @param stream_no Index of input/output stream
	 * @param input Selects between input (true) or output (false) stream direction
	 * @return total data processed if succeeded, 0 otherwise.
	 */
	uint64_t (*get_total_data_processed)(struct comp_dev *dev, uint32_t stream_no, bool input);
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
 * Audio component base configuration from IPC at creation.
 */
struct comp_ipc_config {
	uint32_t core;		/**< core we run on */
	uint32_t id;		/**< component id */
	uint32_t pipeline_id;	/**< component pipeline id */
	enum sof_comp_type type;	/**< component type */
	uint32_t periods_sink;	/**< 0 means variable */
	uint32_t periods_source;/**< 0 means variable */
	uint32_t frame_fmt;	/**< SOF_IPC_FRAME_ */
	uint32_t xrun_action;	/**< action we should take on XRUN */
};

/**
 * Audio component base device "class"
 * - used by other component types.
 */
struct comp_dev {

	/* runtime */
	uint16_t state;		   /**< COMP_STATE_ */
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
	struct comp_ipc_config ipc_config;	/**< Component IPC configuration */
	struct tr_ctx tctx;	/**< trace settings */

	/* common runtime configuration for downstream/upstream */
	uint32_t direction;	/**< enum sof_ipc_stream_direction */
	bool direction_set; /**< flag indicating that the direction has been set */

	const struct comp_driver *drv;	/**< driver */

	/* lists */
	struct list_item bsource_list;	/**< list of source buffers */
	struct list_item bsink_list;	/**< list of sink buffers */

	/* private data - core does not touch this */
	void *priv_data;	/**< private data */

#if CONFIG_PERFORMANCE_COUNTERS
	struct perf_cnt_data pcd;
#endif
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

/**
 * Retrieves Component id from device.
 * @param dev Device.
 * @return Component id.
 */
static inline uint32_t dev_comp_id(const struct comp_dev *dev)
{
	return dev->ipc_config.id;
}

/**
 * Retrieves Component pipeline id from device.
 * @param dev Device.
 * @return Component pipeline id.
 */
static inline uint32_t dev_comp_pipe_id(const struct comp_dev *dev)
{
	return dev->ipc_config.pipeline_id;
}

/**
 * Retrieves component type from device.
 * @param dev Device.
 * @return Component type.
 */
static inline enum sof_comp_type dev_comp_type(const struct comp_dev *dev)
{
	return dev->ipc_config.type;
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
//	struct comp_dev *dev = NULL;
//
//	/*
//	 * Use uncached address everywhere to access components to rule out
//	 * multi-core failures. In the future we might decide to switch over to
//	 * the latest coherence API for performance. In that case components
//	 * will be acquired for cached access and released afterwards.
//	 */
//	dev = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, bytes);
//	if (!dev)
//		return NULL;
//	dev->size = bytes;
//	dev->drv = drv;
//	dev->state = COMP_STATE_INIT;
//	memcpy_s(&dev->tctx, sizeof(struct tr_ctx),
//		 trace_comp_drv_get_tr_ctx(dev->drv), sizeof(struct tr_ctx));
//
//	return dev;
return 0;
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
	(c->priv_data)

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
	__section(".initcall") static void(*f##init)(void) = init
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
static inline void component_set_nearest_period_frames(struct comp_dev *current,
						       uint32_t rate)
{
	/* Sample rate is in Hz and period in microseconds.
	 * As we don't have floats use scale divider 1000000.
	 * Also integer round up the result.
	 * dma buffer size should align with 32bytes which can't be
	 * compatible with current 45K adjustment. 48K is a suitable
	 * adjustment.
	 */
	switch (rate) {
	case 44100:
		current->frames = 48000 * current->period / 1000000;
		break;
	case 88200:
		current->frames = 96000 * current->period / 1000000;
		break;
	case 176400:
		current->frames = 192000 * current->period / 1000000;
		break;
	default:
		current->frames = ceil_divide(rate * current->period, 1000000);
		break;
	}
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
//static inline void comp_underrun(struct comp_dev *dev,
//				 struct comp_buffer *source,
//				 uint32_t copy_bytes)
//{
//	LOG_MODULE_DECLARE(component, CONFIG_SOF_LOG_LEVEL);
//
//	int32_t bytes = (int32_t)audio_stream_get_avail_bytes(&source->stream) -
//			copy_bytes;
//
//	comp_err(dev, "comp_underrun(): dev->comp.id = %u, source->avail = %u, copy_bytes = %u",
//		 dev_comp_id(dev),
//		 audio_stream_get_avail_bytes(&source->stream),
//		 copy_bytes);
//
//	pipeline_xrun(dev->pipeline, dev, bytes);
//}

/**
 * Called by component device when overrun is detected.
 * @param dev Component device.
 * @param sink Sink buffer.
 * @param copy_bytes Requested size of free space to be available.
 */
//static inline void comp_overrun(struct comp_dev *dev, struct comp_buffer *sink,
//				uint32_t copy_bytes)
//{
//	LOG_MODULE_DECLARE(component, CONFIG_SOF_LOG_LEVEL);
//
//	int32_t bytes = (int32_t)copy_bytes -
//			audio_stream_get_free_bytes(&sink->stream);
//
//	comp_err(dev, "comp_overrun(): sink->free = %u, copy_bytes = %u",
//		 audio_stream_get_free_bytes(&sink->stream), copy_bytes);
//
//	pipeline_xrun(dev->pipeline, dev, bytes);
//}

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
void comp_get_copy_limits(struct comp_buffer  *source,
			  struct comp_buffer  *sink,
			  struct comp_copy_limits *cl);

/**
 * Computes source to sink copy operation boundaries including maximum number
 * of frames aligned with requirement that can be transferred (data available in
 * source vs. free space available in sink).
 *
 * @param[in] source Buffer of source.
 * @param[in] sink Buffer of sink.
 * @param[out] cl Current copy limits.
 */
void comp_get_copy_limits_frame_aligned(const struct comp_buffer *source,
					const struct comp_buffer *sink,
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
	struct comp_buffer *source_c, *sink_c;

	source_c = buffer_acquire(source);
	sink_c = buffer_acquire(sink);

	comp_get_copy_limits(source_c, sink_c, cl);

	buffer_release(sink_c);
	buffer_release(source_c);
}

/**
 * Version of comp_get_copy_limits_with_lock_frame_aligned that locks both
 * buffers to guarantee consistent state readings and the frames aligned with
 * the requirement.
 *
 * @param[in] source Buffer of source.
 * @param[in] sink Buffer of sink
 * @param[out] cl Current copy limits.
 */
static inline
void comp_get_copy_limits_with_lock_frame_aligned(struct comp_buffer *source,
						  struct comp_buffer *sink,
						  struct comp_copy_limits *cl)
{
	struct comp_buffer *source_c, *sink_c;

	source_c = buffer_acquire(source);
	sink_c = buffer_acquire(sink);

	comp_get_copy_limits_frame_aligned(source_c, sink_c, cl);

	buffer_release(sink_c);
	buffer_release(source_c);
}

/**
 * Get component state.
 *
 * @param req_dev Requesting component
 * @param dev Component from which user wants to read status.
 */
static inline int comp_get_state(struct comp_dev *req_dev, struct comp_dev *dev)
{
	return dev->state;
}

/**
 * Warning: duplicate declaration in topology.h
 *
 * Called by component in  params() function in order to set and update some of
 * downstream (playback) or upstream (capture) buffer parameters with pcm
 * parameters. There is a possibility to specify which of parameters won't be
 * overwritten (e.g. SRC component should not overwrite rate parameter, because
 * it is able to change it).
 *
 * @param dev Component device
 * @param flag Specifies which parameter should not be updated
 * @param params pcm params
 */
int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params);

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_H__ */
