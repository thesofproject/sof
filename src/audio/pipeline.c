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

/* call op on all upstream components - locks held by caller */
static void connect_upstream(struct pipeline *p, struct comp_dev *start,
	struct comp_dev *current)
{
	struct list_item *clist;

	tracev_value(current->comp.id);

	/* complete component init */
	current->pipeline = p;
	current->frames = p->ipc_pipe.frames_per_sched;

	/* we are an endpoint if we have 0 source components */
	if (list_is_empty(&current->bsource_list)) {
		current->is_endpoint = 1;
		return;
	}

	/* now run this operation upstream */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this source is from another pipeline */
		if (buffer->source->comp.pipeline_id != p->ipc_pipe.pipeline_id)
			continue;

		connect_upstream(p, start, buffer->source);
	}

}

static void connect_downstream(struct pipeline *p, struct comp_dev *start,
	struct comp_dev *current)
{
	struct list_item *clist;

	tracev_value(current->comp.id);

	/* complete component init */
	current->pipeline = p;
	current->frames = p->ipc_pipe.frames_per_sched;

	/* we are an endpoint if we have 0 sink components */
	if (list_is_empty(&current->bsink_list)) {
		current->is_endpoint = 1;
		return;
	}

	/* now run this operation downstream */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this sink is from another pipeline */
		if (buffer->sink->comp.pipeline_id != p->ipc_pipe.pipeline_id)
			continue;

		connect_downstream(p, start, buffer->sink);
	}
}

/* call op on all upstream components - locks held by caller */
static void disconnect_upstream(struct pipeline *p, struct comp_dev *start,
	struct comp_dev *current)
{
	struct list_item *clist;

	tracev_value(current->comp.id);

	/* complete component init */
	current->pipeline = NULL;

	/* now run this operation upstream */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this source is from another pipeline */
		if (buffer->source->comp.pipeline_id != p->ipc_pipe.pipeline_id)
			continue;

		connect_upstream(p, start, buffer->source);
	}

	/* diconnect source from buffer */
	spin_lock(&current->lock);
	list_item_del(&current->bsource_list);
	spin_unlock(&current->lock);
}

static void disconnect_downstream(struct pipeline *p, struct comp_dev *start,
	struct comp_dev *current)
{
	struct list_item *clist;

	tracev_value(current->comp.id);

	/* complete component init */
	current->pipeline = NULL;

	/* now run this operation downstream */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this sink is from another pipeline */
		if (buffer->sink->comp.pipeline_id != p->ipc_pipe.pipeline_id)
			continue;

		connect_downstream(p, start, buffer->sink);
	}

	/* diconnect source from buffer */
	spin_lock(&current->lock);
	list_item_del(&current->bsink_list);
	spin_unlock(&current->lock);
}

/* update pipeline state based on cmd */
static void pipeline_cmd_update(struct pipeline *p, int cmd)
{
	switch (cmd) {
	case COMP_CMD_PAUSE:
		break;
	case COMP_CMD_STOP:
		break;
	case COMP_CMD_RELEASE:
		p->xrun_bytes = 0;
		break;
	case COMP_CMD_START:
		p->xrun_bytes = 0;
		break;
	case COMP_CMD_SUSPEND:
		break;
	case COMP_CMD_RESUME:
		p->xrun_bytes = 0;
		break;
	}
}

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc,
	struct comp_dev *cd)
{
	struct pipeline *p;

	trace_pipe("new");

	/* allocate new pipeline */
	p = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*p));
	if (p == NULL) {
		trace_pipe_error("ePN");
		return NULL;
	}

	/* init pipeline */
	p->sched_comp = cd;
	task_init(&p->pipe_task, pipeline_task, p);
	list_init(&p->comp_list);
	list_init(&p->buffer_list);
	spinlock_init(&p->lock);
	memcpy(&p->ipc_pipe, pipe_desc, sizeof(*pipe_desc));

	return p;
}

/* pipelines must be inactive */
int pipeline_free(struct pipeline *p)
{
	trace_pipe("fre");

	/* make sure we are not in use */
	if (p->sched_comp->state > COMP_STATE_SETUP) {
		trace_pipe_error("epb");
		return -EBUSY;
	}

	/* remove from any scheduling */
	task_free(&p->pipe_task);

	/* disconnect components */
	disconnect_downstream(p, p->sched_comp, p->sched_comp);
	disconnect_upstream(p, p->sched_comp, p->sched_comp);

	/* now free the pipeline */
	rfree(p);

	return 0;
}

void pipeline_complete(struct pipeline *p)
{
	/* now walk downstream and upstream form "start" component and
	  complete component task and pipeline init */

	trace_pipe("com");
	tracev_value(p->ipc_pipe.pipeline_id);

	connect_downstream(p, p->sched_comp, p->sched_comp);
	connect_upstream(p, p->sched_comp, p->sched_comp);
}

/* connect component -> buffer */
int pipeline_comp_connect(struct pipeline *p, struct comp_dev *source_comp,
	struct comp_buffer *sink_buffer)
{
	trace_pipe("cnc");

	/* connect source to buffer */
	spin_lock(&source_comp->lock);
	list_item_prepend(&sink_buffer->source_list, &source_comp->bsink_list);
	sink_buffer->source = source_comp;
	spin_unlock(&source_comp->lock);

	/* connect the components */
	if (sink_buffer->source && sink_buffer->sink)
		sink_buffer->connected = 1;

	tracev_value((source_comp->comp.id << 16) |
		sink_buffer->ipc_buffer.comp.id);
	return 0;
}

/* connect buffer -> component */
int pipeline_buffer_connect(struct pipeline *p,
	struct comp_buffer *source_buffer, struct comp_dev *sink_comp)
{
	trace_pipe("cbc");

	/* connect sink to buffer */
	spin_lock(&sink_comp->lock);
	list_item_prepend(&source_buffer->sink_list, &sink_comp->bsource_list);
	source_buffer->sink = sink_comp;
	spin_unlock(&sink_comp->lock);

	/* connect the components */
	if (source_buffer->source && source_buffer->sink)
		source_buffer->connected = 1;

	tracev_value((source_buffer->ipc_buffer.comp.id << 16) |
		sink_comp->comp.id);
	return 0;
}

/* call op on all downstream components - locks held by caller */
static int component_op_downstream(struct op_data *op_data,
	struct comp_dev *start, struct comp_dev *current,
	struct comp_dev *previous)
{
	struct list_item *clist;
	int err = 0;

	tracev_pipe("CO-");
	tracev_value(current->comp.id);

	/* do operation on this component */
	switch (op_data->op) {
	case COMP_OPS_PARAMS:

		/* dont do any params downstream if current is running */
		if (current->state == COMP_STATE_RUNNING)
			return 0;

		/* send params to the component */
		if (current != start && previous != NULL)
			comp_install_params(current, previous);
		err = comp_params(current);
		break;
	case COMP_OPS_CMD:
		/* send command to the component and update pipeline state  */
		err = comp_cmd(current, op_data->cmd, op_data->cmd_data);
		if (err == 0)
			pipeline_cmd_update(current->pipeline, op_data->cmd);
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
		tracev_pipe("C-D");
		return err;
	}

	/* now run this operation downstream */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = component_op_downstream(op_data, start, buffer->sink,
			current);
		if (err < 0)
			break;
	}

	return err;
}

/* call op on all upstream components - locks held by caller */
static int component_op_upstream(struct op_data *op_data,
	struct comp_dev *start, struct comp_dev *current,
	struct comp_dev *previous)
{
	struct list_item *clist;
	int err = 0;

	tracev_pipe("CO+");
	tracev_value(current->comp.id);

	/* do operation on this component */
	switch (op_data->op) {
	case COMP_OPS_PARAMS:

		/* dont do any params upstream if current is running */
		if (current->state == COMP_STATE_RUNNING)
			return 0;

		/* send params to the component */
		if (current != start && previous != NULL)
			comp_install_params(current, previous);
		err = comp_params(current);
		break;
	case COMP_OPS_CMD:
		/* send command to the component and update pipeline state  */
		err = comp_cmd(current, op_data->cmd, op_data->cmd_data);
		if (err == 0)
			pipeline_cmd_update(current->pipeline, op_data->cmd);
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
		tracev_pipe("C+D");
		return 0;
	}

	/* now run this operation upstream */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected)
			continue;

		err = component_op_upstream(op_data, start, buffer->source,
			current);
		if (err < 0)
			break;
	}

	return err;
}

/* preload all downstream components - locks held by caller */
static int preload_downstream(struct comp_dev *start, struct comp_dev *current)
{
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(current);
	struct list_item *clist;
	int i, total = 0, count = 0;

	trace_pipe("PR-");
	tracev_value(current->comp.id);

	/* reached endpoint ? */
	if (current != start && current->is_endpoint)
		return 0;

	/* now preload the buffers */
	for (i = 0; i < config->preload_count; i++) {
		count = comp_preload(current);

		if (count < 0)
			return count;
		total += count;
	}

	/* now run this operation downstream */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		count = preload_downstream(start, buffer->sink);
		if (count < 0)
			return count;

		total += count;
	}

	return total;
}

/* prepare the pipeline for usage - preload host buffers here */
int pipeline_prepare(struct pipeline *p, struct comp_dev *dev)
{
	struct op_data op_data;
	int ret, i;

	trace_pipe("pre");

	op_data.p = p;
	op_data.op = COMP_OPS_PREPARE;

	spin_lock(&p->lock);

	/* playback pipelines can be preloaded from host before trigger */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {

		/* first of all prepare the pipeline */
		ret = component_op_downstream(&op_data, dev, dev, NULL);
		if (ret < 0)
			goto out;

		/* then preload buffers - the buffers must be moved
		 * downstream so that every component has full buffers for
		 * trigger start */
		for (i = 0; i < MAX_PRELOAD_SIZE; i++) {

			ret = preload_downstream(dev, dev);

			/* complete ? */
			if (ret <= 0)
				goto out;
		}

		/* failed to preload */
		trace_pipe_error("epl");
		ret = -EIO;

	} else {
		ret = component_op_upstream(&op_data, dev, dev, NULL);
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

	trace_pipe("cmd");

	op_data.p = p;
	op_data.op = COMP_OPS_CMD;
	op_data.cmd = cmd;
	op_data.cmd_data = data;

	spin_lock(&p->lock);

	if (host->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		/* send cmd downstream from host to DAI */
		ret = component_op_downstream(&op_data, host, host, NULL);
	} else {
		/* send cmd upstream from host to DAI */
		ret = component_op_upstream(&op_data, host, host, NULL);
	}

	if (ret < 0)
		trace_ipc_error("pae");

	spin_unlock(&p->lock);
	return ret;
}

/*
 * Send pipeline component params from host to endpoints.
 * Params always start at host (PCM) and go downstream for playback and
 * upstream for capture.
 *
 * Playback params can be re-written by upstream components. e.g. upstream SRC
 * can change sample rate for all downstream components regardless of sample
 * rate from host.
 *
 * Capture params can be re-written by downstream components.
 *
 * Params are always modified in the direction of host PCM to DAI.
 */
int pipeline_params(struct pipeline *p, struct comp_dev *host,
	struct sof_ipc_pcm_params *params)
{
	struct op_data op_data;
	int ret;

	trace_pipe("Par");

	op_data.p = p;
	op_data.op = COMP_OPS_PARAMS;

	spin_lock(&p->lock);

	host->params = params->params;

	if (host->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		/* send params downstream from host to DAI */
		ret = component_op_downstream(&op_data, host, host, NULL);
	} else {
		/* send params upstream from host to DAI */
		ret = component_op_upstream(&op_data, host, host, NULL);
	}

	if (ret < 0)
		trace_ipc_error("pae");

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

	if (host->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		/* send reset downstream from host to DAI */
		ret = component_op_downstream(&op_data, host, host, NULL);
	} else {
		/* send reset upstream from host to DAI */
		ret = component_op_upstream(&op_data, host, host, NULL);
	}

	if (ret < 0)
		trace_ipc_error("pae");

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
static int pipeline_copy_from_upstream(struct comp_dev *start,
	struct comp_dev *current)
{
	struct list_item *clist;
	int err = 0;

	tracev_pipe("CP+");
	tracev_value(current->comp.id);

	/* stop going upstream if we reach an end point in this pipeline */
	if (current->is_endpoint && current != start)
		goto copy;

	/* travel upstream to source end point(s) */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go upstream if this component is not connected */
		if (!buffer->connected || buffer->source->state != COMP_STATE_RUNNING)
			continue;

		/* continue upstream */
		err = pipeline_copy_from_upstream(start, buffer->source);
		if (err < 0) {
			trace_pipe_error("ePU");
			break;
		}
	}

copy:
	/* we are at the upstream end point component so copy the buffers */
	err = comp_copy(current);

	/* return back downstream */
	tracev_pipe("CD+");
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
static int pipeline_copy_to_downstream(struct comp_dev *start,
		struct comp_dev *current)
{
	struct list_item *clist;
	int err = 0;

	tracev_pipe("CP-");
	tracev_value(current->comp.id);

	/* component copy/process to downstream */
	if (current != start) {
		err = comp_copy(current);

		/* stop going downstream if we reach an end point in this pipeline */
		if (current->is_endpoint)
			return 0;
	}

	/* travel downstream to sink end point(s) */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected || buffer->sink->state != COMP_STATE_RUNNING)
			continue;

		/* continue downstream */
		err = pipeline_copy_to_downstream(start, buffer->sink);
		if (err < 0) {
			trace_pipe_error("ePD");
			break;
		}
	}

	/* return back upstream */
	tracev_pipe("CD-");
	return err;
}

static int timestamp_downstream(struct comp_dev *start,
		struct comp_dev *current, struct sof_ipc_stream_posn *posn)
{
	struct list_item *clist;
	int res = 0;

	/* is component a DAI endpoint ? */
	if (current != start) {

		/* go downstream if we are not endpoint */
		if (!current->is_endpoint)
			goto downstream;

		if (current->comp.type == SOF_COMP_DAI ||
			current->comp.type == SOF_COMP_SG_DAI) {
			platform_dai_timestamp(current, posn);
			return 1;
		}
	}

downstream:
	/* travel downstream to sink end point(s) */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected || buffer->sink->state != COMP_STATE_RUNNING)
			continue;

		/* continue downstream */
		res = timestamp_downstream(start, buffer->sink, posn);
		if (res == 1)
			break;
	}

	/* return back upstream */
	return res;
}


static int timestamp_upstream(struct comp_dev *start,
		struct comp_dev *current, struct sof_ipc_stream_posn *posn)
{
	struct list_item *clist;
	int res = 0;

	/* is component a DAI endpoint ? */
	if (current != start) {

		/* go downstream if we are not endpoint */
		if (!current->is_endpoint)
			goto upstream;

		if (current->comp.type == SOF_COMP_DAI ||
			current->comp.type == SOF_COMP_SG_DAI) {
			platform_dai_timestamp(current, posn);
			return 1;
		}
	}


upstream:
	/* travel upstream to source end point(s) */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected || buffer->source->state != COMP_STATE_RUNNING)
			continue;

		/* continue downstream */
		res = timestamp_upstream(start, buffer->sink, posn);
		if (res == 1)
			break;
	}

	/* return back upstream */
	return res;
}

/*
 * Get the timestamps for host and first active DAI found.
 */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host,
	struct sof_ipc_stream_posn *posn)
{
	platform_host_timestamp(host, posn);

	if (host->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		timestamp_downstream(host, host, posn);
	} else {
		timestamp_upstream(host, host, posn);
	}
}

static void xrun(struct comp_dev *dev, void *data)
{
	struct sof_ipc_stream_posn *posn = data;

	/* get host timestamps */
	platform_host_timestamp(dev, posn);

	/* send XRUN to host */
	ipc_stream_send_xrun(dev, posn);
}


/* travel down stream from start and run func for each component of type */
static void pipeline_for_each_downstream(struct pipeline *p,
	enum sof_comp_type type, struct comp_dev *current,
	void (*func)(struct comp_dev *, void *), void *data)
{
	struct list_item *clist;

	if (current->comp.type == type)
		func(current, data);

	/* travel downstream to sink end point(s) */
	list_for_item(clist, &current->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		/* continue downstream */
		pipeline_for_each_downstream(p, type, buffer->sink,
			func, data);
	}
}

/* travel up stream from start and run func for each component of type */
static void pipeline_for_each_upstream(struct pipeline *p,
	enum sof_comp_type type, struct comp_dev *current,
	void (*func)(struct comp_dev *, void *), void *data)
{
	struct list_item *clist;

	if (current->comp.type == type)
		func(current, data);

	/* travel upstream to sink end point(s) */
	list_for_item(clist, &current->bsource_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, sink_list);

		/* dont go downstream if this component is not connected */
		if (!buffer->connected)
			continue;

		/* continue downstream */
		pipeline_for_each_upstream(p, type, buffer->source,
			func, data);
	}
}

/*
 * Send an XRUN to each host for this component.
 */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev,
	int32_t bytes)
{
	struct sof_ipc_stream_posn posn;

	/* dont flood host */
	if (p->xrun_bytes)
		return;

	memset(&posn, 0, sizeof(posn));
	p->xrun_bytes = posn.xrun_size = bytes;
	posn.xrun_comp_id = dev->comp.id;

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		pipeline_for_each_upstream(p, SOF_COMP_HOST, dev, xrun, &posn);
	} else {
		pipeline_for_each_downstream(p, SOF_COMP_HOST, dev, xrun, &posn);
	}
}

/* notify pipeline that this component requires buffers emptied/filled */
void pipeline_schedule_copy(struct pipeline *p, struct comp_dev *dev)
{
	schedule_task(&p->pipe_task, p->ipc_pipe.deadline,
		p->ipc_pipe.priority, dev);
	schedule();
}

static void pipeline_task(void *arg)
{
	struct pipeline *p = arg;
	struct task *task = &p->pipe_task;
	struct comp_dev *dev = task->sdata;

	tracev_pipe("PWs");

	/* copy data from upstream source enpoints to downstream endpoints */
	pipeline_copy_from_upstream(dev, dev);
	pipeline_copy_to_downstream(dev, dev);

	tracev_pipe("PWe");
}

/* init pipeline */
int pipeline_init(void)
{
	trace_pipe("PIn");

	pipe_data = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*pipe_data));
	spinlock_init(&pipe_data->lock);

	return 0;
}
