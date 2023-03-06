/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_H__

#include <../include/list.h>
#include <../include/ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;
struct ipc;
struct ipc_msg;

/*
 * Pipeline status to stop execution of current path, but to keep the
 * pipeline alive, when processing COMP_TRIGGER_STOP or COMP_TRIGGER_PAUSE
 */
#define PPL_STATUS_PATH_STOP		1
/*
 * Pipeline status to stop execution of current path, and to terminate the
 * pipeline, when processing COMP_TRIGGER_STOP or COMP_TRIGGER_PAUSE
 */
#define PPL_STATUS_PATH_TERMINATE	2

/* Pipeline scheduled in another thread other than ipc thread */
#define PPL_STATUS_SCHEDULED	2

/* pipeline connection directions */
#define PPL_CONN_DIR_COMP_TO_BUFFER	0
#define PPL_CONN_DIR_BUFFER_TO_COMP	1

/* pipeline processing directions */
#define PPL_DIR_DOWNSTREAM	0
#define PPL_DIR_UPSTREAM	1

/*
 * Audio pipeline.
 */

struct tr_ctx {
	const struct sof_uuid_entry *uuid_p;	/**< UUID pointer, use SOF_UUID() to init */
	uint32_t level;				/**< Default log level */
};

struct pipeline {
	uint32_t comp_id;	/**< component id for pipeline */
	uint32_t pipeline_id;	/**< pipeline id */
	uint32_t sched_id;	/**< Scheduling component id */
	uint32_t core;		/**< core we run on */
	uint32_t period;	/**< execution period in us*/
	uint32_t priority;	/**< priority level 0 (low) to 10 (max) */
	uint32_t period_mips;	/**< worst case instruction count per period */
	uint32_t frames_per_sched;/**< output frames of pipeline, 0 is variable */
	uint32_t xrun_limit_usecs; /**< report xruns greater than limit */
	uint32_t time_domain;	/**< scheduling time domain */
	uint32_t attributes;   /**< pipeline attributes from IPC extension msg/ */

	/* runtime status */
	int32_t xrun_bytes;		/* last xrun length */
	uint32_t status;		/* pipeline status */
	struct tr_ctx tctx;		/* trace settings */

	/* scheduling */
	struct task *pipe_task;		/* pipeline processing task */
	struct pipeline *sched_next;	/* pipeline scheduled after this */
	struct pipeline *sched_prev;	/* pipeline scheduled before this */

	/* component that drives scheduling in this pipe */
	struct comp_dev *sched_comp;
	/* source component for this pipe */
	struct comp_dev *source_comp;
	/* sink component for this pipe */
	struct comp_dev *sink_comp;

	struct list_item list;	/**< list in walk context */

	/* position update */
	uint32_t posn_offset;		/* position update array offset*/
	struct ipc_msg *msg;
	struct {
		int cmd;
		struct comp_dev *host;
		unsigned int delay;	/* period count */
		bool aborted;		/* STOP or PAUSE failed, stay active */
		bool pending;		/* trigger scheduled but not executed yet */
	} trigger;
};

struct pipeline_walk_context {
	int (*comp_func)(struct comp_dev *cd, struct comp_buffer *buffer,
			 struct pipeline_walk_context *ctx, int dir);
	void *comp_data;
	void (*buff_func)(struct comp_buffer *buffer, void *data);
	void *buff_data;
	struct comp_buffer *incoming;
	/**< pipelines to be scheduled after trigger walk */
	struct list_item pipelines;
	/*
	 * If this flag is set, pipeline_for_each_comp() will skip all
	 * incompletely initialised components, i.e. those, whose .pipeline ==
	 * NULL. Such components should not be skipped during initialisation
	 * and clean up, but they should be skipped during streaming.
	 */
	bool skip_incomplete;
};

/* generic pipeline data used by pipeline_comp_* functions */
struct pipeline_data {
	struct comp_dev *start;
	struct sof_ipc_pcm_params *params;
	struct sof_ipc_stream_posn *posn;
	struct pipeline *p;
	int cmd;
	uint32_t delay_ms;		/* between PRE_{START,RELEASE} and {START,RELEASE} */
};

/** \brief Task type registered by pipelines. */
struct pipeline_task {
	struct task task;		/**< parent structure */
	bool registrable;		/**< should task be registered on irq */
	struct comp_dev *sched_comp;	/**< pipeline scheduling component */
};

#define pipeline_task_get(t) container_of(t, struct pipeline_task, task)

/*
 * Pipeline Graph APIs
 *
 * These APIs are used to construct and bind pipeline graphs. They are also
 * used to query pipeline fundamental configuration.
 */

/**
 * \brief Creates a new pipeline.
 * \param[in] pipeline_id Pipeline ID number.
 * \param[in] priority Pipeline scheduling priority.
 * \param[in] comp_id Pipeline component ID number.
 * \return New pipeline pointer or NULL.
 */
struct pipeline *pipeline_new(uint32_t pipeline_id, uint32_t priority, uint32_t comp_id);

/**
 * \brief Free's a pipeline.
 * \param[in] p pipeline.
 * \return 0 on success.
 */
int pipeline_free(struct pipeline *p);

/**
 * \brief Connect components in a pipeline.
 * \param[in] comp connecting component.
 * \param[in] buffer connecting buffer.
 * \param[in] dir Connection direction.
 * \return 0 on success.
 */
int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir);

/**
 * \brief Creates a new pipeline.
 * \param[in] comp connecting component.
 * \param[in] buffer connecting buffer.
 * \param[in] dir Connection direction.
 */
void pipeline_disconnect(struct comp_dev *comp, struct comp_buffer *buffer,
			 int dir);

/**
 * \brief Completes a pipeline.
 * \param[in] p pipeline.
 * \param[in] source Pipeline component device.
 * \param[in] sink Pipeline component device.
 * \return 0 on success.
 */
int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink);

/**
 * \brief Initializes pipeline position structure.
 * \param[in,out] sof Pointer to sof structure.
 */
void pipeline_posn_init(struct sof *sof);

/**
 * \brief Resets the pipeline and free runtime resources.
 * \param[in] p pipeline.
 * \param[in] host_cd Host DMA component device.
 * \return 0 on success.
 */
int pipeline_reset(struct pipeline *p, struct comp_dev *host_cd);

/**
 * \brief Walks the pipeline graph for each component.
 * \param[in] current Current pipeline component.
 * \param[in] ctx Pipeline graph walk context.
 * \param[in] dir Walk direction.
 * \return 0 on success.
 */
int pipeline_for_each_comp(struct comp_dev *current,
			   struct pipeline_walk_context *ctx, int dir);

/**
 * \brief Walks pipeline graph to find dai component.
 * \param[in] pipeline_id is the start pipeline id.
 * \param[in] dir is the direction of the traversal.
 * \return dai component.
 */
struct comp_dev *pipeline_get_dai_comp(uint32_t pipeline_id, int dir);

#if CONFIG_IPC_MAJOR_4
/**
 * \brief Walks pipeline graph to find dai component and latency.
 * \param[in] pipeline_id is the start pipeline id.
 * \param[out] latency to dai.
 * \return dai component.
 */
struct comp_dev *pipeline_get_dai_comp_latency(uint32_t pipeline_id, uint32_t *latency);
#endif

/**
 * Retrieves pipeline id from pipeline.
 * @param p pipeline.
 * @return pipeline id.
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
 * \brief Creates a new pipeline.
 * \param[in] p pipeline.
 * \param[in] cd Pipeline component device.
 * \param[in] params Pipeline parameters.
 * \return 0 on success.
 */
int pipeline_params(struct pipeline *p, struct comp_dev *cd,
		    struct sof_ipc_pcm_params *params);

/**
 * \brief Creates a new pipeline.
 * \param[in] p pipeline.
 * \param[in,out] cd Pipeline component device.
 * \return 0 on success.
 */
int pipeline_prepare(struct pipeline *p, struct comp_dev *cd);

/*
 * Pipeline stream APIs
 *
 * These APIs are used to control pipeline processing work.
 */

/**
 * \brief Trigger pipeline - IPC context
 * \param[in] p pipeline.
 * \param[in] host Host DMA component.
 * \param[in] cmd Pipeline trigger command.
 * \return 0 on success.
 */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host, int cmd);

/**
 * \brief Trigger pipeline - either IPC or pipeline task context
 * \param[in] p pipeline.
 * \param[in] host Host DMA component.
 * \param[in] cmd Pipeline trigger command.
 * \return 0 on success.
 */
int pipeline_trigger_run(struct pipeline *p, struct comp_dev *host, int cmd);

/**
 * \brief Copy data along a pipeline.
 * \param[in] p pipeline.
 * \return 0 on success.
 */
int pipeline_copy(struct pipeline *p);

/**
 * \brief Get time pipeline timestamps from host to dai.
 * \param[in] p pipeline.
 * \param[in] host_dev Host DMA component.
 * \param[in,out] posn Pipeline stream position.
 */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host_dev,
			    struct sof_ipc_stream_posn *posn);

/*
 * Pipeline scheduling APIs
 *
 * These APIs are used to schedule pipeline processing work.
 */

/**
 * \brief Checks if two pipelines have the same scheduling component.
 * \param[in] current This pipeline.
 * \param[in] previous Other pipeline.
 * \return true if both pipelines are scheduled together.
 */
static inline bool pipeline_is_same_sched_comp(struct pipeline *current,
					       struct pipeline *previous)
{
	return current->sched_comp == previous->sched_comp;
}

/**
 * \brief Is pipeline is scheduled with timer.
 * \param[in] p pipeline.
 * \return true if pipeline uses timer based scheduling.
 */
static inline bool pipeline_is_timer_driven(struct pipeline *p)
{
	return p->time_domain == SOF_TIME_DOMAIN_TIMER;
}

/**
 * \brief Is pipeline is scheduled on this core.
 * \param[in] p pipeline.
 * \return true if pipeline core ID == current core ID.
 */
static inline bool pipeline_is_this_cpu(struct pipeline *p)
{
	return p->core == cpu_get_id();
}

/**
 * \brief Free's a pipeline.
 * \param[in] p pipeline.
 * \return 0 on success.
 */
int pipeline_comp_task_init(struct pipeline *p);

/**
 * \brief Free's a pipeline.
 * \param[in] p pipeline.
 * \param[in] start Pipelien start time in microseconds.
 */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start);

/**
 * \brief Trigger pipeline's scheduling component.
 * \param[in] p pipeline.
 * \param[in,out] comp Pipeline component device.
 * \param[in] ctx Pipeline graph walk context.
 */
void pipeline_comp_trigger_sched_comp(struct pipeline *p,
				      struct comp_dev *comp,
				      struct pipeline_walk_context *ctx);

/**
 * \brief Schedule all triggered pipelines.
 * \param[in] ctx Pipeline graph walk context.
 * \param[in] cmd Trigger command.
 */
void pipeline_schedule_triggered(struct pipeline_walk_context *ctx,
				 int cmd);

/**
 * \brief Configure pipeline scheduling.
 * \param[in] p pipeline.
 * \param[in] sched_id Scheduling component ID.
 * \param[in] core DSP core pipeline runs on.
 * \param[in] period Pipeline scheduling period in us.
 * \param[in] period_mips Pipeline worst case MCPS per period.
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
 * \brief Recover the pipeline from a XRUN condition.
 * \param[in] p pipeline.
 * \return 0 on success.
 */
int pipeline_xrun_recover(struct pipeline *p);

/**
 * \brief Perform xrun recovery.
 * \param[in] p pipeline.
 * \param[in] cmd Trigger command.
 * \return 0 on success.
 */
int pipeline_xrun_handle_trigger(struct pipeline *p, int cmd);

/**
 * \brief notify host that we have XRUN.
 * \param[in] p pipeline.
 * \param[in] dev Pipeline component device.
 * \param[in] bytes Number of bytes we have over or under run.
 */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes);

/**
 * \brief Set tolerance for pipeline xrun handling.
 * \param[in] p pipeline.
 * \param[in] xrun_limit_usecs Limit in micro secs that pipeline will tolerate.
 */
int pipeline_xrun_set_limit(struct pipeline *p, uint32_t xrun_limit_usecs);

#endif /* __SOF_AUDIO_PIPELINE_H__ */
