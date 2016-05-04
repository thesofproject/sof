/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
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

#define trace_pipe(__e)	trace_event(TRACE_CLASS_PIPE, __e)
#define trace_pipe_error(__e)	trace_error(TRACE_CLASS_PIPE, __e)
#define tracev_pipe(__e)	tracev_event(TRACE_CLASS_PIPE, __e)

/*
 * Audio pipeline.
 */
struct pipeline {
	uint16_t id;		/* id */
	uint16_t clock;		/* clock use as pipeline time base */
	spinlock_t lock;

	/* work */
	uint32_t work_freq;	/* how often pipeline work is called */

	/* lists */
	struct list_head host_ep_list;		/* list of host endpoints */
	struct list_head dai_ep_list;		/* list of DAI endpoints */
	struct list_head comp_list;		/* list of components */
	struct list_head buffer_list;		/* list of buffers */
	struct list_head list;			/* list in pipeline list */
};

/* static pipeline ID */
extern struct pipeline *pipeline_static;

/* create new pipeline - returns pipeline id */
struct pipeline *pipeline_new(uint16_t id);
void pipeline_free(struct pipeline *p);

struct pipeline *pipeline_from_id(int id);
struct comp_dev *pipeline_get_comp(struct pipeline *p, uint32_t id);

/* pipeline component creation and destruction */
struct comp_dev *pipeline_comp_new(struct pipeline *p, uint32_t type,
	uint32_t index, uint8_t direction);
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
	struct dma_sg_elem *elem);

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

static inline void pipeline_set_work_freq(struct pipeline *p,
	uint32_t work_freq, uint16_t clock)
{
	p->work_freq = work_freq;
	p->clock = clock;
}
#endif
