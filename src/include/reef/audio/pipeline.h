/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_PIPELINE__
#define __INCLUDE_AUDIO_PIPELINE__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dma.h>
#include <reef/audio/component.h>
#include <reef/trace.h>

#define trace_pipe(__e)	trace_event(TRACE_CLASS_PIPE, __e)

/* pipeline states */
#define PIPELINE_STATE_INIT	0	/* pipeline being initialised */
#define PIPELINE_STATE_STOPPED	1	/* pipeline inactive, but ready */
#define PIPELINE_STATE_RUNNING	2	/* pipeline active */
#define PIPELINE_STATE_PAUSED	3	/* pipeline paused */
#define PIPELINE_STATE_DRAINING	4	/* pipeline draining */
#define PIPELINE_STATE_SUSPEND	5	/* pipeline suspended */

/* pipeline commands */
#define PIPELINE_CMD_STOP	0	/* stop pipeline stream */
#define PIPELINE_CMD_START	1	/* start pipeline stream */
#define PIPELINE_CMD_PAUSE	2	/* immediately pause the pipeline stream */
#define PIPELINE_CMD_RELEASE	3	/* release paused pipeline stream */
#define PIPELINE_CMD_DRAIN	4	/* drain pipeline buffers */
#define PIPELINE_CMD_SUSPEND	5	/* suspend pipeline */
#define PIPELINE_CMD_RESUME	6	/* resume pipeline */

/*
 * Audio pipeline.
 */
struct pipeline {
	uint16_t id;		/* id */
	uint16_t state;		/* PIPELINE_STATE_ */ 
	spinlock_t lock;

	/* lists */
	struct list_head endpoint_list;		/* list of endpoints */
	struct list_head comp_list;		/* list of components */
	struct list_head buffer_list;		/* list of buffers */
	struct list_head list;			/* list in pipeline list */
};

/* static pipeline ID */
extern struct pipeline *pipeline_static;

/* create new pipeline - returns pipeline id */
struct pipeline *pipeline_new(void);
void pipeline_free(struct pipeline *p);

struct pipeline *pipeline_from_id(int id);
struct comp_dev *pipeline_get_comp(struct pipeline *p,
	struct comp_desc *desc);

int pipeline_comp_new(struct pipeline *p, struct comp_desc *desc);

/* insert component in pipeline */
int pipeline_comp_connect(struct pipeline *p, struct comp_desc *source_desc,
	struct comp_desc *sink_desc, struct buffer_desc *buffer_desc);

/* pipeline parameters */
int pipeline_params(struct pipeline *p, struct comp_desc *desc,
	struct stream_params *params);

/* pipeline parameters */
int pipeline_host_buffer(struct pipeline *p, struct comp_desc *desc,
	struct stream_params *params, struct dma_sg_elem *elem);

/* prepare the pipeline for usage */
int pipeline_prepare(struct pipeline *p, struct comp_desc *host_desc,
	struct stream_params *params);

/* reset the pipeline and free resources */
int pipeline_reset(struct pipeline *p, struct comp_desc *host_desc,
	struct stream_params *params);

/* send pipeline a command */
int pipeline_cmd(struct pipeline *p, struct comp_desc *host_desc,
	struct stream_params *params, int cmd, void *data);

/* initialise pipeline subsys */
int pipeline_init(void);

/* static pipeline creation */
struct pipeline *init_static_pipeline(void);

/* pipeline creation */
int init_pipeline(void);

/* pipeline work */
void pipeline_do_work(struct pipeline *p);

#endif
