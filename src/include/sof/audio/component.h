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
#include <rtos/mutex.h>
#include <rtos/userspace_helper.h>
#include <sof/lib/dai.h>
#include <sof/lib/uuid.h>
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

/** \addtogroup component_infra_api
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

/** \brief Retrieves id (-1 = undefined) from the component driver */
#define trace_comp_drv_get_id(drv_p) (-1)

/** \brief Retrieves subid (-1 = undefined) from the component driver */
#define trace_comp_drv_get_subid(drv_p) (-1)

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
/** \brief Retrieves trace context from the component driver */
#define trace_comp_drv_get_tr_ctx(drv_p) ((drv_p)->tctx)

/** \brief Retrieves trace context from the component device */
#define trace_comp_get_tr_ctx(comp_p) (&(comp_p)->tctx)

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

/** \brief Component module binding endpoint type. */
enum bind_type {
	COMP_BIND_TYPE_SOURCE,	/**< Bind to a data source */
	COMP_BIND_TYPE_SINK	/**< Bind to a data sink */
};

/**
 * \brief Component module binding metadata and endpoint references.
 */
struct bind_info {
	const struct ipc4_module_bind_unbind *ipc4_data;	/**< Pointer to IPC4 bind/unbind data */
	enum bind_type bind_type;				/**< Type of binding (source or sink) */

	/* pointers to sink or source API of the data provider/consumer
	 * that is being bound to the module
	 */
	union {
		struct sof_source *source;			/**< Source interface pointer when bind_type == COMP_BIND_TYPE_SOURCE */
		struct sof_sink *sink;				/**< Sink interface pointer when bind_type == COMP_BIND_TYPE_SINK */
	};
};

/**
 * \brief Audio component operations table.
 *
 * All component operations must return 0 for success, negative values for
 * errors and 1 to stop the pipeline walk operation unless specified otherwise
 * in the operation documentation.
 */
struct comp_ops {
	/**
	 * \brief Creates a new component device.
	 * \param[in] drv Parent component driver.
	 * \param[in] ipc_config Component parameters.
	 * \param[in] spec Pointer to initialization data.
	 * \return Pointer to the new component device, or NULL on failure.
	 */
	struct comp_dev *(*create)(const struct comp_driver *drv,
				   const struct comp_ipc_config *ipc_config,
				   const void *spec);

	/**
	 * \brief Called to delete the specified component device.
	 * \param[in,out] dev Component device to be deleted.
	 */
	void (*free)(struct comp_dev *dev);

	/**
	 * \brief Sets component audio stream parameters.
	 * \param[in,out] dev Component device.
	 * \param[in,out] params Audio (PCM) stream parameters to be set.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*params)(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params);

	/**
	 * \brief Fetches hardware stream parameters.
	 * \param[in] dev Component device.
	 * \param[out] params Receives copy of stream parameters retrieved from DAI HW settings.
	 * \param[in] dir Stream direction (enum sof_ipc_stream_direction).
	 * \return 0 on success, negative error code on failure.
	 */
	int (*dai_get_hw_params)(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params, int dir);

	/**
	 * \brief Configures attached DAI.
	 * \param[in,out] dd DAI data context.
	 * \param[in,out] dev Component device.
	 * \param[in] dai_config DAI configuration.
	 * \param[in] dai_spec_config DAI specific configuration data.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*dai_config)(struct dai_data *dd, struct comp_dev *dev,
			  struct ipc_config_dai *dai_config, const void *dai_spec_config);

#if CONFIG_IPC_MAJOR_3 || CONFIG_LIBRARY
	/**
	 * \brief Used to pass standard and bespoke commands (with optional data).
	 * \param[in,out] dev Component device.
	 * \param[in] cmd Command.
	 * \param[in,out] data Command data.
	 * \param[in] max_data_size Max size of returned data acceptable by caller for 'get' commands.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size);
#endif

	/**
	 * \brief Trigger, atomic - used to start/stop/pause stream operations.
	 * \param[in,out] dev Component device.
	 * \param[in] cmd Command, one of COMP_TRIGGER_...
	 * \return 0 on success, negative error code on failure.
	 */
	int (*trigger)(struct comp_dev *dev, int cmd);

	/**
	 * \brief Prepares component after params are set.
	 * \param[in,out] dev Component device.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*prepare)(struct comp_dev *dev);

	/**
	 * \brief Resets component.
	 * \param[in,out] dev Component device.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*reset)(struct comp_dev *dev);

	/**
	 * \brief Copy and process stream data from source to sink buffers.
	 * \param[in,out] dev Component device.
	 * \return Number of copied frames, or negative error code on failure.
	 */
	int (*copy)(struct comp_dev *dev);

	/**
	 * \brief Retrieves component rendering position.
	 * \param[in] dev Component device.
	 * \param[out] posn Receives reported position.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*position)(struct comp_dev *dev,
		struct sof_ipc_stream_posn *posn);

	/**
	 * \brief Gets attribute in component.
	 * \param[in] dev Component device.
	 * \param[in] type Attribute type.
	 * \param[out] value Attribute value buffer.
	 * \return 0 if succeeded, error code otherwise.
	 */
	int (*get_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/**
	 * \brief Sets attribute in component.
	 * \param[in,out] dev Component device.
	 * \param[in] type Attribute type.
	 * \param[in] value Attribute value.
	 * \return 0 if succeeded, error code otherwise.
	 */
	int (*set_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);

	/**
	 * \brief Configures timestamping in attached DAI.
	 * \param[in,out] dev Component device.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*dai_ts_config)(struct comp_dev *dev);

	/**
	 * \brief Starts timestamping.
	 * \param[in,out] dev Component device.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*dai_ts_start)(struct comp_dev *dev);

	/**
	 * \brief Stops timestamping.
	 * \param[in,out] dev Component device.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*dai_ts_stop)(struct comp_dev *dev);

	/**
	 * \brief Gets timestamp.
	 * \param[in] dev Component device.
	 * \param[out] tsd Receives timestamp data.
	 * \return 0 on success, negative error code on failure.
	 */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	int (*dai_ts_get)(struct comp_dev *dev, struct dai_ts_data *tsd);
#else
	int (*dai_ts_get)(struct comp_dev *dev,
			  struct timestamp_data *tsd);
#endif

	/**
	 * \brief Bind, atomic - used to notify component of bind event.
	 * \param[in,out] dev Component device.
	 * \param[in] bind_data Bind info.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*bind)(struct comp_dev *dev, const struct bind_info *bind_data);

	/**
	 * \brief Unbind, atomic - used to notify component of unbind event.
	 * \param[in,out] dev Component device.
	 * \param[in] unbind_data Unbind info.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*unbind)(struct comp_dev *dev, const struct bind_info *unbind_data);

	/**
	 * \brief Gets large config in component.
	 * \param[in,out] dev Component device.
	 * \param[in] param_id Parameter ID for each component.
	 * \param[in] first_block First block of large block.
	 * \param[in] last_block Last block of large block.
	 * \param[in,out] data_offset Block offset filled by callee.
	 * \param[out] data Block data buffer.
	 * \return 0 if succeeded, error code otherwise.
	 */
	int (*get_large_config)(struct comp_dev *dev, uint32_t param_id,
				bool first_block,
				bool last_block,
				uint32_t *data_offset,
				char *data);

	/**
	 * \brief Set large config in component.
	 * \param[in,out] dev Component device.
	 * \param[in] param_id Parameter ID for each component.
	 * \param[in] first_block First block of large block.
	 * \param[in] last_block Last block of large block.
	 * \param[in] data_offset Block offset in large block.
	 * \param[in] data Block data payload.
	 * \return 0 if succeeded, error code otherwise.
	 */
	int (*set_large_config)(struct comp_dev *dev, uint32_t param_id,
				bool first_block,
				bool last_block,
				uint32_t data_offset,
				const char *data);

	/**
	 * \brief Returns total data processed in number of bytes.
	 * \param[in] dev Component device.
	 * \param[in] stream_no Index of input/output stream.
	 * \param[in] input Selects between input (true) or output (false) stream direction.
	 * \return Total data processed if succeeded, 0 otherwise.
	 */
	uint64_t (*get_total_data_processed)(struct comp_dev *dev, uint32_t stream_no, bool input);
};

#if CONFIG_ZEPHYR_LOG
#define SOF_DRV_UID_NAME(drv) (drv)->uid
#else
#define SOF_DRV_UID_NAME(drv) (drv)->tctx->uuid_p
#endif

/**
 * Audio component base driver "class"
 * - used by all other component types.
 */
struct comp_driver {
	uint32_t type;					/**< SOF_COMP_ for driver */
	const struct sof_uuid *uid;			/**< Address to UUID value */
#if !CONFIG_ZEPHYR_LOG
	struct tr_ctx *tctx;				/**< Pointer to trace context */
#endif
	struct comp_ops ops;				/**< component operations */
	const struct module_interface *adapter_ops;	/**< module specific operations.
							  * Intended to replace the ops field.
							  * Currently used by module_adapter.
							  */
	struct k_heap *user_heap;			/**< Userspace heap */
	struct sof_uuid uid_cp;				/**< UUID copy for LLEXT modules */
};

/** \brief Holds constant pointer to component driver */
struct comp_driver_info {
	const struct comp_driver *drv;			/**< pointer to component driver */
	const struct module_interface **adapter_ops;	/**< pointer for updating ops */
	struct list_item list;				/**< list of component drivers */
};

/** \name Component processing domains
 *  @{
 */
#define COMP_PROCESSING_DOMAIN_LL 0	/**< Low latency processing domain */
#define COMP_PROCESSING_DOMAIN_DP 1	/**< Data processing / worker thread domain */
/** @}*/

/**
 * \brief Audio component base configuration from IPC at creation.
 */
struct comp_ipc_config {
	uint32_t core;			/**< Core we run on */
	uint32_t id;			/**< Component ID */
	uint32_t pipeline_id;		/**< Component pipeline ID */
	uint32_t proc_domain;		/**< Processing domain (COMP_PROCESSING_DOMAIN_*) */
	enum sof_comp_type type;	/**< Component type (SOF_COMP_*) */
	uint32_t periods_sink;		/**< Sink periods (0 means variable) */
	uint32_t periods_source;	/**< Source periods (0 means variable) */
	uint32_t frame_fmt;		/**< Frame format (SOF_IPC_FRAME_*) */
	uint32_t xrun_action;		/**< Action to take on XRUN */
#if CONFIG_IPC_MAJOR_4
	bool ipc_extended_init;		/**< True if extended init is included in IPC payload */
	uint32_t ipc_config_size;	/**< Size of config received by IPC */
#endif
};

/**
 * \brief Component performance measurement tracking data.
 */
struct comp_perf_data {
	size_t peak_of_measured_cpc;		/**< Maximum measured CPC at runtime */
	struct perf_data_item_comp *perf_data_item;	/**< Pointer to performance data item */
};

/**
 * \brief Audio component base device "class" used by all other component types.
 */
struct comp_dev {
	/* runtime */
	uint16_t state;		   /**< Component state (COMP_STATE_*) */
	uint32_t frames;	   /**< Number of frames copied to sink */
	bool expect_eos;	   /**< End of stream expected flag */
	struct pipeline *pipeline; /**< Parent pipeline reference */

	struct task *task;	/**< Component processing task (cross-core or DP) */
	uint32_t size;		/**< Component allocated size in bytes */
	uint32_t period;	/**< Component processing period in microseconds */
	uint32_t priority;	/**< Component processing priority */
	bool is_shared;		/**< True if component is shared across cores */
	struct comp_ipc_config ipc_config;	/**< Component IPC configuration */
#if !CONFIG_ZEPHYR_LOG
	struct tr_ctx tctx;	/**< Trace settings */
#endif

	/* common runtime configuration for downstream/upstream */
	uint32_t direction;	/**< Stream direction (enum sof_ipc_stream_direction) */
	bool direction_set;	/**< Flag indicating that direction has been set */

	const struct comp_driver *drv;	/**< Component driver */

	struct processing_module *mod;	/**< Module adapter reference */

	/* lists */
	struct list_item bsource_list;	/**< List of source buffers */
	struct list_item bsink_list;	/**< List of sink buffers */

	/* performance data*/
	struct comp_perf_data perf_data;	/**< Performance data */
	size_t ibs;		/**< Input Buffer Size for pin 0 */
	size_t obs;		/**< Output Buffer Size for pin 0 */
	size_t cpc;		/**< Max DSP cycles per chunk */
	size_t ll_chunk_size : 16;	/**< Size of 1ms input format chunk in bytes */

	/* private data - core does not touch this */
	void *priv_data;	/**< Driver private data */

#if CONFIG_PERFORMANCE_COUNTERS_COMPONENT
	struct perf_cnt_data pcd;	/**< Performance counter data */
#endif

#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	int32_t kcps_inc[CONFIG_CORE_COUNT];	/**< KCPS clock demand per core */
#endif
};

/**
 * \brief Get a pointer to the first comp_buffer object providing data to the component.
 * \param[in] component Target component device.
 * \return Pointer to first producer comp_buffer, or NULL if none.
 */
static inline struct comp_buffer *comp_dev_get_first_data_producer(struct comp_dev *component)
{
	return list_is_empty(&component->bsource_list) ? NULL :
	       list_first_item(&component->bsource_list, struct comp_buffer, sink_list);
}

/**
 * \brief Get a pointer to the next comp_buffer object providing data to the component.
 * \param[in] component Target component device.
 * \param[in] producer Current producer buffer.
 * \return Pointer to next producer comp_buffer, or NULL if none.
 */
static inline struct comp_buffer *comp_dev_get_next_data_producer(struct comp_dev *component,
								  struct comp_buffer *producer)
{
	return producer->sink_list.next == &component->bsource_list ? NULL :
	       list_item(producer->sink_list.next, struct comp_buffer, sink_list);
}

/**
 * \brief Safely get a pointer to the next comp_buffer object providing data to the component.
 * \param[in] component Target component device.
 * \param[in] producer Current producer buffer, or NULL.
 * \return Pointer to next producer comp_buffer, or NULL if none.
 */
static inline struct comp_buffer *comp_dev_get_next_data_producer_safe(struct comp_dev *component,
								       struct comp_buffer *producer)
{
	return producer ? comp_dev_get_next_data_producer(component, producer) : NULL;
}

/**
 * \brief Get a pointer to the first comp_buffer object receiving data from the component.
 * \param[in] component Target component device.
 * \return Pointer to first consumer comp_buffer, or NULL if none.
 */
static inline struct comp_buffer *comp_dev_get_first_data_consumer(struct comp_dev *component)
{
	return list_is_empty(&component->bsink_list) ? NULL :
	       list_first_item(&component->bsink_list, struct comp_buffer, source_list);
}

/**
 * \brief Get a pointer to the next comp_buffer object receiving data from the component.
 * \param[in] component Target component device.
 * \param[in] consumer Current consumer buffer.
 * \return Pointer to next consumer comp_buffer, or NULL if none.
 */
static inline struct comp_buffer *comp_dev_get_next_data_consumer(struct comp_dev *component,
								  struct comp_buffer *consumer)
{
	return consumer->source_list.next == &component->bsink_list ? NULL :
			list_item(consumer->source_list.next, struct comp_buffer, source_list);
}

/**
 * \brief Safely get a pointer to the next comp_buffer object receiving data from the component.
 * \param[in] component Target component device.
 * \param[in] consumer Current consumer buffer, or NULL.
 * \return Pointer to next consumer comp_buffer, or NULL if none.
 */
static inline struct comp_buffer *comp_dev_get_next_data_consumer_safe(struct comp_dev *component,
								       struct comp_buffer *consumer)
{
	return consumer ? comp_dev_get_next_data_consumer(component, consumer) : NULL;
}

/**
 * \brief Macro to iterate through component's list of producer buffers.
 * \param[in] _dev Target component device.
 * \param[out] _producer Buffer pointer iterator variable.
 */
#define comp_dev_for_each_producer(_dev, _producer)			\
	for (_producer = comp_dev_get_first_data_producer(_dev);	\
	     _producer != NULL;						\
	     _producer = comp_dev_get_next_data_producer(_dev, _producer))

/**
 * \brief Safe macro to iterate through component's producer buffers allowing buffer deletion.
 * \param[in] _dev Target component device.
 * \param[out] _producer Buffer pointer iterator variable.
 * \param[out] _next_producer Storage pointer for next buffer.
 */
#define comp_dev_for_each_producer_safe(_dev, _producer, _next_producer)		\
	for (_producer = comp_dev_get_first_data_producer(_dev),			\
	     _next_producer = comp_dev_get_next_data_producer_safe(_dev, _producer);	\
	     _producer != NULL;								\
	     _producer = _next_producer,						\
	     _next_producer = comp_dev_get_next_data_producer_safe(_dev, _producer))

/**
 * \brief Macro to iterate through component's list of consumer buffers.
 * \param[in] _dev Target component device.
 * \param[out] _consumer Buffer pointer iterator variable.
 */
#define comp_dev_for_each_consumer(_dev, _consumer)				\
	for (_consumer = comp_dev_get_first_data_consumer(_dev);		\
	     _consumer != NULL;							\
	     _consumer = comp_dev_get_next_data_consumer(_dev, _consumer))	\

/**
 * \brief Safe macro to iterate through component's consumer buffers allowing buffer deletion.
 * \param[in] _dev Target component device.
 * \param[out] _consumer Buffer pointer iterator variable.
 * \param[out] _next_consumer Storage pointer for next buffer.
 */
#define comp_dev_for_each_consumer_safe(_dev, _consumer, _next_consumer)		\
	for (_consumer = comp_dev_get_first_data_consumer(_dev),			\
	     _next_consumer = comp_dev_get_next_data_consumer_safe(_dev, _consumer);	\
	     _consumer != NULL;								\
	     _consumer = _next_consumer,						\
	     _next_consumer = comp_dev_get_next_data_consumer_safe(_dev, _consumer))

/* Common helper function used internally by the component implementations
 * begin here.
 */

/** \brief Struct for use with comp_get_copy_limits() function. */
struct comp_copy_limits {
	int frames;			/**< Number of frames that can be copied */
	int source_bytes;		/**< Available bytes in source */
	int sink_bytes;			/**< Free bytes in sink */
	int source_frame_bytes;		/**< Frame size in bytes in source */
	int sink_frame_bytes;		/**< Frame size in bytes in sink */
};

/**
 * \brief Retrieves Component ID from device.
 * \param[in] dev Component device.
 * \return Component ID.
 */
static inline uint32_t dev_comp_id(const struct comp_dev *dev)
{
	return dev->ipc_config.id;
}

/**
 * \brief Retrieves Component pipeline ID from device.
 * \param[in] dev Component device.
 * \return Component pipeline ID.
 */
static inline uint32_t dev_comp_pipe_id(const struct comp_dev *dev)
{
	return dev->ipc_config.pipeline_id;
}

/**
 * \brief Retrieves component type from device.
 * \param[in] dev Component device.
 * \return Component type (enum sof_comp_type).
 */
static inline enum sof_comp_type dev_comp_type(const struct comp_dev *dev)
{
	return dev->ipc_config.type;
}

/**
 * \brief Initialize common part of a component device.
 * \param[in] drv Parent component driver.
 * \param[in,out] dev Component device.
 * \param[in] bytes Size of the component device in bytes.
 */
static inline void comp_init(const struct comp_driver *drv,
			     struct comp_dev *dev, size_t bytes)
{
	dev->size = bytes;
	dev->drv = drv;
	dev->state = COMP_STATE_INIT;
	list_init(&dev->bsink_list);
	list_init(&dev->bsource_list);
#if !CONFIG_ZEPHYR_LOG
	memcpy_s(&dev->tctx, sizeof(dev->tctx),
		 trace_comp_drv_get_tr_ctx(dev->drv), sizeof(struct tr_ctx));
#endif
}

/**
 * \brief Allocates memory for the component device and initializes common part.
 * \param[in] drv Parent component driver.
 * \param[in] bytes Size of the component device in bytes.
 * \return Pointer to the allocated component device, or NULL on failure.
 */
static inline struct comp_dev *comp_alloc(const struct comp_driver *drv, size_t bytes)
{
	/*
	 * Use uncached address everywhere to access components to rule out
	 * multi-core failures. TODO: verify if cached alias may be used in some cases
	 */
	struct comp_dev *dev = sof_heap_alloc(drv->user_heap,
					      SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					      bytes, 0);

	if (!dev)
		return NULL;

	memset(dev, 0, sizeof(*dev));
	comp_init(drv, dev, bytes);

	return dev;
}

/**
 * \brief Frees memory allocated for component device.
 *
 * This is a counterpart to comp_alloc() and not to be confused with comp_free().
 *
 * \param[in,out] dev Pointer to the component device to free.
 */
static inline void comp_free_device(struct comp_dev *dev)
{
	sof_heap_free(dev->drv->user_heap, dev);
}

/**
 * \brief Module adapter associated with a component.
 * \param[in] dev Component device.
 * \return Pointer to associated processing module, or NULL if none.
 */
static inline struct processing_module *comp_mod(const struct comp_dev *dev)
{
	return dev->mod;
}

/**
 * \brief Assigns private data to component device.
 * \param[in,out] c Component device.
 * \param[in] data Private data pointer.
 */
#define comp_set_drvdata(c, data) \
	(c->priv_data = data)

/**
 * \brief Retrieves driver private data from component device.
 * \param[in] c Component device.
 * \return Private data pointer.
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
void sys_comp_module_dolby_dax_audio_processing_interface_init(void);
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
void sys_comp_module_phase_vocoder_interface_init(void);
void sys_comp_module_rtnr_interface_init(void);
void sys_comp_module_selector_interface_init(void);
void sys_comp_module_sound_dose_interface_init(void);
void sys_comp_module_src_interface_init(void);
void sys_comp_module_src_lite_interface_init(void);
void sys_comp_module_stft_process_interface_init(void);
void sys_comp_module_tdfb_interface_init(void);
void sys_comp_module_tone_interface_init(void);
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
 * \brief Registers the component driver on the list of available components.
 * \param[in,out] drv Component driver to be registered.
 * \return 0 if succeeded, error code otherwise.
 */
int comp_register(struct comp_driver_info *drv);

/**
 * \brief Unregisters the component driver from the list of available components.
 * \param[in,out] drv Component driver to be unregistered.
 */
void comp_unregister(struct comp_driver_info *drv);

/**
 * \brief Set adapter ops for a dynamically created driver.
 * \param[in,out] drv Component driver to update.
 * \param[in] ops Module interface operations.
 * \return 0 or a negative error code.
 */
int comp_set_adapter_ops(const struct comp_driver *drv, const struct module_interface *ops);

/** @}*/

/**
 * \brief Component state transition handling.
 * \param[in,out] dev Component device.
 * \param[in] cmd Command, one of COMP_TRIGGER_...
 * \return 0 if succeeded, error code otherwise.
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
 * \brief Set component period frames based on sample rate and period.
 * \param[in,out] current Target component device.
 * \param[in] rate Sample rate in Hz.
 */
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
 * \brief Called by the component device when underrun is detected.
 * \param[in,out] dev Component device.
 * \param[in] source Source buffer.
 * \param[in] copy_bytes Requested size of data in bytes.
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
 * \brief Called by component device when overrun is detected.
 * \param[in,out] dev Component device.
 * \param[in] sink Sink buffer.
 * \param[in] copy_bytes Requested size of free space in bytes.
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
 * \brief Computes source to sink copy operation boundaries including maximum number
 * of frames that can be transferred.
 *
 * \param[in] source Source buffer.
 * \param[in] sink Sink buffer.
 * \param[out] cl Current copy limits.
 */
void comp_get_copy_limits(struct comp_buffer *source,
			  struct comp_buffer *sink,
			  struct comp_copy_limits *cl);

/**
 * \brief Computes source to sink copy operation boundaries including maximum number
 * of frames aligned with requirement that can be transferred.
 *
 * \param[in] source Buffer of source.
 * \param[in] sink Buffer of sink.
 * \param[out] cl Current copy limits.
 */
void comp_get_copy_limits_frame_aligned(const struct comp_buffer *source,
					const struct comp_buffer *sink,
					struct comp_copy_limits *cl);

/**
 * \brief Get component state.
 * \param[in] dev Component from which user wants to read status.
 * \retval COMP_STATE_NOT_EXIST If there's no connected component.
 * \return State of the component (COMP_STATE_*).
 */
static inline int comp_get_state(const struct comp_dev *dev)
{
	if (!dev)
		return COMP_STATE_NOT_EXIST;
	return dev->state;
}

/**
 * \brief Provide state of a component connected to a buffer as a data provider.
 * \param[in] buffer Interconnect buffer to be checked.
 * \retval COMP_STATE_NOT_EXIST If there's no connected component.
 * \return State of the component.
 */
static inline int comp_buffer_get_source_state(const struct comp_buffer *buffer)
{
	return comp_get_state(comp_buffer_get_source_component(buffer));
}

/**
 * \brief Provide state of a component connected to a buffer as a data consumer.
 * \param[in] buffer Interconnect buffer to be checked.
 * \retval COMP_STATE_NOT_EXIST If there's no connected component.
 * \return State of the component.
 */
static inline int comp_buffer_get_sink_state(const struct comp_buffer *buffer)
{
	return comp_get_state(comp_buffer_get_sink_component(buffer));
}

/**
 * \brief Verify and set component audio parameters.
 *
 * Called by component in params() function in order to set and update some of
 * downstream (playback) or upstream (capture) buffer parameters with PCM
 * parameters.
 *
 * \param[in,out] dev Component device.
 * \param[in] flag Specifies which parameter should not be updated.
 * \param[in,out] params PCM stream parameters.
 * \return 0 on success, negative error code on failure.
 */
int comp_verify_params(struct comp_dev *dev, uint32_t flag,
		       struct sof_ipc_stream_params *params);

/**
 * \brief Update IBS, OBS, CPC, LL chunk size for component.
 * \param[in,out] dev Component to update.
 */
void comp_update_ibs_obs_cpc(struct comp_dev *dev);

/**
 * \brief Initialize performance data fields if component has assigned slot in measurement window.
 * \param[in,out] dev Component to init.
 */
void comp_init_performance_data(struct comp_dev *dev);

/**
 * \brief Update performance data entry for component and check for budget violation.
 * \param[in,out] dev Component to update.
 * \param[in] cycles_used Execution time.
 * \return True if budget violation occurred, false otherwise.
 */
bool comp_update_performance_data(struct comp_dev *dev, uint32_t cycles_used);

/**
 * \brief Get memory region flag for component buffer allocations.
 * \param[in] drv Component driver pointer.
 * \return Memory flag (SOF_MEM_FLAG_*).
 */
static inline int user_get_buffer_memory_region(const struct comp_driver *drv)
{
#if CONFIG_SOF_USERSPACE_USE_DRIVER_HEAP
	return drv->user_heap ? SOF_MEM_FLAG_USER_SHARED_BUFFER : SOF_MEM_FLAG_USER;
#else
	return SOF_MEM_FLAG_USER;
#endif
}

/** @}*/

#endif /* __SOF_AUDIO_COMPONENT_H__ */
