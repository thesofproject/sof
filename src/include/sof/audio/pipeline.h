/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_PIPELINE_H__
#define __SOF_AUDIO_PIPELINE_H__

#include <sof/lib/cpu.h>
#include <sof/schedule/task.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stdbool.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;
struct sof_ipc_buffer;
struct sof_ipc_pcm_params;
struct sof_ipc_stream_posn;
struct ipc;

/*
 * This flag disables firmware-side xrun recovery.
 * It should remain enabled in the situation when the
 * recovery is delegated to the outside of firmware.
 */
#define NO_XRUN_RECOVERY 1

/* pipeline tracing */
#define trace_pipe(format, ...) \
	trace_event(TRACE_CLASS_PIPE, format, ##__VA_ARGS__)
#define trace_pipe_with_ids(pipe_ptr, format, ...)		\
	trace_event_with_ids(TRACE_CLASS_PIPE,			\
			     pipe_ptr->ipc_pipe.pipeline_id,	\
			     pipe_ptr->ipc_pipe.comp_id,	\
			     format, ##__VA_ARGS__)

#define trace_pipe_error(format, ...) \
	trace_error(TRACE_CLASS_PIPE, format, ##__VA_ARGS__)
#define trace_pipe_error_with_ids(pipe_ptr, format, ...)	\
	trace_error_with_ids(TRACE_CLASS_PIPE,			\
			     pipe_ptr->ipc_pipe.pipeline_id,	\
			     pipe_ptr->ipc_pipe.comp_id,	\
			     format, ##__VA_ARGS__)

#define tracev_pipe(format, ...) \
	tracev_event(TRACE_CLASS_PIPE, format, ##__VA_ARGS__)
#define tracev_pipe_with_ids(pipe_ptr, format, ...)		\
	tracev_event_with_ids(TRACE_CLASS_PIPE,			\
			     pipe_ptr->ipc_pipe.pipeline_id,	\
			     pipe_ptr->ipc_pipe.comp_id,	\
			     format, ##__VA_ARGS__)

/* Pipeline status to stop execution of current path */
#define PPL_STATUS_PATH_STOP	1

/* pipeline connection directions */
#define PPL_CONN_DIR_COMP_TO_BUFFER	0
#define PPL_CONN_DIR_BUFFER_TO_COMP	1

/* pipeline processing directions */
#define PPL_DIR_DOWNSTREAM	0
#define PPL_DIR_UPSTREAM	1

/*
 * Audio pipeline.
 */
struct pipeline {
	struct sof_ipc_pipe_new ipc_pipe;

	/* runtime status */
	int32_t xrun_bytes;		/* last xrun length */
	uint32_t status;		/* pipeline status */

	/* scheduling */
	struct task *pipe_task;		/* pipeline processing task */

	/* component that drives scheduling in this pipe */
	struct comp_dev *sched_comp;
	/* source component for this pipe */
	struct comp_dev *source_comp;
	/* sink component for this pipe */
	struct comp_dev *sink_comp;

	/* position update */
	uint32_t posn_offset;		/* position update array offset*/
};

/* static pipeline */
extern struct pipeline *pipeline_static;

/* checks if two pipelines have the same scheduling component */
static inline bool pipeline_is_same_sched_comp(struct pipeline *current,
					       struct pipeline *previous)
{
	return current->sched_comp == previous->sched_comp;
}

/* checks if pipeline is scheduled with timer */
static inline bool pipeline_is_timer_driven(struct pipeline *p)
{
	return p->ipc_pipe.time_domain == SOF_TIME_DOMAIN_TIMER;
}

/* checks if pipeline is scheduled on this core */
static inline bool pipeline_is_this_cpu(struct pipeline *p)
{
	return p->ipc_pipe.core == cpu_get_id();
}

/* pipeline creation and destruction */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc,
	struct comp_dev *cd);
int pipeline_free(struct pipeline *p);

/* pipeline buffer creation and destruction */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc);
void buffer_free(struct comp_buffer *buffer);

/* insert component in pipeline */
int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir);

/* complete the pipeline */
int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink);

/* pipeline parameters */
int pipeline_params(struct pipeline *p, struct comp_dev *cd,
		    struct sof_ipc_pcm_params *params);

/* prepare the pipeline for usage */
int pipeline_prepare(struct pipeline *p, struct comp_dev *cd);

/* reset the pipeline and free resources */
int pipeline_reset(struct pipeline *p, struct comp_dev *host_cd);

/* perform selected cache command on pipeline */
void pipeline_cache(struct pipeline *p, struct comp_dev *dev, int cmd);

/* trigger pipeline - atomic */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host_cd, int cmd);

/* static pipeline creation */
int init_static_pipeline(struct ipc *ipc);

/* pipeline creation */
int init_pipeline(void);

/* schedule a copy operation for this pipeline */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start);
void pipeline_schedule_cancel(struct pipeline *p);

/* get time pipeline timestamps from host to dai */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host_dev,
			    struct sof_ipc_stream_posn *posn);

void pipeline_schedule(void *arg);

/* notify host that we have XRUN */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes);

#endif /* __SOF_AUDIO_PIPELINE_H__ */
