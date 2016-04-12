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
#include <platform/timer.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

#define PIPELINE_COPY_IDLE	0
#define PIPELINE_COPY_SCHEDULED	1
#define PIPELINE_COPY_RUNNING	2

struct pipeline_data {
	spinlock_t lock;
	uint32_t next_id;	/* monotonic ID counter */
	struct list_head pipeline_list;	/* list of all pipelines */
	uint32_t copy_status;	/* PIPELINE_COPY_ */
};

/* generic operation data used by op graph walk */
struct op_data {
	int op;
	struct pipeline *p;
	struct stream_params *params;
	int cmd;
	void *cmd_data;
	uint32_t copy_size;
};

static struct pipeline_data *pipe_data;
static int component_op_sink(struct op_data *op_data, struct comp_dev *comp);
//static int component_op_source(struct op_data *op_data, struct comp_dev *comp);

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
	uint32_t id)
{
	struct comp_dev *cd;
	struct list_head *clist;

	/* search for pipeline by id */
	list_for_each(clist, &p->comp_list) {

		cd = container_of(clist, struct comp_dev, pipeline_list);

		if (cd->id == id)
			return cd;
	}

	/* not found */
	return NULL;
}

struct comp_dev *pipeline_get_comp(struct pipeline *p, uint32_t id)
{
	struct comp_dev *cd;

	spin_lock(&p->lock);
	cd = pipeline_comp_from_id(p, id);
	spin_unlock(&p->lock);
	return cd;
}	

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(uint16_t id)
{
	struct pipeline *p;

	trace_pipe("PNw");

	p = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*p));
	if (p == NULL)
		return NULL;

	spin_lock(&pipe_data->lock);
	p->id = id;
	//p->state = PIPELINE_STATE_INIT;
	list_init(&p->comp_list);
	list_init(&p->host_ep_list);
	list_init(&p->dai_ep_list);
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

	trace_pipe("PFr");

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
struct comp_dev *pipeline_comp_new(struct pipeline *p, uint32_t type,
	uint32_t index, uint8_t direction)
{
	struct comp_dev *cd;

	trace_pipe("CNw");

	cd = comp_new(p, type, index, pipe_data->next_id++, direction);
	if (cd == NULL) {
		pipe_data->next_id--;
		return NULL;
	}

	spin_lock(&p->lock);

	switch (type) {
	case COMP_TYPE_DAI_SSP:
	case COMP_TYPE_DAI_HDA:
		/* add to DAI endpoint list and to list*/
		list_add(&cd->endpoint_list, &p->dai_ep_list);
		break;
	case COMP_TYPE_HOST:
		/* add to DAI endpoint list and to list*/
		list_add(&cd->endpoint_list, &p->host_ep_list);
		break;
	default:
		break;
	}

	/* add component dev to pipeline list */ 
	list_add(&cd->pipeline_list, &p->comp_list);

	spin_unlock(&pipe_data->lock);
	return cd;
}

/* free component in the pipeline */
int pipeline_comp_free(struct pipeline *p, struct comp_dev *cd)
{
	trace_pipe("CFr");
	// TODO
	return 0;
}

/* create a new component in the pipeline */
struct comp_buffer *pipeline_buffer_new(struct pipeline *p,
	struct buffer_desc *desc)
{
	struct comp_buffer *buffer;

	trace_pipe("BNw");

	spin_lock(&p->lock);

	/* allocate buffer */
	buffer = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*buffer));
	if (buffer == NULL)
		return NULL;

	buffer->addr = rballoc(RZONE_MODULE, RMOD_SYS, desc->size);
	if (buffer->addr == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, buffer);
		return NULL;
	}

	buffer->w_ptr = buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + desc->size;
	buffer->free = desc->size;
	buffer->avail = 0;
	buffer->desc = *desc;
	buffer->id = pipe_data->next_id++;
	list_add(&buffer->pipeline_list, &p->buffer_list);

	spin_unlock(&pipe_data->lock);
	return buffer;
}

/* free component in the pipeline */
int pipeline_buffer_free(struct pipeline *p, struct comp_buffer *buffer)
{
	trace_pipe("BFr");
	// TODO
	return 0;
}

/* insert component in pipeline and allocate buffers */
int pipeline_comp_connect(struct pipeline *p, struct comp_dev *source,
	struct comp_dev *sink, struct comp_buffer *buffer)
{
	trace_pipe("CCn");

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

	trace_pipe("CO-");
	trace_value(comp->id);

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
	case COMP_OPS_RESET:
		/* component should reset and free resources */
		err = comp_reset(comp);
		break;
	case COMP_OPS_BUFFER: /* handled by other API call */
	default:
		return -EINVAL;
	}

	/* dont walk the graph any further if this component fails */
	if (err < 0) {
		trace_pipe_error("eOp");
		return err;
	}

	if (comp->is_dai)
		return 0;

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

	trace_pipe("CO+");
	trace_value(comp->id);

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
	case COMP_OPS_RESET:
		/* component should reset and free resources */
		err = comp_reset(comp);
		break;
	case COMP_OPS_BUFFER: /* handled by other API call */
	default:
		return -EINVAL;
	}

	/* dont walk the graph any further if this component fails */
	if (err < 0) {
		trace_pipe_error("eOp");
		return err;
	}

	if (comp->is_dai)
		return 0;

	/* now run this operation upstream */
	list_for_each(clist, &comp->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = component_op_source(op_data, buffer->source);
		if (err < 0)
			break;
	}

	return err;
}


/* prepare the pipeline for usage - preload host buffers here */
int pipeline_prepare(struct pipeline *p, struct comp_dev *host)
{
	struct op_data op_data;
	int ret;

	trace_pipe("Ppr");

	op_data.p = p;
	op_data.op = COMP_OPS_PREPARE;

	spin_lock(&p->lock);
	if (host->direction == STREAM_DIRECTION_PLAYBACK)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);
	spin_unlock(&p->lock);

	return ret;
}

/* send pipeline component/endpoint a command */
int pipeline_cmd(struct pipeline *p, struct comp_dev *host, int cmd,
	void *data)
{
	struct op_data op_data;
	int ret;

	trace_pipe("PCm");

	op_data.p = p;
	op_data.op = COMP_OPS_CMD;
	op_data.cmd = cmd;
	op_data.cmd_data = data;

	spin_lock(&p->lock);
	if (host->direction == STREAM_DIRECTION_PLAYBACK)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);

	spin_unlock(&p->lock);
	return ret;
}

/* send pipeline component/endpoint params */
int pipeline_params(struct pipeline *p, struct comp_dev *host,
	struct stream_params *params)
{
	struct op_data op_data;
	int ret;

	trace_pipe("Par");

	op_data.p = p;
	op_data.op = COMP_OPS_PARAMS;
	op_data.params = params;

	spin_lock(&p->lock);
	if (host->direction == STREAM_DIRECTION_PLAYBACK)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);
	spin_unlock(&p->lock);

	return ret;
}

/* send pipeline component/endpoint params */
int pipeline_reset(struct pipeline *p, struct comp_dev *host)
{
	struct op_data op_data;
	int ret;

	trace_pipe("PRe");

	op_data.p = p;
	op_data.op = COMP_OPS_RESET;

	spin_lock(&p->lock);
	if (host->direction == STREAM_DIRECTION_PLAYBACK)
		ret = component_op_sink(&op_data, host);
	else
		ret = component_op_source(&op_data, host);
	spin_unlock(&p->lock);
	return ret;
}

/* TODO: remove ?? configure pipelines host DMA buffer */
int pipeline_host_buffer(struct pipeline *p, struct comp_dev *host,
	struct dma_sg_elem *elem)
{
	trace_pipe("PBr");

	return comp_host_buffer(host, elem);
}

/* copy audio data from DAI buffer to host PCM buffer via pipeline */
static int pipeline_copy_playback(struct comp_dev *comp)
{
	struct list_head *clist;
	int err;

	/* component should copy to buffers */
	err = comp_copy(comp);

	/* dont walk the graph any further if this component fails or
	   doesnt copy any data */
	if (err < 0) {
		trace_pipe_error("ePc");
		return err;
	}

	if (comp->is_host)
		return 0;

	/* now copy upstream */
	list_for_each(clist, &comp->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = pipeline_copy_playback(buffer->source);
		if (err < 0)
			break;
	}

	return err;
}

/* copy audio data from DAI buffer to host PCM buffer via pipeline */
static int pipeline_copy_capture(struct comp_dev *comp)
{
	struct list_head *clist;
	int err;

	/* component should copy to buffers */
	err = comp_copy(comp);

	/* dont walk the graph any further if this component fails or
	   doesnt copy any data */
	if (err < 0) {
		trace_pipe_error("ePc");
		return err;
	}

	if (comp->is_host)
		return 0;

	/* now copy upstream */
	list_for_each(clist, &comp->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = pipeline_copy_capture(buffer->sink);
		if (err < 0)
			break;
	}

	return err;
}

/* called on timer tick to process pipeline data */
void pipeline_do_work(struct pipeline *p, uint32_t udelay)
{
	struct list_head *elist;

	if (pipe_data->copy_status != PIPELINE_COPY_SCHEDULED)
		return;

	pipe_data->copy_status = PIPELINE_COPY_RUNNING;

	tracev_pipe("PWs");

	if (list_empty(&p->dai_ep_list))
		return;

	/* process capture streams in the pipeline */
	list_for_each(elist, &p->dai_ep_list) {
		struct comp_dev *ep;

		ep = container_of(elist, struct comp_dev, endpoint_list);

		if (ep->state != COMP_STATE_RUNNING)
			continue;

		if (ep->direction == STREAM_DIRECTION_PLAYBACK)
			continue;

		/* process downstream */
		pipeline_copy_capture(ep);
	}

	/* now process playback streams in the pipeline */
	list_for_each(elist, &p->dai_ep_list) {
		struct comp_dev *ep;

		ep = container_of(elist, struct comp_dev, endpoint_list);

		if (ep->state != COMP_STATE_RUNNING)
			continue;

		if (ep->direction != STREAM_DIRECTION_PLAYBACK)
			continue;

		/* process downstream */
		pipeline_copy_playback(ep);
	}

	tracev_pipe("PWe");

	pipe_data->copy_status = PIPELINE_COPY_IDLE;
}

/* notify pipeline that this buffer needs filled */
void pipeline_fill_buffer(struct pipeline *p, struct comp_buffer *buffer)
{
	uint32_t flags;

	spin_lock_irq(pipe_data->lock, flags);

	/* is buffer work already running or scheduled ? */
	if (pipe_data->copy_status == PIPELINE_COPY_RUNNING ||
		pipe_data->copy_status == PIPELINE_COPY_SCHEDULED)
		goto out;

	/* send software interrupt to pipeline */
	pipe_data->copy_status = PIPELINE_COPY_SCHEDULED;
out:
	spin_unlock_irq(pipe_data->lock, flags);
}

/* notify pipeline that this buffer emptied */
void pipeline_empty_buffer(struct pipeline *p, struct comp_buffer *buffer)
{
	uint32_t flags;

	spin_lock_irq(pipe_data->lock, flags);

	/* is buffer work already running or scheduled ? */
	if (pipe_data->copy_status == PIPELINE_COPY_RUNNING ||
		pipe_data->copy_status == PIPELINE_COPY_SCHEDULED)
		goto out;

	/* send software interrupt to pipeline */
	pipe_data->copy_status = PIPELINE_COPY_SCHEDULED;
out:
	spin_unlock_irq(pipe_data->lock, flags);
}

/* init pipeline */
int pipeline_init(void)
{
	trace_pipe("PIn");

	pipe_data = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*pipe_data));
	list_init(&pipe_data->pipeline_list);
	pipe_data->next_id = 0;
	spinlock_init(&pipe_data->lock);

	return 0;
}
