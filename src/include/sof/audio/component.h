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
#include <sof/debug/telemetry/telemetry.h>
#include <rtos/idc.h>
#include <rtos/userspace_helper.h>
#include <sof/lib/dai.h>
#include <sof/schedule/schedule.h>
#include <ipc/control.h>
#include <sof/ipc/topology.h>
#include <kernel/abi.h>

#include <limits.h>

struct comp_dev;
struct sof_ipc_stream_posn;
struct dai_hw_params;
struct timestamp_data;
struct dai_ts_data;

/** \addtogroup component_api Component API
 *  @{
 */

/* NOTE: Keep the component state diagram up to date:
 * sof-docs/developer_guides/firmware/components/images/comp-dev-states.pu
 */

/** \name Audio Component States
 *  @{
 */
#define COMP_STATE_NOT_EXIST    0	/**< Component does not exist */
#define COMP_STATE_INIT		1	/**< Component being initialised */
#define COMP_STATE_READY	2	/**< Component inactive, but ready */
#define COMP_STATE_SUSPEND	3	/**< Component suspended */
#define COMP_STATE_PREPARE	4	/**< Component prepared */
#define COMP_STATE_PAUSED	5	/**< Component paused */
#define COMP_STATE_ACTIVE	6	/**< Component active */
#define COMP_STATE_PRE_ACTIVE	7	/**< Component after early initialisation */
/** @}*/

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
#define COMP_ATTR_IPC4_CONFIG	5	/**< Component ipc4 set/get config */
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
	comp_info(comp_p, "perf comp_copy samples %u period %u cpu avg %u peak %u %u",\
		  (uint32_t)((comp_p)->frames),            \
		  (uint32_t)((comp_p)->period),			    \
		  (uint32_t)((pcd)->cpu_delta_sum),			\
		  (uint32_t)((pcd)->cpu_delta_peak),			\
		  (uint32_t)((pcd)->peak_mcps_period_cnt))

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
struct ipc4_module_bind_unbind;

enum bind_type {
	COMP_BIND_TYPE_SOURCE,
	COMP_BIND_TYPE_SINK
};

struct bind_info {
	/* pointer to IPC4 bind/unbind data */
	struct ipc4_module_bind_unbind *ipc4_data;

	/* type of binding
	 * bind call will be called twice for every component, first when binding a data source,
	 * than when binding data sink.
	 *
	 * bind_type is indicating type of binding
	 */
	enum bind_type bind_type;

	/* pointers to sink or source API of the data provider/consumer
	 * that is being bound to the module
	 *
	 * if bind_type == COMP_BIND_TYPE_SOURCE, the pointer points to source being bound/unbound
	 * if bind_type == COMP_BIND_TYPE_SINK, the pointer points to sink being bound/unbound
	 *
	 * NOTE! As in pipeline2.0 there may be a binding between modules,
	 * without a buffer in between, it cannot be a pointer to any buffer type, module should
	 * use sink/source API
	 */
	union {
		struct sof_source *source;
		struct sof_sink *sink;
	};
};

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
	 * @param ipc_config Component parameters.
	 * @param spec Pointer to initialization data
	 * @return Pointer to the new component device.
	 *
	 * Any component-specific private data is allocated separately and
	 * pointer is connected to the comp_dev::private by using
	 * comp_set_drvdata() and later retrieved by comp_get_drvdata().
	 *
	 * All parameters should be initialized to their default values.
	 * Usually can be __cold.
	 */
	struct comp_dev *(*create)(const struct comp_driver *drv,
				   const struct comp_ipc_config *ipc_config,
				   const void *spec);

	/**
	 * Called to delete the specified component device.
	 * @param dev Component device to be deleted.
	 *
	 * All data structures previously allocated on the run-time heap
	 * must be freed by the implementation of <code>free()</code>.
	 * Usually can be __cold.
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
	 * Usually shouldn't be __cold.
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
	 * Usually can be __cold.
	 */
	int (*dai_get_hw_params)(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params, int dir);

	/**
	 * Configures attached DAI.
	 * @param dev Component device.
	 * @param dai_config DAI configuration.
	 *
	 * Mandatory for components that allocate DAI.
	 * Usually can be __cold.
	 */
	int (*dai_config)(struct dai_data *dd, struct comp_dev *dev,
			  struct ipc_config_dai *dai_config, const void *dai_spec_config);

#if CONFIG_IPC_MAJOR_3 || CONFIG_LIBRARY
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
#endif

	/**
	 * Trigger, atomic - used to start/stop/pause stream operations.
	 * @param dev Component device.
	 * @param cmd Command, one of COMP_TRIGGER_...
	 *
	 * Usually shouldn't be __cold.
	 */
	int (*trigger)(struct comp_dev *dev, int cmd);

	/**
	 * Prepares component after params are set.
	 * @param dev Component device.
	 *
	 * Prepare should be used to get the component ready for starting
	 * processing after it's hw_params are known or after an XRUN.
	 * Usually shouldn't be __cold.
	 */
	int (*prepare)(struct comp_dev *dev);

	/**
	 * Resets component.
	 * @param dev Component device.
	 *
	 * Resets the component state and any hw_params to default component
	 * state. Should also free any resources acquired during hw_params.
	 * Usually shouldn't be __cold.
	 */
	int (*reset)(struct comp_dev *dev);

	/**
	 * Copy and process stream data from source to sink buffers.
	 * @param dev Component device.
	 * @return Number of copied frames.
	 *
	 * Usually shouldn't be __cold.
	 */
	int (*copy)(struct comp_dev *dev);

	/**
	 * Retrieves component rendering position.
	 * @param dev Component device.
	 * @param posn Receives reported position.
	 *
	 * Usually shouldn't be __cold.
	 */
	int (*position)(struct comp_dev *dev,
		struct sof_ipc_stream_posn *posn);

	/**
	 * Gets attribute in component.
	 * @param dev Component device.
	 * @param type Attribute type.
	 * @param value Attribute value.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * Usually can be __cold.
	 */
	int (*get_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/**
	 * Sets attribute in component.
	 * @param dev Component device.
	 * @param type Attribute type.
	 * @param value Attribute value.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * Usually can be __cold.
	 */
	int (*set_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/**
	 * Configures timestamping in attached DAI.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 * Usually can be __cold.
	 */
	int (*dai_ts_config)(struct comp_dev *dev);

	/**
	 * Starts timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 * Usually can be __cold.
	 */
	int (*dai_ts_start)(struct comp_dev *dev);

	/**
	 * Stops timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 * Usually can be __cold.
	 */
	int (*dai_ts_stop)(struct comp_dev *dev);

	/**
	 * Gets timestamp.
	 * @param dev Component device.
	 * @param tsd Receives timestamp data.
	 *
	 * Mandatory for components that allocate DAI.
	 * Usually shouldn't be __cold.
	 */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	int (*dai_ts_get)(struct comp_dev *dev, struct dai_ts_data *tsd);
#else
	int (*dai_ts_get)(struct comp_dev *dev,
			  struct timestamp_data *tsd);
#endif

	/**
	 * Bind, atomic - used to notify component of bind event.
	 * @param dev Component device.
	 * @param data Bind info
	 *
	 * Usually can be __cold.
	 */
	int (*bind)(struct comp_dev *dev, struct bind_info *bind_data);

	/**
	 * Unbind, atomic - used to notify component of unbind event.
	 * @param dev Component device.
	 * @param data unBind info
	 *
	 * Usually can be __cold.
	 */
	int (*unbind)(struct comp_dev *dev, struct bind_info *unbind_data);

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
	 * Usually can be __cold.
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
	 * Usually can be __cold.
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
	 *
	 * Usually shouldn't be __cold.
	 */
	uint64_t (*get_total_data_processed)(struct comp_dev *dev, uint32_t stream_no, bool input);
};

/**
 * Audio component base driver "class"
 * - used by all other component types.
 */
struct comp_driver {
	uint32_t type;					/**< SOF_COMP_ for driver */
	const struct sof_uuid *uid;			/**< Address to UUID value */
	struct tr_ctx *tctx;				/**< Pointer to trace context */
	struct comp_ops ops;				/**< component operations */
	const struct module_interface *adapter_ops;	/**< module specific operations.
							  * Intended to replace the ops field.
							  * Currently used by module_adapter.
							  */
	struct sys_heap *user_heap;			/**< Userspace heap */
};

/** \brief Holds constant pointer to component driver */
struct comp_driver_info {
	const struct comp_driver *drv;			/**< pointer to component driver */
	const struct module_interface **adapter_ops;	/**< pointer for updating ops */
	struct list_item list;				/**< list of component drivers */
};

#define COMP_PROCESSING_DOMAIN_LL 0
#define COMP_PROCESSING_DOMAIN_DP 1

/**
 * Audio component base configuration from IPC at creation.
 */
struct comp_ipc_config {
	uint32_t core;			/**< core we run on */
	uint32_t id;			/**< component id */
	uint32_t pipeline_id;		/**< component pipeline id */
	uint32_t proc_domain;		/**< processing domain - LL or DP */
	enum sof_comp_type type;	/**< component type */
	uint32_t periods_sink;		/**< 0 means variable */
	uint32_t periods_source;	/**< 0 means variable */
	uint32_t frame_fmt;		/**< SOF_IPC_FRAME_ */
	uint32_t xrun_action;		/**< action we should take on XRUN */
#if CONFIG_IPC_MAJOR_4
	bool ipc_extended_init;		/**< true if extended init is included in ipc payload */
	uint32_t ipc_config_size;	/**< size of a config received by ipc */
#endif
};

struct comp_perf_data {
	/* maximum measured cpc on run-time.
	 *
	 * if current measured cpc exceeds peak_of_measured_cpc_ then
	 * ResoruceEvent(BUDGET_VIOLATION) notification must be send.
	 * Otherwise there is no new information for host to care about
	 */
	size_t peak_of_measured_cpc;
	/* Pointer to performance data structure. */
	struct perf_data_item_comp *perf_data_item;
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

	struct task *task;	/**< component's processing task used
				  *  1) for components running on different core
				  *    than the rest of the pipeline
				  *  2) for all DP tasks
				  */
	uint32_t size;		/**< component's allocated size */
	uint32_t period;	/**< component's processing period
				  *  for LL modules is set to LL pipeline's period
				  *  for DP module its meaning is "the time the module MUST
				  *  provide data that allows the following module to perform
				  *  without glitches"
				  */
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

	struct processing_module *mod; /**< self->mod->dev == self, NULL if component is not using
					 *  module adapter
					 */

	/* lists */
	struct list_item bsource_list;	/**< list of source buffers */
	struct list_item bsink_list;	/**< list of sink buffers */

	/* performance data*/
	struct comp_perf_data perf_data;
	/* Input Buffer Size for pin 0, add array for other pins if needed */
	size_t ibs;
	/* Output Buffers Size for pin 0, add array for other pins if needed */
	size_t obs;
	/* max dsp cycles per chunk */
	size_t cpc;
	/* size of 1ms for input format in bytes */
	size_t ll_chunk_size : 16;

	/* private data - core does not touch this */
	void *priv_data;	/**< private data */

#if CONFIG_PERFORMANCE_COUNTERS_COMPONENT
	struct perf_cnt_data pcd;
#endif

#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	int32_t kcps_inc[CONFIG_CORE_COUNT];
#endif
};

/**
 * Get a pointer to the first comp_buffer object providing data to the component
 * The function will return NULL if there's no data provider
 */
static inline struct comp_buffer *comp_dev_get_first_data_producer(struct comp_dev *component)
{
	return list_is_empty(&component->bsource_list) ? NULL :
	       list_first_item(&component->bsource_list, struct comp_buffer, sink_list);
}

/**
 * Get a pointer to the next comp_buffer object providing data to the component
 * The function will return NULL if there're no more data providers
 * _save version also checks if producer != NULL
 */
static inline struct comp_buffer *comp_dev_get_next_data_producer(struct comp_dev *component,
								  struct comp_buffer *producer)
{
	return producer->sink_list.next == &component->bsource_list ? NULL :
	       list_item(producer->sink_list.next, struct comp_buffer, sink_list);
}

static inline struct comp_buffer *comp_dev_get_next_data_producer_safe(struct comp_dev *component,
								       struct comp_buffer *producer)
{
	return producer ? comp_dev_get_next_data_producer(component, producer) : NULL;
}

/**
 * Get a pointer to the first comp_buffer object receiving data from the component
 * The function will return NULL if there's no data consumers
 */
static inline struct comp_buffer *comp_dev_get_first_data_consumer(struct comp_dev *component)
{
	return list_is_empty(&component->bsink_list) ? NULL :
	       list_first_item(&component->bsink_list, struct comp_buffer, source_list);
}

/**
 * Get a pointer to the next comp_buffer object receiving data from the component
 * The function will return NULL if there're no more data consumers
 * _safe version also checks if consumer is != NULL
 */
static inline struct comp_buffer *comp_dev_get_next_data_consumer(struct comp_dev *component,
								  struct comp_buffer *consumer)
{
	return consumer->source_list.next == &component->bsink_list ? NULL :
			list_item(consumer->source_list.next, struct comp_buffer, source_list);
}

static inline struct comp_buffer *comp_dev_get_next_data_consumer_safe(struct comp_dev *component,
								       struct comp_buffer *consumer)
{
	return consumer ? comp_dev_get_next_data_consumer(component, consumer) : NULL;
}

/*
 * a macro for easy iteration through component's list of producers
 */
#define comp_dev_for_each_producer(_dev, _producer)			\
	for (_producer = comp_dev_get_first_data_producer(_dev);	\
	     _producer != NULL;						\
	     _producer = comp_dev_get_next_data_producer(_dev, _producer))

/*
 * a macro for easy iteration through component's list of producers
 * allowing deletion of a buffer during iteration
 *
 * additional "safe storage" pointer to struct comp_buffer must be provided
 */
#define comp_dev_for_each_producer_safe(_dev, _producer, _next_producer)		\
	for (_producer = comp_dev_get_first_data_producer(_dev),			\
	     _next_producer = comp_dev_get_next_data_producer_safe(_dev, _producer);	\
	     _producer != NULL;								\
	     _producer = _next_producer,						\
	     _next_producer = comp_dev_get_next_data_producer_safe(_dev, _producer))

/*
 * a macro for easy iteration through component's list of consumers
 */
#define comp_dev_for_each_consumer(_dev, _consumer)				\
	for (_consumer = comp_dev_get_first_data_consumer(_dev);		\
	     _consumer != NULL;							\
	     _consumer = comp_dev_get_next_data_consumer(_dev, _consumer))	\

/*
 * a macro for easy iteration through component's list of consumers
 * allowing deletion of a buffer during iteration
 *
 * additional "safe storage" pointer to struct comp_buffer must be provided
 */
#define comp_dev_for_each_consumer_safe(_dev, _consumer, _next_consumer)		\
	for (_consumer = comp_dev_get_first_data_consumer(_dev),			\
	     _next_consumer = comp_dev_get_next_data_consumer_safe(_dev, _consumer);	\
	     _consumer != NULL;								\
	     _consumer = _next_consumer,						\
	     _next_consumer = comp_dev_get_next_data_consumer_safe(_dev, _consumer))

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
 * Initialize common part of a component device
 * @param drv Parent component driver.
 * @param dev Device.
 * @param bytes Size of the component device in bytes.
 */
static inline void comp_init(const struct comp_driver *drv,
			     struct comp_dev *dev, size_t bytes)
{
	dev->size = bytes;
	dev->drv = drv;
	dev->state = COMP_STATE_INIT;
	list_init(&dev->bsink_list);
	list_init(&dev->bsource_list);
	memcpy_s(&dev->tctx, sizeof(dev->tctx),
		 trace_comp_drv_get_tr_ctx(dev->drv), sizeof(struct tr_ctx));
}

/**
 * Allocates memory for the component device and initializes common part.
 * @param drv Parent component driver.
 * @param bytes Size of the component device in bytes.
 * @return Pointer to the component device.
 */
static inline struct comp_dev *comp_alloc(const struct comp_driver *drv, size_t bytes)
{
	/*
	 * Use uncached address everywhere to access components to rule out
	 * multi-core failures. TODO: verify if cached alias may be used in some cases
	 */
	struct comp_dev *dev = module_driver_heap_rzalloc(drv->user_heap,
							  SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
							  bytes);

	if (!dev)
		return NULL;

	comp_init(drv, dev, bytes);

	return dev;
}

/**
 * \brief Module adapter associated with a component
 * @param dev Component device
 */
static inline struct processing_module *comp_mod(const struct comp_dev *dev)
{
	return dev->mod;
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

#if defined UNIT_TEST || defined __ZEPHYR__  || CONFIG_LIBRARY_STATIC
#define DECLARE_MODULE(init)

/* declared modules */
void sys_comp_dai_init(void);
void sys_comp_host_init(void);
void sys_comp_kpb_init(void);
void sys_comp_selector_init(void);

/* Start of modules in alphabetical order */
void sys_comp_module_aria_interface_init(void);
void sys_comp_module_asrc_interface_init(void);
void sys_comp_module_copier_interface_init(void);
void sys_comp_module_crossover_interface_init(void);
void sys_comp_module_dcblock_interface_init(void);
void sys_comp_module_demux_interface_init(void);
void sys_comp_module_drc_interface_init(void);
void sys_comp_module_dts_interface_init(void);
void sys_comp_module_eq_fir_interface_init(void);
void sys_comp_module_eq_iir_interface_init(void);
void sys_comp_module_gain_interface_init(void);
void sys_comp_module_google_rtc_audio_processing_interface_init(void);
void sys_comp_module_google_ctc_audio_processing_interface_init(void);
void sys_comp_module_igo_nr_interface_init(void);
void sys_comp_module_level_multiplier_interface_init(void);
void sys_comp_module_mfcc_interface_init(void);
void sys_comp_module_mixer_interface_init(void);
void sys_comp_module_mixin_interface_init(void);
void sys_comp_module_mixout_interface_init(void);
void sys_comp_module_multiband_drc_interface_init(void);
void sys_comp_module_mux_interface_init(void);
void sys_comp_module_nxp_eap_interface_init(void);
void sys_comp_module_rtnr_interface_init(void);
void sys_comp_module_selector_interface_init(void);
void sys_comp_module_sound_dose_interface_init(void);
void sys_comp_module_src_interface_init(void);
void sys_comp_module_src_lite_interface_init(void);
void sys_comp_module_tdfb_interface_init(void);
void sys_comp_module_template_interface_init(void);
void sys_comp_module_tester_interface_init(void);
void sys_comp_module_volume_interface_init(void);
/* End of modules in alphabetical order */

#elif CONFIG_LIBRARY
/* In case of shared libs components are initialised in dlopen */
#define DECLARE_MODULE(init) __attribute__((constructor)) \
	static void _module_##init(void) { init(); }
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

/**
 * Set adapter ops for a dynamically created driver.
 *
 * @param drv Component driver to update.
 * @param ops Module interface operations.
 * @return 0 or a negative error code
 */
int comp_set_adapter_ops(const struct comp_driver *drv, const struct module_interface *ops);

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
	uint64_t frames;

	/* Sample rate is in Hz and period in microseconds.
	 * As we don't have floats use scale divider 1000000.
	 * Also integer round up the result.
	 * dma buffer size should align with 32bytes which can't be
	 * compatible with current 45K adjustment. 48K is a suitable
	 * adjustment.
	 */

	switch (rate) {
	case 44100:
		rate = 48000;
		break;
	case 88200:
		rate = 96000;
		break;
	case 176400:
		rate = 192000;
		break;
	default:
		break;
	}

	frames = SOF_DIV_ROUND_UP((uint64_t)rate * current->period, 1000000);

	assert(frames <= UINT_MAX);
	current->frames = (uint32_t)frames;
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
	LOG_MODULE_DECLARE(component, CONFIG_SOF_LOG_LEVEL);

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
	LOG_MODULE_DECLARE(component, CONFIG_SOF_LOG_LEVEL);

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
void comp_get_copy_limits(struct comp_buffer *source,
			  struct comp_buffer *sink,
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
 * Get component state.
 *
 * @param dev Component from which user wants to read status.
 *
 * @retval COMP_STATE_NOT_EXIST if there's no connected component
 * @return state of the component
 */
static inline int comp_get_state(const struct comp_dev *dev)
{
	if (!dev)
		return COMP_STATE_NOT_EXIST;
	return dev->state;
}

/**
 * @brief helper, provide state of a component connected to a buffer as a data provider
 *
 * @param buffer a buffer to be checked
 *
 * @retval COMP_STATE_NOT_EXIST if there's no connected component
 * @return state of the component
 */
static inline int comp_buffer_get_source_state(const struct comp_buffer *buffer)
{
	return comp_get_state(comp_buffer_get_source_component(buffer));
}

/**
 * @brief helper, provide state of a component connected to a buffer as a data consumer
 *
 * @param buffer a buffer to be checked
 *
 * @retval COMP_STATE_NOT_EXIST if there's no connected component
 * @return state of the component
 */
static inline int comp_buffer_get_sink_state(const struct comp_buffer *buffer)
{
	return comp_get_state(comp_buffer_get_sink_component(buffer));
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

/**
 * Update ibs, obs, cpc, ll chunk size for component.
 *
 * @param dev Component to update.
 */
void comp_update_ibs_obs_cpc(struct comp_dev *dev);

/**
 * If component has assigned slot in performance measurement window,
 * initialize its fields.
 * @param dev Component to init.
 */
void comp_init_performance_data(struct comp_dev *dev);

/**
 * Update performance data entry for component. Also checks for budget violation.
 *
 * @param dev Component to update.
 * @param cycles_used Execution time.
 * @return true if budget violation occurred
 */
bool comp_update_performance_data(struct comp_dev *dev, uint32_t cycles_used);

static inline int user_get_buffer_memory_region(const struct comp_driver *drv)
{
#if CONFIG_USERSPACE
	return drv->user_heap ? SOF_MEM_FLAG_USER_SHARED_BUFFER : SOF_MEM_FLAG_USER;
#else
	return SOF_MEM_FLAG_USER;
#endif
}
#endif /* __SOF_AUDIO_COMPONENT_H__ */
