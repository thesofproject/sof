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
#include <reef/wait.h>

#define trace_pipe(__e)	trace_event(TRACE_CLASS_PIPE, __e)
#define trace_pipe_error(__e)	trace_error(TRACE_CLASS_PIPE, __e)
#define tracev_pipe(__e)	tracev_event(TRACE_CLASS_PIPE, __e)

/*
 * Audio pipeline.
 */
struct pipeline {
	uint32_t id;		/* id */
	spinlock_t lock;
	completion_t complete;	/* indicate if the pipeline data is finished*/

	/* lists */
	struct list_item host_ep_list;		/* list of host endpoints */
	struct list_item dai_ep_list;		/* list of DAI endpoints */
	struct list_item comp_list;		/* list of components */
	struct list_item buffer_list;		/* list of buffers */
	struct list_item list;			/* list in pipeline list */
};

/* static pipeline ID */
extern struct pipeline *pipeline_static;

/* create new pipeline - returns pipeline id */
struct pipeline *pipeline_new(uint32_t id);
void pipeline_free(struct pipeline *p);

struct pipeline *pipeline_from_id(int id);
struct comp_dev *pipeline_get_comp(struct pipeline *p, uint32_t id);

/* pipeline component creation and destruction */
struct comp_dev *pipeline_comp_new(struct pipeline *p, uint32_t type,
	uint32_t index, uint32_t direction);
int pipeline_comp_free(struct pipeline *p, struct comp_dev *cd);

/* pipeline buffer creation and destruction */
struct comp_buffer *pipeline_buffer_new(struct pipeline *p,
	struct buffer_desc *desc);
int pipeline_buffer_free(struct pipeline *p, struct comp_buffer *buffer);

/* insert component in pipeline */
int pipeline_comp_connect(struct pipeline *p, struct comp_dev *source_cd,
	struct comp_dev *sink_cd, struct comp_buffer *buffer);

/* pipeline parameters */
int pipeline_params(struct pipeline *p, struct comp_dev *cd,
	struct stream_params *params);

/* pipeline parameters */
int pipeline_host_buffer(struct pipeline *p, struct comp_dev *cd,
	struct dma_sg_elem *elem, uint32_t host_size);

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
struct pipeline *init_static_pipeline(void);

/* pipeline creation */
int init_pipeline(void);

void pipeline_schedule_copy(struct pipeline *p, struct comp_dev *dev);

void pipeline_schedule(void *arg);

#endif
