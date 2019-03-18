/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_AUDIO_PIPELINE_H__
#define __INCLUDE_AUDIO_PIPELINE_H__

#include <stdint.h>
#include <stddef.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/dma.h>
#include <sof/audio/component.h>
#include <sof/trace.h>
#include <sof/schedule.h>
#include <uapi/ipc/topology.h>

/*
 * This flag disables firmware-side xrun recovery.
 * It should remain enabled in the situation when the
 * recovery is delegated to the outside of firmware.
 */
#define NO_XRUN_RECOVERY 0

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

struct ipc_pipeline_dev;
struct ipc;

/* Pipeline status to stop execution of current path */
#define PPL_PATH_STOP	1

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
	spinlock_t lock; /* pipeline lock */
	struct sof_ipc_pipe_new ipc_pipe;

	/* runtime status */
	int32_t xrun_bytes;		/* last xrun length */
	uint32_t status;		/* pipeline status */
	bool preload;			/* is pipeline preload needed */

	/* scheduling */
	struct task pipe_task;		/* pipeline processing task */
	struct comp_dev *sched_comp;	/* component that drives scheduling in this pipe */
	struct comp_dev *source_comp;	/* source component for this pipe */
	struct comp_dev *sink_comp;	/* sink component for this pipe */

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

/* checks if pipeline is in preload phase */
static inline bool pipeline_is_preload(struct pipeline *p)
{
	return p->preload;
}

/* checks if pipeline is scheduled with timer */
static inline bool pipeline_is_timer_driven(struct pipeline *p)
{
	return p->ipc_pipe.timer_delay;
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
void pipeline_schedule_copy_idle(struct pipeline *p);
void pipeline_schedule_cancel(struct pipeline *p);

/* get time pipeline timestamps from host to dai */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host_dev,
			    struct sof_ipc_stream_posn *posn);

void pipeline_schedule(void *arg);

/* notify host that we have XRUN */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes);

#endif
