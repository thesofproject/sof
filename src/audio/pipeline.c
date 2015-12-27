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

/* caller hold locks */
static struct comp_dev *pipeline_comp_from_id(struct pipeline *p,
	struct comp_desc *desc)
{
	struct comp_dev *cd;
	struct list_head *clist;

	/* search for pipeline by id */
	list_for_each(clist, &p->comp_list) {

		cd = container_of(clist, struct comp_dev, pipeline_list);
		if (cd->drv->uuid == desc->uuid && cd->id == desc->id)
			return cd;
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
	list_init(&p->comp_list);
	list_init(&p->endpoint_list);
	list_init(&p->buffer_list);

	spinlock_init(&p->lock);
	list_add(&p->list, &pipe_data->pipeline_list);
	spin_unlock(&pipe_data->lock);

	return p->id;
}

/* pipelines must be inactive */
void pipeline_free(int pipeline_id)
{
	struct pipeline *p;
	struct list_head *clist, *t;

	spin_lock(&pipe_data->lock);

	p = pipeline_from_id(pipeline_id);
	if (p == NULL) {
		spin_unlock(&pipe_data->lock);
		return;
	}

	/* free all components */
	list_for_each_safe(clist, t, &p->comp_list) {
		struct comp_dev *comp;

		comp = container_of(clist, struct comp_dev, pipeline_list);
		comp_free(comp);
	}

	/* free all buffers */
	list_for_each_safe(clist, t, &p->buffer_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, pipeline_list);
		rfree(RZONE_MODULE, RMOD(buffer->source->drv->module_id),
			buffer->addr);
		rfree(RZONE_MODULE, RMOD(buffer->source->drv->module_id),
			buffer);
	}

	/* now free the pipeline */
	list_del(&p->list);
	rfree(RZONE_MODULE, RMOD_SYS, p);
	spin_unlock(&pipe_data->lock);
}

/* create a new component in the pipeline */
int pipeline_comp_new(int pipeline_id, struct comp_desc *desc)
{
	static struct pipeline *p;
	struct comp_dev *cd;

	p = pipeline_from_id(pipeline_id);
	if (p == NULL)
		return -EINVAL;

	cd = comp_new(desc);
	if (cd == NULL)
		return -ENODEV;

	spinlock_init(&cd->lock);
	list_init(&cd->bsource_list);
	list_init(&cd->bsink_list);

	spin_lock(&p->lock);

	switch (COMP_TYPE(desc->uuid)) {
	case COMP_TYPE_DAI_SSP:
	case COMP_TYPE_DAI_HDA:
		/* add to endpoint list and to comp list*/
		list_add(&cd->endpoint_list, &p->endpoint_list);
	default:
		/* add component dev to pipeline list */ 
		list_add(&cd->pipeline_list, &p->comp_list);
	}

	spin_unlock(&pipe_data->lock);
	return 0;
}

/* insert component in pipeline */
int pipeline_comp_connect(int pipeline_id, struct comp_desc *source_desc,
	struct comp_desc *sink_desc)
{
	static struct pipeline *p;
	struct comp_dev *source, *sink;
	struct comp_buffer *buffer;

	p = pipeline_from_id(pipeline_id);
	if (p == NULL)
		return -EINVAL;

	source = pipeline_comp_from_id(p, source_desc);
	if (source == NULL)
		return -ENODEV;

	sink = pipeline_comp_from_id(p, sink_desc);
	if (sink == NULL)
		return -ENODEV;

	/* buffer space allocated later */
	buffer = rmalloc(RZONE_MODULE, RMOD(source->drv->module_id),
		sizeof(*buffer));
	if (buffer == NULL)
		return -ENOMEM;

	/* connect source to buffer */
	spin_lock(&source->lock);
	list_add(&buffer->source_list, &source->bsink_list);
	buffer->source = source;
	spin_unlock(&source->lock);

	/* connect sink to buffer */
	spin_lock(&sink->lock);
	list_add(&buffer->sink_list, &sink->bsource_list);
	buffer->sink = sink;
	spin_unlock(&sink->lock);

	return 0;
}

/* prepare the pipeline for usage */
int pipeline_prepare(int pipeline_id, struct comp_desc *host_desc)
{
	/* buffer allocated here as graph is walked */
	return 0;
}

/* send pipeline component/endpoint a command */
int pipeline_cmd(int pipeline_id, struct comp_desc *host_desc, int cmd)
{
	/* walk graph and send cmd to all components from host source */
	return 0;
}

/* send pipeline component/endpoint params */
int pipeline_params(int pipeline_id, struct comp_desc *host_desc,
	struct stream_params *params)
{
	return 0;
}

/* configure pipelines host DMA buffer */
int pipeline_host_buffer(int pipeline_id, struct comp_desc *desc,
	struct dma_sg_config *config)
{

	return 0;
}

/* init pipeline */
int pipeline_init(void)
{
	pipe_data = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*pipe_data));
	list_init(&pipe_data->pipeline_list);

	/* init components */
	sys_comp_init();
	return 0;
}
