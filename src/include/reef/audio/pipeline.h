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
#include <reef/audio/component.h>

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

/*
 * Pipeline component descriptor.
 */
struct pipe_comp_desc {
	uint32_t uuid;
	int id;
};

/* create new pipeline - returns pipeline id */
int pipeline_new(void);
void pipeline_free(int pipeline_id);

/* insert component in pipeline */
int pipeline_comp_connect(int pipeline_id, struct pipe_comp_desc *source_desc,
	struct pipe_comp_desc *sink_desc);

/* prepare the pipeline for usage */
int pipeline_prepare(int pipeline_id);

/* send pipeline a command */
int pipeline_cmd(int pipeline_id, int cmd);

/* initialise pipeline subsys */
int pipeline_init(void);

/* static pipeline creation */
int init_static_pipeline(void);

/* pipeline creation */
int init_pipeline(void);

#endif
