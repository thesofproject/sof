/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * @file sof/audio/pipeline.h
 * @brief Audio pipeline management, graph scheduling, and execution control.
 * @ingroup audio_pipeline_api
 */

#ifndef __SOF_AUDIO_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_H__

#include <sof/lib/cpu.h>
#include <sof/list.h>
#include <rtos/task.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <sof/audio/pipeline-trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/** \addtogroup audio_pipeline_api
 *  @{
 */

struct comp_buffer;
struct comp_dev;
struct ipc;
struct ipc_msg;
struct k_heap;
struct mod_alloc_ctx;

/**
 * \brief Pipeline status to stop execution of current path, but to keep the
 * pipeline alive, when processing COMP_TRIGGER_STOP or COMP_TRIGGER_PAUSE.
 */
#define PPL_STATUS_PATH_STOP		1

/**
 * \brief Pipeline status to stop execution of current path, and to terminate the
 * pipeline, when processing COMP_TRIGGER_STOP or COMP_TRIGGER_PAUSE.
 */
#define PPL_STATUS_PATH_TERMINATE	2

/** \brief Pipeline scheduled in another thread other than IPC thread. */
#define PPL_STATUS_SCHEDULED	2

/** \name Pipeline connection directions
 *  @{
 */
#define PPL_CONN_DIR_COMP_TO_BUFFER	0	/**< Connection from component to buffer */
#define PPL_CONN_DIR_BUFFER_TO_COMP	1	/**< Connection from buffer to component */
/** @}*/

/** \name Pipeline processing directions
 *  @{
 */
#define PPL_DIR_DOWNSTREAM	0	/**< Downstream playback/processing direction */
#define PPL_DIR_UPSTREAM	1	/**< Upstream capture/processing direction */
/** @}*/

/**
 * \brief Audio pipeline structure representing a stream graph and execution context.
 */
struct pipeline {
	struct mod_alloc_ctx *alloc;	/**< Alloc context used for allocating this pipeline */
	uint32_t comp_id;	/**< Component ID for pipeline */
	uint32_t pipeline_id;	/**< Pipeline ID */
	uint32_t sched_id;	/**< Scheduling component ID */
	uint32_t core;		/**< Core we run on */
	uint32_t period;	/**< Execution period in microseconds */
	uint32_t priority;	/**< Priority level 0 (low) to 10 (max) */
	uint32_t period_mips;	/**< Worst case instruction count per period */
	uint32_t frames_per_sched;/**< Output frames of pipeline, 0 is variable */
	uint32_t xrun_limit_usecs; /**< Report xruns greater than limit */
	uint32_t time_domain;	/**< Scheduling time domain */
	uint32_t attributes;   /**< Pipeline attributes from IPC extension message */

	/* runtime status */
	int32_t xrun_bytes;		/**< Last xrun length in bytes */
	uint32_t status;		/**< Pipeline status flags */
	struct tr_ctx tctx;		/**< Trace settings */

	/* scheduling */
#ifdef CONFIG_IPC_MAJOR_4
	struct task *trigger_task;	/**< IPC4 pipeline trigger task */
#endif
	struct task *pipe_task;		/**< IPC3 pipeline processing task or IPC4 pipeline copy task */
	struct pipeline *sched_next;	/**< Pipeline scheduled after this */
	struct pipeline *sched_prev;	/**< Pipeline scheduled before this */

	/* component that drives scheduling in this pipe */
	struct comp_dev *sched_comp;	/**< Component that drives scheduling in this pipe */
	/* source component for this pipe */
	struct comp_dev *source_comp;	/**< Source component for this pipe */
	/* sink component for this pipe */
	struct comp_dev *sink_comp;	/**< Sink component for this pipe */

	struct list_item list;	/**< List item in walk context */

	/* position update */
	uint32_t posn_offset;		/**< Position update array offset */
	struct ipc_msg *msg;		/**< IPC message pointer for position updates */
	struct {
		int cmd;		/**< Trigger command */
		struct comp_dev *host;	/**< Associated host component device */
		unsigned int delay;	/**< Period count delay */
		bool aborted;		/**< STOP or PAUSE failed, stay active */
		bool pending;		/**< Trigger scheduled but not executed yet */
	} trigger;			/**< Pipeline trigger control structure */
};

/**
 * \brief Pipeline traversal walk context for graph exploration and operations.
 */
struct pipeline_walk_context {
	/** Callback function invoked for each component in walk */
	int (*comp_func)(struct comp_dev *cd, struct comp_buffer *buffer,
			 struct pipeline_walk_context *ctx, int dir);
	void *comp_data;	/**< Component callback user data */
	/** Callback function invoked for each buffer in walk */
	void (*buff_func)(struct comp_buffer *buffer, void *data);
	void *buff_data;	/**< Buffer callback user data */
	struct comp_buffer *incoming;	/**< Incoming buffer context */
	struct list_item pipelines;	/**< Pipelines to be scheduled after trigger walk */
	/*
	 * If this flag is set, pipeline_for_each_comp() will skip all
	 * incompletely initialised components, i.e. those, whose .pipeline ==
	 * NULL. Such components should not be skipped during initialisation
	 * and clean up, but they should be skipped during streaming.
	 */
	bool skip_incomplete;	/**< Skip incompletely initialized components */
};

/**
 * \brief Generic pipeline data used by pipeline_comp_* functions.
 */
struct pipeline_data {
	struct comp_dev *start;			/**< Starting component device */
	struct sof_ipc_pcm_params *params;	/**< Stream PCM parameters */
	struct sof_ipc_stream_posn *posn;	/**< Stream position structure */
	struct pipeline *p;			/**< Target pipeline */
	int cmd;				/**< Command code */
	uint32_t delay_ms;			/**< Delay in milliseconds between pre and main trigger phases */
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
	int32_t kcps[CONFIG_CORE_COUNT];	/**< The max count of KCPS per core */
#endif
};

/** \brief Task type registered by pipelines. */
struct pipeline_task {
	struct task task;		/**< Parent task structure */
	bool registrable;		/**< Should task be registered on IRQ */
	struct comp_dev *sched_comp;	/**< Pipeline scheduling component */
};

struct ipc4_pipeline_ext_obj_mem_data;

/** \brief For storing IPC payload data during pipeline creation. */
struct create_pipeline_params {
	const struct ipc4_pipeline_ext_obj_mem_data *mem_data;	/**< IPC4 pipeline external memory descriptor */
};

/** \brief Retrieves the enclosing pipeline_task structure from a task pointer. */
#define pipeline_task_get(t) container_of(t, struct pipeline_task, task)

/*
 * Pipeline Graph APIs
 *
 * These APIs are used to construct and bind pipeline graphs. They are also
 * used to query pipeline fundamental configuration.
 */

/**
 * \brief Creates a new pipeline.
 * \param[in,out] heap Heap to allocate the pipeline on, or NULL for default.
 * \param[in] pipeline_id Pipeline ID number.
 * \param[in] priority Pipeline scheduling priority.
 * \param[in] comp_id Pipeline component ID number.
 * \param[in] pparams Pipeline parameters from IPC payload, may be NULL.
 * \return Pointer to new pipeline on success, or NULL on failure.
 */
struct pipeline *pipeline_new(struct k_heap *heap, uint32_t pipeline_id, uint32_t priority,
			      uint32_t comp_id, struct create_pipeline_params *pparams);

/**
 * \brief Frees a pipeline and its associated resources.
 * \param[in,out] p Pipeline to free.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_free(struct pipeline *p);

/**
 * \brief Connect components in a pipeline.
 * \param[in,out] comp Connecting component device.
 * \param[in,out] buffer Connecting buffer.
 * \param[in] dir Connection direction (PPL_CONN_DIR_*).
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir);

/**
 * \brief Disconnect components and buffers in a pipeline.
 * \param[in,out] comp Connecting component device.
 * \param[in,out] buffer Connecting buffer.
 * \param[in] dir Connection direction (PPL_CONN_DIR_*).
 */
void pipeline_disconnect(struct comp_dev *comp, struct comp_buffer *buffer,
			 int dir);

/**
 * \brief Completes a pipeline setup by verifying connectivity between source and sink.
 * \param[in,out] p Pipeline to complete.
 * \param[in,out] source Pipeline source component device.
 * \param[in,out] sink Pipeline sink component device.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink);

/**
 * \brief Initializes pipeline position structure.
 * \param[in,out] sof Pointer to global SOF system context.
 */
void pipeline_posn_init(struct sof *sof);

#ifdef CONFIG_SOF_USERSPACE_LL
/**
 * \brief Grants user-space thread access to pipeline position mutex.
 * \param[in,out] thread Thread to grant access to.
 */
void pipeline_posn_grant_access(struct k_thread *thread);
#endif

/**
 * \brief Resets the pipeline and frees runtime resources.
 * \param[in,out] p Pipeline to reset.
 * \param[in,out] host_cd Host DMA component device.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_reset(struct pipeline *p, struct comp_dev *host_cd);

/**
 * \brief Sets End Of Stream state for all devices in the pipeline.
 * \param[in,out] p Target pipeline.
 * \param[in] eos End Of Stream flag state.
 */
void pipeline_set_eos(struct pipeline *p, bool eos);

/**
 * \brief Walks the pipeline graph for each component.
 * \param[in,out] current Current pipeline component to start traversal from.
 * \param[in,out] ctx Pipeline graph walk context.
 * \param[in] dir Walk direction (PPL_DIR_*).
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_for_each_comp(struct comp_dev *current,
			   struct pipeline_walk_context *ctx, int dir);

/**
 * \brief Walks pipeline graph to find DAI component.
 * \param[in] pipeline_id Start pipeline ID.
 * \param[in] dir Direction of traversal (PPL_DIR_*).
 * \return Pointer to DAI component device, or NULL if not found.
 */
struct comp_dev *pipeline_get_dai_comp(uint32_t pipeline_id, int dir);

#if CONFIG_IPC_MAJOR_4
/**
 * \brief Walks pipeline graph to find DAI component and latency.
 * \param[in] pipeline_id Start pipeline ID.
 * \param[out] latency Receives latency to DAI as a count of buffered processing blocks (input_data/ibs - output_data/obs).
 * \return Pointer to DAI component device, or NULL if not found.
 */
struct comp_dev *pipeline_get_dai_comp_latency(uint32_t pipeline_id, uint32_t *latency);
#endif

/**
 * \brief Retrieves pipeline ID from pipeline structure.
 * \param[in] p Target pipeline.
 * \return Pipeline ID.
 */
static inline uint32_t pipeline_id(struct pipeline *p)
{
	return p->pipeline_id;
}

/*
 * Pipeline configuration APIs
 *
 * These APIs are used to configure the runtime parameters of a pipeline.
 */

/**
 * \brief Sets pipeline runtime audio stream parameters.
 * \param[in,out] p Pipeline to configure.
 * \param[in,out] cd Pipeline component device.
 * \param[in] params Stream PCM parameters.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_params(struct pipeline *p, struct comp_dev *cd,
		    struct sof_ipc_pcm_params *params);

/**
 * \brief Prepares pipeline for audio processing.
 * \param[in,out] p Pipeline to prepare.
 * \param[in,out] cd Pipeline component device.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_prepare(struct pipeline *p, struct comp_dev *cd);

/*
 * Pipeline stream APIs
 *
 * These APIs are used to control pipeline processing work.
 */

/**
 * \brief Trigger pipeline in IPC context.
 * \param[in,out] p Target pipeline.
 * \param[in,out] host Host DMA component device.
 * \param[in] cmd Pipeline trigger command (COMP_TRIGGER_*).
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host, int cmd);

/**
 * \brief Trigger pipeline in either IPC or pipeline task context.
 * \param[in,out] p Target pipeline.
 * \param[in,out] host Host DMA component device.
 * \param[in] cmd Pipeline trigger command (COMP_TRIGGER_*).
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_trigger_run(struct pipeline *p, struct comp_dev *host, int cmd);

/**
 * \brief Copy and process stream data along a pipeline.
 * \param[in,out] p Target pipeline.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_copy(struct pipeline *p);

/**
 * \brief Get pipeline timestamps from host to DAI.
 * \param[in] p Target pipeline.
 * \param[in] host_dev Host DMA component device.
 * \param[in,out] posn Pipeline stream position structure to populate.
 */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host_dev,
			    struct sof_ipc_stream_posn *posn);

/*
 * Pipeline scheduling APIs
 *
 * These APIs are used to schedule pipeline processing work.
 */

/**
 * \brief Checks if two pipelines share the same scheduling component.
 * \param[in] current First pipeline.
 * \param[in] previous Second pipeline.
 * \return True if both pipelines are scheduled together, false otherwise.
 */
static inline bool pipeline_is_same_sched_comp(struct pipeline *current,
					       struct pipeline *previous)
{
	return current->sched_comp == previous->sched_comp;
}

/**
 * \brief Checks if pipeline is scheduled with timer.
 * \param[in] p Target pipeline.
 * \return True if pipeline uses timer-based scheduling, false otherwise.
 */
static inline bool pipeline_is_timer_driven(struct pipeline *p)
{
	return p->time_domain == SOF_TIME_DOMAIN_TIMER;
}

/**
 * \brief Checks if pipeline is scheduled on the current core.
 * \param[in] p Target pipeline.
 * \return True if pipeline core ID matches current core ID, false otherwise.
 */
static inline bool pipeline_is_this_cpu(struct pipeline *p)
{
	return p->core == cpu_get_id();
}

/**
 * \brief Initializes a low-latency (LL) task for a pipeline.
 * \param[in,out] p Target pipeline.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_comp_ll_task_init(struct pipeline *p);

/**
 * \brief Frees the low-latency (LL) tasks owned by a pipeline.
 * \param[in,out] p Target pipeline.
 */
void pipeline_comp_ll_task_free(struct pipeline *p);

/**
 * \brief Initializes a data processing (DP) task for a component.
 * \param[in,out] comp Component device for which the task is created.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_comp_dp_task_init(struct comp_dev *comp);

/**
 * \brief Schedules a pipeline copy operation to run after a delay.
 * \param[in,out] p Target pipeline.
 * \param[in] start Pipeline start time in microseconds.
 */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start);

/**
 * \brief Triggers pipeline's scheduling component.
 * \param[in,out] p Target pipeline.
 * \param[in,out] comp Pipeline component device.
 * \param[in,out] ctx Pipeline graph walk context.
 */
void pipeline_comp_trigger_sched_comp(struct pipeline *p,
				      struct comp_dev *comp,
				      struct pipeline_walk_context *ctx);

/**
 * \brief Schedules all triggered pipelines collected during traversal.
 * \param[in,out] ctx Pipeline graph walk context.
 * \param[in] cmd Trigger command (COMP_TRIGGER_*).
 */
void pipeline_schedule_triggered(struct pipeline_walk_context *ctx,
				 int cmd);

/**
 * \brief Configures pipeline scheduling parameters.
 * \param[in,out] p Target pipeline.
 * \param[in] sched_id Scheduling component ID.
 * \param[in] core DSP core the pipeline runs on.
 * \param[in] period Pipeline scheduling period in microseconds.
 * \param[in] period_mips Pipeline worst-case MCPS per period.
 * \param[in] frames_per_sched Pipeline frames processed per schedule.
 * \param[in] time_domain Pipeline scheduling time domain.
 */
void pipeline_schedule_config(struct pipeline *p, uint32_t sched_id,
			      uint32_t core, uint32_t period,
			      uint32_t period_mips, uint32_t frames_per_sched,
			      uint32_t time_domain);

/*
 * Pipeline error handling APIs
 *
 * These APIs are used to handle, report and recover from pipeline errors.
 */

/**
 * \brief Recovers the pipeline from an XRUN condition.
 * \param[in,out] p Target pipeline.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_xrun_recover(struct pipeline *p);

/**
 * \brief Performs XRUN recovery trigger handling.
 * \param[in,out] p Target pipeline.
 * \param[in] cmd Trigger command (COMP_TRIGGER_*).
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_xrun_handle_trigger(struct pipeline *p, int cmd);

/**
 * \brief Notifies host and pipeline that an XRUN condition occurred.
 * \param[in,out] p Target pipeline.
 * \param[in,out] dev Pipeline component device where XRUN occurred.
 * \param[in] bytes Number of overrun or underrun bytes.
 */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes);

/**
 * \brief Sets tolerance limit for pipeline XRUN handling.
 * \param[in,out] p Target pipeline.
 * \param[in] xrun_limit_usecs Limit in microseconds that pipeline will tolerate before reporting.
 * \return 0 on success, or negative error code on failure.
 */
int pipeline_xrun_set_limit(struct pipeline *p, uint32_t xrun_limit_usecs);

/**
 * \brief Sends an IPC notification that an error occurred in a module's processing function.
 * \param[in] component Component device in which the error occurred.
 * \param[in] error_code Error code to report.
 */
void pipeline_comp_copy_error_notify(const struct comp_dev *component, int error_code);

/** @}*/

#endif /* __SOF_AUDIO_PIPELINE_H__ */
