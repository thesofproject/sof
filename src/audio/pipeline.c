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
 *         Keyon Jie <yang.jie@linux.intel.com>
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

#define PIPELINE_DEPTH_LIMIT	5
#define PIPELINE_MAX_COUNT	20

struct pipeline_data {
	spinlock_t lock;
	uint32_t next_id;	/* monotonic ID counter */
	struct list_item pipeline_list;	/* list of all pipelines */
	uint32_t copy_status;	/* PIPELINE_COPY_ */
	struct list_item schedule_list;	/* list of components to be copied */
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
	struct list_item *plist;

	/* search for pipeline by id */
	list_for_item(plist, &pipe_data->pipeline_list) {

		p = container_of(plist, struct pipeline, list);
		if (p->id == id)
			return p;
	}

	/* not found */
	trace_pipe_error("ePp");
	return NULL;
}

/* caller hold locks */
static struct comp_dev *pipeline_comp_from_id(struct pipeline *p,
	uint32_t id)
{
	struct comp_dev *cd;
	struct list_item *clist;

	/* search for pipeline by id */
	list_for_item(clist, &p->comp_list) {

		cd = container_of(clist, struct comp_dev, pipeline_list);

		if (cd->id == id)
			return cd;
	}

	/* not found */
	trace_pipe_error("ePc");
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
struct pipeline *pipeline_new(uint32_t id)
{
	struct pipeline *p;

	trace_pipe("PNw");

	p = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*p));
	if (p == NULL) {
		trace_pipe_error("ePN");
		return NULL;
	}

	spin_lock(&pipe_data->lock);
	p->id = id;
	//p->state = PIPELINE_STATE_INIT;
	list_init(&p->comp_list);
	list_init(&p->host_ep_list);
	list_init(&p->dai_ep_list);
	list_init(&p->buffer_list);
	wait_init(&p->complete);

	spinlock_init(&p->lock);
	list_item_prepend(&p->list, &pipe_data->pipeline_list);
	spin_unlock(&pipe_data->lock);

	return p;
}

/* pipelines must be inactive */
void pipeline_free(struct pipeline *p)
{
	struct list_item *clist, *t;

	trace_pipe("PFr");

	spin_lock(&pipe_data->lock);

	/* free all components */
	list_for_item_safe(clist, t, &p->comp_list) {
		struct comp_dev *comp;

		comp = container_of(clist, struct comp_dev, pipeline_list);
		comp_free(comp);
	}

	/* free all buffers */
	list_for_item_safe(clist, t, &p->buffer_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, pipeline_list);
		rfree(buffer->addr);
		rfree(buffer);
	}

	/* now free the pipeline */
	list_item_del(&p->list);
	rfree(p);
	spin_unlock(&pipe_data->lock);
}

/* create a new component in the pipeline */
struct comp_dev *pipeline_comp_new(struct pipeline *p, uint32_t type,
	uint32_t index, uint32_t direction)
{
	struct comp_dev *cd;

	trace_pipe("CNw");

	cd = comp_new(p, type, index, pipe_data->next_id++, direction);
	if (cd == NULL) {
		pipe_data->next_id--;
		trace_pipe_error("eNw");
		return NULL;
	}

	spin_lock(&p->lock);

	switch (type) {
	case COMP_TYPE_DAI_SSP:
	case COMP_TYPE_DAI_HDA:
		/* add to DAI endpoint list and to list*/
		list_item_prepend(&cd->endpoint_list, &p->dai_ep_list);
		break;
	case COMP_TYPE_HOST:
		/* add to DAI endpoint list and to list*/
		list_item_prepend(&cd->endpoint_list, &p->host_ep_list);
		break;
	default:
		break;
	}

	/* add component dev to pipeline list */
	list_item_prepend(&cd->pipeline_list, &p->comp_list);
	spin_unlock(&p->lock);

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

	/* allocate buffer */
	buffer = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*buffer));
	if (buffer == NULL) {
		trace_pipe_error("eBm");
		return NULL;
	}

	buffer->addr = rballoc(RZONE_RUNTIME, RFLAGS_NONE, desc->size);
	if (buffer->addr == NULL) {
		rfree(buffer);
		trace_pipe_error("ebm");
		return NULL;
	}
	bzero(buffer->addr, desc->size);

	buffer->w_ptr = buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + desc->size;
	buffer->free = desc->size;
	buffer->avail = 0;
	buffer->desc = *desc;
	buffer->connected = 0;

	spin_lock(&p->lock);
	buffer->id = pipe_data->next_id++;
	list_item_prepend(&buffer->pipeline_list, &p->buffer_list);
	spin_unlock(&p->lock);
	return buffer;
}

/* free component in the pipeline */
int pipeline_buffer_free(struct pipeline *p, struct comp_buffer *buffer)
{
	trace_pipe("BFr");

	spin_lock(&p->lock);
	list_item_del(&buffer->pipeline_list);
	rfree(buffer->addr);
	rfree(buffer);
	spin_unlock(&p->lock);
	return 0;
}

/* insert component in pipeline and allocate buffers */
int pipeline_comp_connect(struct pipeline *p, struct comp_dev *source,
	struct comp_dev *sink, struct comp_buffer *buffer)
{
	trace_pipe("CCn");

	/* connect source to buffer */
	spin_lock(&source->lock);
	list_item_prepend(&buffer->source_list, &source->bsink_list);
	buffer->source = source;
	spin_unlock(&source->lock);

	/* connect sink to buffer */
	spin_lock(&sink->lock);
	list_item_prepend(&buffer->sink_list, &sink->bsource_list);
	buffer->sink = sink;
	spin_unlock(&sink->lock);

	/* connect the components */
	spin_lock(&p->lock);
	buffer->connected = 1;
	spin_unlock(&p->lock);

	return 0;
}

/* preload component buffers prior to playback start */
static int component_preload(struct comp_dev *comp, uint32_t depth,
		uint32_t max_depth)
{
	struct list_item *clist;
	int err;

	/* bail if we are at max_depth */
	if (depth > max_depth)
		return 0;

	trace_value(comp->id);
	err = comp_preload(comp);

	/* dont walk the graph any further if this component fails */
	if (err < 0) {
		trace_pipe_error("eOp");
		return err;
	} else if (err > 0 || comp->is_dai) {
		/* we finish walking the graph if we reach the DAI or component is
		 * currently active and configured already (err > 0).
		 */
		trace_pipe("prl");
		trace_value((uint32_t)err);
		trace_value(comp->is_dai);
		trace_value(comp->id);
		trace_value(comp->is_host);
		return 0;
	}

	/* now run this operation downstream */
	list_for_item(clist, &comp->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = component_preload(buffer->sink, depth + 1, max_depth);
		if (err < 0)
			break;
	}

	return err;
}

/* work out the pipeline depth from this component to the farthest endpoint */
static int pipeline_depth(struct comp_dev *comp, int current)
{
	struct list_item *clist;
	int depth = 0;

	/* go down stream to deepest endpoint */
	list_for_item(clist, &comp->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		depth = pipeline_depth(buffer->sink, current + 1);
		if (depth > current)
			current = depth;
	}

	return current;
}

/* call op on all downstream components - locks held by caller */
static int component_op_sink(struct op_data *op_data, struct comp_dev *comp)
{
	struct list_item *clist;
	int err = 0;

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
		if (comp->is_mixer && (comp->state >= COMP_STATE_PREPARE)) {
			/* don't need go downstream, finished */
			trace_pipe("C-M");
			return 0;
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
		trace_pipe_error("eOi");
		trace_value(op_data->op);
		return -EINVAL;
	}

	/* dont walk the graph any further if this component fails */
	if (err < 0) {
		trace_pipe_error("eOp");
		return err;
	}

	if (comp->is_dai) {
		trace_pipe("C-D");
		return 0;
	}

	if (comp->is_mixer && (err > 0)) {
		/* don't need go downstream, finished */
		trace_pipe("C-M");
		return 0;
	}

	/* now run this operation downstream */
	list_for_item(clist, &comp->bsink_list) {
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
	struct list_item *clist;
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
		list_for_item(clist, &comp->bsource_list) {
			struct comp_buffer *buffer;

			buffer = container_of(clist, struct comp_buffer,
				sink_list);
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
		trace_pipe_error("eOi");
		trace_value(op_data->op);
		return -EINVAL;
	}

	/* dont walk the graph any further if this component fails */
	if (err < 0) {
		trace_pipe_error("eOp");
		return err;
	}

	if (comp->is_dai) {
		trace_pipe("C+D");
		return 0;
	}

	/* now run this operation upstream */
	list_for_item(clist, &comp->bsource_list) {
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
	int ret, depth, i;

	trace_pipe("Ppr");

	op_data.p = p;
	op_data.op = COMP_OPS_PREPARE;

	spin_lock(&p->lock);
	if (host->direction == STREAM_DIRECTION_PLAYBACK) {

		/* first of all prepare the pipeline */
		ret = component_op_sink(&op_data, host);
		if (ret < 0)
			goto out;

		/* then preload buffers - the buffers must be moved
		 * downstream so that every component has full buffers for
		 * trigger start */
		depth = pipeline_depth(host, 0);
		for (i = depth; i > 0; i--) {
			ret = component_preload(host, 0, i);
			if (ret < 0)
				break;
		}
	} else {
		ret = component_op_source(&op_data, host);
	}

out:
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
	struct dma_sg_elem *elem, uint32_t host_size)
{
	trace_pipe("PBr");

	return comp_host_buffer(host, elem, host_size);
}

/* copy audio data from DAI buffer to host PCM buffer via pipeline */
static int pipeline_copy_playback(struct comp_dev *comp, uint32_t depth)
{
	struct list_item *clist;
	int err;

	/* protect the stack from pipeline that are too large or loop */
	if (depth++ >= PIPELINE_DEPTH_LIMIT) {
		trace_pipe_error("ePD");
		return -EINVAL;
	}
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
	list_for_item(clist, &comp->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected || buffer->source->state != COMP_STATE_RUNNING)
			continue;

		err = pipeline_copy_playback(buffer->source, depth);
		if (err < 0)
			break;
	}

	return err;
}

/* copy audio data from DAI buffer to host PCM buffer via pipeline */
static int pipeline_copy_capture(struct comp_dev *comp, uint32_t depth)
{
	struct list_item *clist;
	int err;

	/* protect the stack from pipeline that are too large or loop */
	if (depth++ >= PIPELINE_DEPTH_LIMIT) {
		trace_pipe_error("ePD");
		return -EINVAL;
	}

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
	list_for_item(clist, &comp->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = pipeline_copy_capture(buffer->sink, depth);
		if (err < 0)
			break;
	}

	return err;
}

//TODO check locks for IRQ status prior to locking
/* notify pipeline that this component requires buffers emptied/filled */
void pipeline_schedule_copy(struct pipeline *p, struct comp_dev *dev)
{
	struct comp_dev *cd;
	struct list_item *clist;
	uint32_t flags;

	/* add to list of scheduled components */
	spin_lock_irq(&pipe_data->lock, flags);

	/* check to see if we are already scheduled ? */
	list_for_item(clist, &pipe_data->schedule_list) {
		cd = container_of(clist, struct comp_dev, schedule_list);

		/* keep original */
		if (cd == dev)
			goto out;
	}

	list_item_append(&dev->schedule_list, &pipe_data->schedule_list);

out:
	spin_unlock_irq(&pipe_data->lock, flags);

	/* now schedule the copy */
	interrupt_set(PLATFORM_PIPELINE_IRQ);
}

void pipeline_schedule(void *arg)
{
	struct comp_dev *dev;
	uint32_t finished = 0, count = 0, flags;

	interrupt_clear(PLATFORM_PIPELINE_IRQ);

	tracev_pipe("PWs");
	pipe_data->copy_status = PIPELINE_COPY_RUNNING;

	/* dont loop forever, but have a max number of components to copy */
	while (count < PIPELINE_MAX_COUNT) {

		/* get next component scheduled or finish */
		spin_lock_irq(&pipe_data->lock, flags);

		if (list_is_empty(&pipe_data->schedule_list))
			finished = 1;
		else {
			dev = list_first_item(&pipe_data->schedule_list,
				struct comp_dev, schedule_list);
			list_item_del(&dev->schedule_list);
		}

		spin_unlock_irq(&pipe_data->lock, flags);

		if (finished)
			break;

		/* copy the component buffers */
		if (dev->direction == STREAM_DIRECTION_PLAYBACK)
			pipeline_copy_playback(dev, 0);
		else
			pipeline_copy_capture(dev, 0);

		count++;
	}

	if (count == PIPELINE_MAX_COUNT) {
		trace_pipe_error("ePC");
	}

	tracev_pipe("PWe");
	pipe_data->copy_status = PIPELINE_COPY_IDLE;
}

/* init pipeline */
int pipeline_init(void)
{
	trace_pipe("PIn");

	pipe_data = rzalloc(RZONE_SYS, RFLAGS_NONE, sizeof(*pipe_data));
	list_init(&pipe_data->pipeline_list);
	list_init(&pipe_data->schedule_list);
	pipe_data->next_id = 0;
	spinlock_init(&pipe_data->lock);

	/* configure pipeline scheduler interrupt */
	interrupt_register(PLATFORM_PIPELINE_IRQ, pipeline_schedule, NULL);
	interrupt_enable(PLATFORM_PIPELINE_IRQ);

	return 0;
}
