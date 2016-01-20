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
#include <reef/debug.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

struct pipeline_data {
	uint16_t next_id;	/* next ID of new pipeline */
	spinlock_t lock;
	struct list_head pipeline_list;	/* list of all pipelines */
};

/* generic operation data used by op graph walk */
struct op_data {
	int op;
	struct pipeline *p;
	struct stream_params *params;
	int cmd;
	void *cmd_data;
};

static struct pipeline_data *pipe_data;
static int component_op_sink(struct op_data *op_data, struct comp_dev *comp);
static int component_op_source(struct op_data *op_data, struct comp_dev *comp);

/* caller hold locks */
struct pipeline *pipeline_from_id(int id)
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
struct pipeline *pipeline_new(void)
{
	struct pipeline *p;

	trace_pipe('N');

	p = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*p));
	if (p == NULL)
		return NULL;

	spin_lock(&pipe_data->lock);
	p->id = pipe_data->next_id++;
	p->state = PIPELINE_STATE_INIT;
	list_init(&p->comp_list);
	list_init(&p->endpoint_list);
	list_init(&p->buffer_list);

	spinlock_init(&p->lock);
	list_add(&p->list, &pipe_data->pipeline_list);
	spin_unlock(&pipe_data->lock);

	return p;
}

/* pipelines must be inactive */
void pipeline_free(struct pipeline *p)
{
	struct list_head *clist, *t;

	trace_pipe('F');

	spin_lock(&pipe_data->lock);

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
int pipeline_comp_new(struct pipeline *p, struct comp_desc *desc)
{
	struct comp_dev *cd;

	trace_pipe('n');

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

/* insert component in pipeline and allocate buffers */
int pipeline_comp_connect(struct pipeline *p, struct comp_desc *source_desc,
	struct comp_desc *sink_desc, struct buffer_desc *buffer_desc)
{
	struct comp_dev *source, *sink;
	struct comp_buffer *buffer;

	trace_pipe('c');

	source = pipeline_comp_from_id(p, source_desc);
	if (source == NULL)
		return -ENODEV;

	sink = pipeline_comp_from_id(p, sink_desc);
	if (sink == NULL)
		return -ENODEV;

	/* allocate buffer */
	buffer = rmalloc(RZONE_MODULE, RMOD(source->drv->module_id),
		sizeof(*buffer));
	if (buffer == NULL)
		return -ENOMEM;
	buffer->addr = rballoc(RZONE_MODULE, RMOD(source->drv->module_id),
		buffer->desc.size);
	if (buffer->addr == NULL) {
		rfree(RZONE_MODULE, RMOD(source->drv->module_id), buffer);
		return -ENOMEM;
	}
	buffer->w_ptr = buffer->r_ptr = buffer->addr;
	buffer->avail = 0;
	buffer->desc = *buffer_desc;
	list_add(&buffer->pipeline_list, &p->buffer_list);

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

/* call op on all downstream components - locks held by caller */
static int component_op_sink(struct op_data *op_data, struct comp_dev *comp)
{
	struct list_head *clist;
	int err;

	/* do operation on this component */
	switch (op_data->op) {
	case COMP_OPS_PARAMS:
		/* send params to the component */
		err = comp_params(comp, op_data->params);
		break;
	case COMP_OPS_CMD:
		/* send command to the component */
		err = comp_cmd(comp, op_data->cmd, op_data->cmd_data);
		break;
	case COMP_OPS_PREPARE:
		/* prepare the buffers first by clearing contents */
		list_for_each(clist, &comp->bsink_list) {
			struct comp_buffer *buffer;

			buffer = container_of(clist, struct comp_buffer,
				source_list);
			bzero(buffer->addr, buffer->desc.size);
		}

		/* prepare the component */
		err = comp_prepare(comp);
		break;
	case COMP_OPS_COPY:
		/* component should copy to buffers */
		err = comp_copy(comp);
		break;
	case COMP_OPS_BUFFER: /* handled by other API call */
	default:
		return -EINVAL;
	}

	/* dont walk the graph any further if this component fails or
	   doesnt copy any data */
	if (err <= 0)
		return err;

	/* now run this operation downstream */
	list_for_each(clist, &comp->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = component_op_sink(op_data, buffer->sink);
		if (err < 0)
			break;
	}

	return err;
}

/* call op on all upstream components - locks held by caller */
static int component_op_source(struct op_data *op_data, struct comp_dev *comp)
{
	struct list_head *clist;
	int err;

	/* do operation on this component */
	switch (op_data->op) {
	case COMP_OPS_PARAMS:
		/* send params to the component */
		err = comp_params(comp, op_data->params);
		break;
	case COMP_OPS_CMD:
		/* send command to the component */
		err = comp_cmd(comp, op_data->cmd, op_data->cmd_data);
		break;
	case COMP_OPS_PREPARE:
		/* prepare the buffers first */
		list_for_each(clist, &comp->bsink_list) {
			struct comp_buffer *buffer;

			buffer = container_of(clist, struct comp_buffer,
				source_list);
			bzero(buffer->addr, buffer->desc.size);
		}

		/* prepare the component */
		err = comp_prepare(comp);
		break;
	case COMP_OPS_COPY:
		/* component should copy to buffers */
		err = comp_copy(comp);
		break;
	case COMP_OPS_BUFFER: /* handled by other API call */
	default:
		return -EINVAL;
	}

	/* dont walk the graph any further if this component fails or
	   doesnt copy any data */
	if (err <= 0)
		return err;

	/* now run this operation downstream */
	list_for_each(clist, &comp->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = component_op_sink(op_data, buffer->sink);
		if (err < 0)
			break;
	}

	return err;
}

/* prepare the pipeline for usage */
int pipeline_prepare(struct pipeline *p, struct comp_desc *host_desc)
{
	struct comp_dev *host;
	struct op_data op_data;
	int ret;

	trace_pipe('p');

	host = pipeline_comp_from_id(p, host_desc);
	if (host == NULL)
		return -ENODEV;

	op_data.p = p;
	op_data.op = COMP_OPS_PREPARE;

	spin_lock(&p->lock);
	if (host->is_playback)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);
	spin_unlock(&p->lock);
	return ret;
}

/* send pipeline component/endpoint a command */
int pipeline_cmd(struct pipeline *p, struct comp_desc *host_desc, int cmd,
	void *data)
{
	struct comp_dev *host;
	struct op_data op_data;
	int ret;

	trace_pipe('C');

	host = pipeline_comp_from_id(p, host_desc);
	if (host == NULL)
		return -ENODEV;

	op_data.p = p;
	op_data.op = COMP_OPS_CMD;
	op_data.cmd = cmd;
	op_data.cmd_data = data;

	spin_lock(&p->lock);
	if (host->is_playback)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);
	spin_unlock(&p->lock);
	return ret;
}

/* send pipeline component/endpoint params */
int pipeline_params(struct pipeline *p, struct comp_desc *host_desc,
	struct stream_params *params)
{
	struct comp_dev *host;
	struct op_data op_data;
	int ret;

	trace_pipe('P');

	host = pipeline_comp_from_id(p, host_desc);
	if (host == NULL)
		return -ENODEV;

	op_data.p = p;
	op_data.op = COMP_OPS_PARAMS;
	op_data.params = params;

	spin_lock(&p->lock);
	if (host->is_playback)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);
	spin_unlock(&p->lock);
	return ret;
}

/* configure pipelines host DMA buffer */
int pipeline_host_buffer(struct pipeline *p, struct comp_desc *desc,
	struct dma_sg_config *config)
{
	struct comp_dev *comp;

	trace_pipe('B');

	comp = pipeline_comp_from_id(p, desc);
	if (comp == NULL)
		return -ENODEV;

	return comp_host_buffer(comp, config);
}

/* init pipeline */
int pipeline_init(void)
{
	trace_pipe('I');

	pipe_data = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*pipe_data));
	list_init(&pipe_data->pipeline_list);

	/* init components */
	sys_comp_init();
	return 0;
}
