/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

struct pipeline_data {
	uint16_t next_id;	/* next ID of new pipeline */
	spinlock_t lock;
	struct list_head pipeline_list;	/* list of all pipelines */
};

static struct pipeline_data *pipe_data;

/* caller hold locks */
static struct pipeline *pipeline_from_id(int id)
{
	struct pipeline *p;
	struct list_head *plist;

	/* search for pipeline by id */
	list_for_each(plist, &pipe_data->pipeline_list) {

		p = container_of(plist, struct pipeline, list);
		if (p->id == id)
			return p;
	}

	/* not found */
	return NULL;
}

/* create new pipeline - returns pipeline id or negative error */
int pipeline_new(void)
{
	struct pipeline *p;

	p = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*p));
	if (p == NULL)
		return -ENOMEM;

	spin_lock(&pipe_data->lock);
	p->id = pipe_data->next_id++;
	p->state = PIPELINE_STATE_INIT;
	list_add(&p->list, &pipe_data->pipeline_list);
	spin_unlock(&pipe_data->lock);

	return p->id;
}

/* pipelines must be inactive */
void pipeline_free(int pipeline_id)
{
	struct pipeline *p;

	spin_lock(&pipe_data->lock);

	p = pipeline_from_id(pipeline_id);
	if (p == NULL) {
		spin_unlock(&pipe_data->lock);
		return;
	}

	/* free all components */


	/* now free the pipeline */
	list_del(&p->list);
	rfree(RZONE_MODULE, RMOD_SYS, p);
	spin_unlock(&pipe_data->lock);
}

/* create a new component in the pipeline */
int pipeline_comp_new(int pipeline_id, int comp_uuid, int comp_id)
{

	return 0;
}

/* insert component in pipeline */
int pipeline_comp_connect(int pipeline_id, struct pipe_comp_desc *source_desc,
	struct pipe_comp_desc *sink_desc)
{

	return 0;
}

/* prepare the pipeline for usage */
int pipeline_prepare(int pipeline_id)
{

	return 0;
}

/* send pipeline a command */
int pipeline_cmd(int pipeline_id, int cmd)
{

	return 0;
}

/* init pipeline */
int pipeline_init(void)
{
	pipe_data = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*pipe_data));
	list_init(&pipe_data->pipeline_list);
	return 0;
}
