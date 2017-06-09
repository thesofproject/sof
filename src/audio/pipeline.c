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
#include <reef/ipc.h>
#include <platform/timer.h>
#include <platform/platform.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

struct pipeline_data {
	spinlock_t lock;
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
static void pipeline_task(void *arg);

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc)
{
	struct pipeline *p;

	trace_pipe("PNw");

	/* allocate new pipeline */
	p = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*p));
	if (p == NULL) {
		trace_pipe_error("ePN");
		return NULL;
	}

	/* init pipeline */
	task_init(&p->pipe_task, pipeline_task, p);
	list_init(&p->comp_list);
	list_init(&p->buffer_list);
	spinlock_init(&p->lock);
	memcpy(&p->ipc_pipe, pipe_desc, sizeof(*pipe_desc));

	return p;
}

/* pipelines must be inactive */
void pipeline_free(struct pipeline *p)
{
	trace_pipe("PFr");

	/* remove from any scheduling */

	/* now free the pipeline */
//	list_item_del(&p->list);
	rfree(p);
//	spin_unlock(&pipe_data->lock);
}

int pipeline_pipe_connect(struct pipeline *psource, struct pipeline *psink,
		struct comp_dev *source_cd, struct comp_dev *sink_cd,
		struct comp_buffer *buffer)
{
	uint32_t flags;

	trace_pipe("CCn");

	spin_lock_irq(&psource->lock, flags);

	/* connect source to buffer */
	spin_lock(&source_cd->lock);
	list_item_prepend(&buffer->source_list, &source_cd->bsink_list);
	buffer->source = source_cd;
	spin_unlock(&source_cd->lock);

	/* connect sink to buffer */
	spin_lock(&sink_cd->lock);
	list_item_prepend(&buffer->sink_list, &sink_cd->bsource_list);
	buffer->sink = sink_cd;
	spin_unlock(&sink_cd->lock);

	/* connect the components */
	buffer->connected = 1;

	spin_unlock_irq(&psource->lock, flags);
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
static int component_preload(struct comp_dev *start, struct comp_dev *current,
	uint32_t depth, uint32_t max_depth)
{
	struct list_item *clist;
	int err;

	/* bail if we are at max_depth */
	if (depth > max_depth)
		return 0;

//	trace_pipe("L00");
//	trace_value(depth);
//	trace_value(current->id);

	err = comp_preload(current);

	/* dont walk the graph any further if this component fails */
	if (err < 0) {
		trace_pipe_error("eOp");
		return err;
	} else if (err > 0 || (current != start && current->is_endpoint)) {
		/* we finish walking the graph if we reach the DAI or component is
		 * currently active and configured already (err > 0).
		 */
//		trace_pipe("prl");
//		trace_value(current->id);
		return err;
	}

	/* now run this operation downstream */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		/* dont go downstream if this sink is from another pipeline */
		if (buffer->sink->pipeline != start->pipeline)
			continue;

		err = component_preload(start, buffer->sink, depth + 1, max_depth);
		if (err < 0)
			break;
	}

	return err;
}

/* work out the pipeline depth from this component to its farthest
 * downstream end point component */
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
static int component_op_downstream(struct op_data *op_data,
	struct comp_dev *start, struct comp_dev *current, int op_start)
{
	struct list_item *clist;
	int err = 0;

// TODO: params must go on stack for correct downstreaming

	trace_pipe("CO-");
	trace_value(current->comp.id);

	/* do we need to perform cmd to start component ? */
	if (op_start && start == current)
		goto downstream;

	/* do operation on this component */
	switch (op_data->op) {
	case COMP_OPS_PARAMS:
		/* send params to the component */
		err = comp_params(current, op_data->params);
		break;
	case COMP_OPS_CMD:
		/* send command to the component */
		err = comp_cmd(current, op_data->cmd, op_data->cmd_data);
		break;
	case COMP_OPS_PREPARE:
		/* prepare the component */
		err = comp_prepare(current);
		break;
	case COMP_OPS_RESET:
		/* component should reset and free resources */
		err = comp_reset(current);
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
	} else if (err > 0 || (current != start && current->is_endpoint)) {
		/* we finish walking the graph if we reach the DAI or component is
		 * currently active and configured already (err > 0).
		 */
		trace_pipe("C-D");
		return err;
	}

downstream:
	/* now run this operation downstream */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		/* dont go downstream if this sink is from another pipeline */
		if (buffer->sink->pipeline != start->pipeline)
			continue;

		err = component_op_downstream(op_data, start, buffer->sink, op_start);
		if (err < 0)
			break;
	}

	return err;
}

/* call op on all upstream components - locks held by caller */
static int component_op_upstream(struct op_data *op_data,
	struct comp_dev *start, struct comp_dev *current, int op_start)
{
	struct list_item *clist;
	int err = 0;

	trace_pipe("CO+");
	trace_value(current->comp.id);

	/* do we need to perform cmd to start component ? */
	if (op_start && start == current)
		goto upstream;

	/* do operation on this component */
	switch (op_data->op) {
	case COMP_OPS_PARAMS:
		/* send params to the component */
		err = comp_params(current, op_data->params);
		break;
	case COMP_OPS_CMD:
		/* send command to the component */
		err = comp_cmd(current, op_data->cmd, op_data->cmd_data);
		break;
	case COMP_OPS_PREPARE:
		/* prepare the component */
		err = comp_prepare(current);
		break;
	case COMP_OPS_RESET:
		/* component should reset and free resources */
		err = comp_reset(current);
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
	} else if (err > 0 || (current != start && current->is_endpoint)) {
		trace_pipe("C+D");
		return 0;
	}

upstream:
	/* now run this operation upstream */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		/* dont go upstream if this source is from another pipeline */
		if (buffer->source->pipeline != start->pipeline)
			continue;

		err = component_op_upstream(op_data, start, buffer->source, op_start);
		if (err < 0)
			break;
	}

	return err;
}


/* prepare the pipeline for usage - preload host buffers here */
int pipeline_prepare(struct pipeline *p, struct comp_dev *dev)
{
	struct sof_ipc_comp_host *host = (struct sof_ipc_comp_host *)&dev->comp;
	struct op_data op_data;
	int ret, depth, i;

	trace_pipe("Ppr");

	op_data.p = p;
	op_data.op = COMP_OPS_PREPARE;

	spin_lock(&p->lock);
	if (host->direction == SOF_IPC_STREAM_PLAYBACK) {

		/* first of all prepare the pipeline */
		ret = component_op_downstream(&op_data, dev, dev, 1);
		if (ret < 0)
			goto out;

		/* then preload buffers - the buffers must be moved
		 * downstream so that every component has full buffers for
		 * trigger start */
		depth = pipeline_depth(dev, 0);

		for (i = depth; i > 0; i--) {
			ret = component_preload(dev, dev, 0, i);
			if (ret < 0)
				break;
		}
	} else {
		ret = component_op_upstream(&op_data, dev, dev, 1);
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

	/* send cmd upstream */
	ret = component_op_upstream(&op_data, host, host, 1);
	if (ret < 0)
		goto out;

	/* send cmd downstream */
	ret = component_op_downstream(&op_data, host, host, 0);

out:
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

	/* send cmd upstream */
	ret = component_op_upstream(&op_data, host, host, 1);
	if (ret < 0)
		goto out;

	/* send cmd downstream */
	ret = component_op_downstream(&op_data, host, host, 0);

out:
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

	/* send cmd upstream */
	ret = component_op_upstream(&op_data, host, host, 1);
	if (ret < 0)
		goto out;

	/* send cmd downstream */
	ret = component_op_downstream(&op_data, host, host, 0);
out:
	spin_unlock(&p->lock);
	return ret;
}

/*
 * Upstream Copy and Process.
 *
 * Copy period(s) from all upstream sources to this component. The period will
 * be copied and processed by each component from the upstream component
 * end point(s) to the downstream components in a single operation.
 * i.e. the period data is processed from upstream end points to downstream
 * "comp" recursively in a single call to this function.
 */
static int pipeline_copy_upstream(struct comp_dev *start,
	struct comp_dev *current, int copy_start)
{
	struct list_item *clist;
	int err = 0;

//	trace_pipe("CP+");
//	trace_value(current->id);

	/* stop going upstream if we reach an end point in this pipeline */
	if (current != start && current->is_endpoint)
		goto copy;

	/* travel upstream to source end point(s) */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected || buffer->source->state != COMP_STATE_RUNNING)
			continue;

		/* dont go upstream if this source is from another pipeline */
		if (buffer->source->pipeline != start->pipeline)
			continue;

		/* continue upstream */
		err = pipeline_copy_upstream(start, buffer->source, copy_start);
		if (err < 0) {
			trace_pipe_error("ePU");
			break;
		}
	}

copy:
	/* we are at the upstream end point component so copy the buffers */
	if (current == start) {
		if (copy_start)
			err = comp_copy(current);
	} else
		err = comp_copy(current);

	/* return back downstream */
//	trace_pipe("CD+");
	return err;
}

/*
 * Downstream Copy and Process.
 *
 * Copy period(s) from this component to all downstream sinks. The period will
 * be copied and processed by each component from this component to all
 * downstream end point component(s) in a single operation.
 * i.e. the period data is processed from this component to downstream
 * end points recursively in a single call to this function.
 */
static int pipeline_copy_downstream(struct comp_dev *start,
		struct comp_dev *current, int copy_start)
{
	struct list_item *clist;
	int err = 0;

//	trace_pipe("CP-");
//	trace_value(current->id);

	/* component copy/process to downstream */
	if (current == start) {
		if (copy_start)
			err = comp_copy(current);
	} else
		err = comp_copy(current);

	/* stop going downstream if we reach an end point in this pipeline */
	if (current != start && current->is_endpoint)
		return 0;

	/* travel downstream to source end point(s) */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected || buffer->sink->state != COMP_STATE_RUNNING)
			continue;

		/* dont go downstream if this sink is from another pipeline */
		if (buffer->sink->pipeline != start->pipeline)
			continue;

		/* continue downstream */
		err = pipeline_copy_downstream(start, buffer->sink, copy_start);
		if (err < 0) {
			trace_pipe_error("ePD");
			break;
		}
	}

	/* return back upstream */
//	trace_pipe("CD-");
	return err;
}

/* notify pipeline that this component requires buffers emptied/filled */
void pipeline_schedule_copy(struct pipeline *p, struct comp_dev *dev,
	uint32_t deadline, uint32_t priority)
{
	schedule_task(&p->pipe_task, deadline, priority, dev);
	schedule();
}

static void pipeline_task(void *arg)
{
	struct pipeline *p = arg;
	struct task *task = &p->pipe_task;
	struct comp_dev *dev = task->sdata;

	trace_pipe("PWs");

	/* copy datas from upstream source components to downstream sinks */
	pipeline_copy_upstream(dev, dev, 1);
	pipeline_copy_downstream(dev, dev, 0);

	trace_pipe("PWe");
}

/* init pipeline */
int pipeline_init(void)
{
	trace_pipe("PIn");

	pipe_data = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*pipe_data));
	spinlock_init(&pipe_data->lock);

	return 0;
}
