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
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dma.h>
#include <reef/audio/component.h>
#include <reef/trace.h>
#include <reef/schedule.h>
#include <uapi/ipc.h>

/* pipeline tracing */
#define trace_pipe(__e)	trace_event(TRACE_CLASS_PIPE, __e)
#define trace_pipe_error(__e)	trace_error(TRACE_CLASS_PIPE, __e)
#define tracev_pipe(__e)	tracev_event(TRACE_CLASS_PIPE, __e)

struct ipc_pipeline_dev;
struct ipc;

/*
 * Audio pipeline.
 */
struct pipeline {
	spinlock_t lock;
	struct sof_ipc_pipe_new ipc_pipe;

	/* runtime status */
	int32_t xrun_bytes;		/* last xrun length */
	uint32_t status;		/* pipeline status */

	/* lists */
	struct list_item comp_list;		/* list of components */
	struct list_item buffer_list;		/* list of buffers */

	/* scheduling */
	struct task pipe_task;		/* pipeline processing task */
	struct comp_dev *sched_comp;	/* component that drives scheduling in this pipe */
	struct comp_dev *source_comp;	/* source component for this pipe */
};

/* static pipeline */
extern struct pipeline *pipeline_static;

/* pipeline creation and destruction */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc,
	struct comp_dev *cd);
int pipeline_free(struct pipeline *p);

/* pipeline buffer creation and destruction */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc);
void buffer_free(struct comp_buffer *buffer);

/* insert component in pipeline */
int pipeline_comp_connect(struct pipeline *p, struct comp_dev *source_comp,
	struct comp_buffer *sink_buffer);
int pipeline_buffer_connect(struct pipeline *p,
	struct comp_buffer *source_buffer, struct comp_dev *sink_comp);
int pipeline_complete(struct pipeline *p);

/* pipeline parameters */
int pipeline_params(struct pipeline *p, struct comp_dev *cd,
	struct sof_ipc_pcm_params *params);

/* prepare the pipeline for usage */
int pipeline_prepare(struct pipeline *p, struct comp_dev *cd);

/* reset the pipeline and free resources */
int pipeline_reset(struct pipeline *p, struct comp_dev *host_cd);

/* send pipeline a command */
int pipeline_cmd(struct pipeline *p, struct comp_dev *host_cd, int cmd,
	void *data);

/* initialise pipeline subsys */
int pipeline_init(void);

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
