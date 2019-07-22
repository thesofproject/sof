// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/list.h>
#include <sof/schedule/schedule.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* generic pipeline data used by pipeline_comp_* functions */
struct pipeline_data {
	struct comp_dev *start;
	struct sof_ipc_pcm_params *params;
	struct sof_ipc_stream_posn *posn;
	struct pipeline *p;
	int cmd;
};

static uint64_t pipeline_task(void *arg);

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(struct sof_ipc_pipe_new *pipe_desc,
			      struct comp_dev *cd)
{
	struct pipeline *p;
	uint32_t type;

	trace_pipe("pipeline_new()");

	/* allocate new pipeline */
	p = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*p));
	if (!p) {
		trace_pipe_error("pipeline_new() error: Out of Memory");
		return NULL;
	}

	/* init pipeline */
	p->sched_comp = cd;
	p->status = COMP_STATE_INIT;

	spinlock_init(&p->lock);
	assert(!memcpy_s(&p->ipc_pipe, sizeof(p->ipc_pipe),
	   pipe_desc, sizeof(*pipe_desc)));

	/* get pipeline task type */
	type = pipeline_is_timer_driven(p) ? SOF_SCHEDULE_LL :
		SOF_SCHEDULE_EDF;
	schedule_task_init(&p->pipe_task, type, pipe_desc->priority,
			   pipeline_task, p, pipe_desc->core, 0);

	return p;
}

int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir)
{
	trace_pipe("pipeline: connect comp %d and buffer %d",
		   comp->comp.id, buffer->ipc_buffer.comp.id);

	spin_lock(&comp->lock);
	list_item_prepend(buffer_comp_list(buffer, dir),
			  comp_buffer_list(comp, dir));
	buffer_set_comp(buffer, comp, dir);
	spin_unlock(&comp->lock);

	return 0;
}

#define PPL_ACCORDING_TO_STREAM_DIR		(1 << 0)
#define PPL_CONTRARY_TO_STREAM_DIR		(1 << 1)

/* Generic method for walking the graph upstream or downstream inside the
 * pipeline
 */
static int pipeline_for_each_comp(struct pipeline *p,
				  struct comp_dev *current,
				  void *data,
				  int (*func)(struct comp_dev *, void *,
					      int),
				  void (*buff_func)(struct comp_buffer *),
				  int dir)
{
	struct list_item *buffer_list;
	struct list_item *clist;
	struct comp_buffer *buffer;
	struct comp_dev *end_comp;
	struct comp_dev *beg_comp;
	struct comp_dev *process_comp;
	int stop = 0;
	int err = 0;

	trace_pipe_with_ids(p, "pipeline_for_each_comp()");

	/* beg and end component in pipeline */
	end_comp = dir == PPL_DIR_UPSTREAM ? p->source_comp : p->sink_comp;
	beg_comp = dir == PPL_DIR_UPSTREAM ? p->sink_comp : p->source_comp;

	process_comp = current ? current : beg_comp;

	if (!process_comp) {
		trace_pipe_with_ids(p, "pipeline_for_each_comp(): beginning"
				       " component does not exist.");
		return -EINVAL;
	}

	while (!stop) {
		if (process_comp == end_comp)
			stop = 1;

		if (func) {
			err = func(process_comp, data, dir);
			if (err < 0 || err == PPL_STATUS_PATH_STOP)
				break;
		}

		buffer_list = comp_buffer_list(process_comp, dir);

		if (list_is_empty(buffer_list))
			break;

		/* Going through connected buffers. Only beg and end components
		 * could have more than one sink/source buffers
		 */
		list_for_item(clist, buffer_list) {
			buffer = buffer_from_list(clist, struct comp_buffer,
						  dir);

			/* execute operation on buffer */
			if (buff_func)
				buff_func(buffer);

			process_comp = buffer_get_comp(buffer, dir);
		}
	}

	return err;
}

/* Function checks whether there is any pipeline connected to the pipeline
 * currently being processed. If there is, it invokes proper function on
 * this pipeline.
 */
static int pipeline_for_each_pipe(struct pipeline *p,
				  struct comp_dev *current,
				  void *data,
				  int (*func)(struct comp_dev *, void *, int),
				  void (*buff_func)(struct comp_buffer *),
				  int dir,
				  uint32_t flags)
{
	struct comp_dev *end_comp;
	struct comp_dev *beg_comp;
	struct comp_dev *process_comp;
	struct comp_dev *comp;
	struct comp_buffer *buffer;
	struct list_item *buffer_list;
	struct list_item *clist;

	int pipe_search_dir;
	int ret = 0;

	trace_pipe_with_ids(p, "pipeline_for_each_pipe()");

	/* beginning component in pipeline */
	end_comp = dir == PPL_DIR_UPSTREAM ? p->source_comp : p->sink_comp;
	beg_comp = dir == PPL_DIR_UPSTREAM ? p->sink_comp : p->source_comp;

	process_comp = current ? current : beg_comp;

	if (!process_comp) {
		trace_pipe_with_ids(p, "pipeline_for_each_pipe(): "
				       "beginning component does not exist.");
		return -EINVAL;
	}

	/* for PPL_ACCORDING_TO_STREAM_DIR firstly we invoke comp func
	 * on currently being processed pipeline and later we search for
	 * another connected pipeline.
	 */
	if (flags & PPL_ACCORDING_TO_STREAM_DIR) {
		ret = pipeline_for_each_comp(p, process_comp, data, func,
					     buff_func, dir);
		if (ret < 0 || ret == PPL_STATUS_PATH_STOP)
			return ret;
	}

	if (flags & PPL_CONTRARY_TO_STREAM_DIR) {
		pipe_search_dir = dir == PPL_DIR_UPSTREAM ?
				  PPL_DIR_DOWNSTREAM : PPL_DIR_UPSTREAM;
		buffer_list = comp_buffer_list(beg_comp, pipe_search_dir);
	} else {
		pipe_search_dir = dir;
		buffer_list = comp_buffer_list(end_comp, pipe_search_dir);
	}

	/* searching for connected pipelines */
	list_for_item(clist, buffer_list) {
		buffer = buffer_from_list(clist, struct comp_buffer,
					  pipe_search_dir);
		comp = buffer_get_comp(buffer, pipe_search_dir);

		/* recursive call for connected pipelines */
		pipeline_for_each_pipe(comp->pipeline, NULL, data, func,
				       buff_func, dir, flags);
	}

	/* for PPL_CONTRARY_TO_STREAM_DIR firstly we search for another
	 * connected pipeline and later we invoke comp func
	 * on pipeline.
	 */
	if (flags & PPL_CONTRARY_TO_STREAM_DIR) {
		ret = pipeline_for_each_comp(p, process_comp, data, func,
					     buff_func, dir);

		if (ret < 0 || ret == PPL_STATUS_PATH_STOP)
			return ret;
	}

	return ret;
}

static int pipeline_comp_complete(struct comp_dev *current, void *data,
				  int dir)
{
	struct pipeline_data *ppl_data = data;

	tracev_pipe_with_ids(ppl_data->p, "pipeline_comp_complete(), "
			     "current->comp.id = %u, dir = %u",
			     current->comp.id, dir);

	/* complete component init */
	current->pipeline = ppl_data->p;
	current->frames = ppl_data->p->ipc_pipe.frames_per_sched;

	return 0;
}

int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink)
{
	struct pipeline_data data;

	trace_pipe_with_ids(p, "pipeline_complete()");

	/* check whether pipeline is already completed */
	if (p->status != COMP_STATE_INIT) {
		trace_pipe_error_with_ids(p, "pipeline_complete() error: "
					  "Pipeline already completed");
		return -EINVAL;
	}

	data.start = source;
	data.p = p;

	/* now walk downstream from source component and
	 * complete component task and pipeline initialization
	 */
	p->source_comp = source;
	p->sink_comp = sink;

	pipeline_for_each_comp(p, source, &data, pipeline_comp_complete, NULL,
			       PPL_DIR_DOWNSTREAM);

	p->status = COMP_STATE_READY;

	/* show heap status */
	heap_trace_all(0);

	return 0;
}

static int pipeline_comp_free(struct comp_dev *current, void *data, int dir)
{
	int disconnect_dir;

	trace_pipe("pipeline_comp_free(), current->comp.id = %u, dir = %u",
		   current->comp.id, dir);

	/* complete component free */
	current->pipeline = NULL;

	/* direction of disconnecting buffers should be opposite to walking
	 * through the component in pipelines
	 */
	disconnect_dir = dir == PPL_DIR_UPSTREAM ?
			 PPL_DIR_DOWNSTREAM : PPL_DIR_UPSTREAM;

	/* disconnect source from buffer */
	spin_lock(&current->lock);
	list_item_del(comp_buffer_list(current, disconnect_dir));
	spin_unlock(&current->lock);

	return 0;
}

/* pipelines must be inactive */
int pipeline_free(struct pipeline *p)
{
	struct pipeline_data data;

	trace_pipe_with_ids(p, "pipeline_free()");

	/* make sure we are not in use */
	if (p->sink_comp) {
		if (p->sink_comp->state > COMP_STATE_READY) {
			trace_pipe_error_with_ids(p, "pipeline_free() error: "
						  "Pipeline in use, %u, %u",
						  p->sink_comp->comp.id,
						  p->sink_comp->state);
			return -EBUSY;
		}

		data.start = p->sink_comp;

		/* disconnect components */
		pipeline_for_each_comp(p, NULL, &data, pipeline_comp_free, NULL,
				       PPL_DIR_UPSTREAM);
	}

	/* remove from any scheduling */
	schedule_task_free(&p->pipe_task);

	/* now free the pipeline */
	rfree(p);

	/* show heap status */
	heap_trace_all(0);

	return 0;
}

static int pipeline_comp_params(struct comp_dev *current, void *data, int dir)
{
	struct pipeline_data *ppl_data = data;
	int stream_direction = ppl_data->params->params.direction;
	int end_type;
	int err = 0;

	tracev_pipe("pipeline_comp_params(), current->comp.id = %u, dir = %u",
		    current->comp.id, dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		/* If pipeline connected to the starting one is in improper
		 * direction (CAPTURE towards DAI, PLAYBACK towards HOST),
		 * stop propagation of parameters not to override their config.
		 * Direction param of the pipeline can not be trusted at this
		 * point, as it might not be configured yet, hence checking
		 * for endpoint component type.
		 */
		end_type = comp_get_endpoint_type(current->pipeline->sink_comp);
		if (stream_direction == SOF_IPC_STREAM_PLAYBACK) {
			if (end_type == COMP_ENDPOINT_HOST ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}

		if (stream_direction == SOF_IPC_STREAM_CAPTURE) {
			if (end_type == COMP_ENDPOINT_DAI ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}
	}

	/* don't do any params if current is running */
	if (current->state == COMP_STATE_ACTIVE)
		return 0;

	/* send current params to the component */
	current->params = ppl_data->params->params;

	err = comp_params(current);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	/* save params changes made by component */
	ppl_data->params->params = current->params;

	return err;
}

/* Send pipeline component params from host to endpoints.
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
	struct pipeline_data data;
	int ret;
	uint32_t flags;

	trace_pipe_with_ids(p, "pipeline_params()");

	data.params = params;
	data.start = host;

	spin_lock_irq(&p->lock, flags);

	ret = pipeline_for_each_pipe(host->pipeline, host, &data,
				     pipeline_comp_params, NULL,
				     host->params.direction,
				     PPL_ACCORDING_TO_STREAM_DIR);

	if (ret < 0) {
		trace_pipe_error("pipeline_params() error: ret = %d, host->"
				 "comp.id = %u", ret, host->comp.id);
	}

	spin_unlock_irq(&p->lock, flags);

	return ret;
}

static int pipeline_comp_prepare(struct comp_dev *current, void *data, int dir)
{
	int err = 0;
	struct pipeline_data *ppl_data = data;
	int stream_direction = ppl_data->start->params.direction;
	int end_type;

	tracev_pipe("pipeline_comp_prepare(), current->comp.id = %u, dir = %u",
		    current->comp.id, dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		/* If pipeline connected to the starting one is in improper
		 * direction (CAPTURE towards DAI, PLAYBACK towards HOST),
		 * stop propagation. Direction param of the pipeline can not be
		 * trusted at this point, as it might not be configured yet,
		 * hence checking for endpoint component type.
		 */
		end_type = comp_get_endpoint_type(current->pipeline->sink_comp);
		if (stream_direction == SOF_IPC_STREAM_PLAYBACK) {
			if (end_type == COMP_ENDPOINT_HOST ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}

		if (stream_direction == SOF_IPC_STREAM_CAPTURE) {
			if (end_type == COMP_ENDPOINT_DAI ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}
	}

	err = comp_prepare(current);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return err;
}

/* prepare the pipeline for usage - preload host buffers here */
int pipeline_prepare(struct pipeline *p, struct comp_dev *dev)
{
	struct pipeline_data ppl_data;
	int ret = 0;
	uint32_t flags;

	trace_pipe_with_ids(p, "pipeline_prepare()");

	ppl_data.start = dev;

	spin_lock_irq(&p->lock, flags);

	ret = pipeline_for_each_pipe(dev->pipeline, dev, &ppl_data,
				     pipeline_comp_prepare, NULL,
				     dev->params.direction,
				     PPL_ACCORDING_TO_STREAM_DIR);

	if (ret < 0) {
		trace_pipe_error("pipeline_prepare() error: ret = %d,"
				 "dev->comp.id = %u", ret, dev->comp.id);
		goto out;
	}

	/* pipeline preload needed only for playback streams without active
	 * sink component (it can be active for e.g. mixer pipelines)
	 */
	p->preload = dev->params.direction == SOF_IPC_STREAM_PLAYBACK &&
		p->sink_comp->state != COMP_STATE_ACTIVE;
	p->status = COMP_STATE_PREPARE;

out:
	spin_unlock_irq(&p->lock, flags);
	return ret;
}

static int pipeline_comp_cache(struct comp_dev *current, void *data, int dir)
{
	struct pipeline_data *ppl_data = data;

	tracev_pipe("pipeline_comp_cache(), current->comp.id = %u, dir = %u",
		    current->comp.id, dir);

	comp_cache(current, ppl_data->cmd);

	return 0;
}

/* execute cache operation on pipeline */
void pipeline_cache(struct pipeline *p, struct comp_dev *dev, int cmd)
{
	struct pipeline_data data;
	uint32_t flags;

	/* pipeline needs to be invalidated before usage */
	if (cmd == CACHE_INVALIDATE)
		dcache_invalidate_region(p, sizeof(*p));

	trace_pipe_with_ids(p, "pipeline_cache()");

	data.start = dev;
	data.cmd = cmd;

	spin_lock_irq(&p->lock, flags);

	/* execute cache operation on components and buffers */
	pipeline_comp_cache(dev, &data, dev->params.direction);

	pipeline_for_each_comp(dev->pipeline, dev, &data, pipeline_comp_cache,
			       comp_buffer_cache_op(data.cmd),
			       dev->params.direction);

	/* pipeline needs to be flushed after usage */
	if (cmd == CACHE_WRITEBACK_INV)
		dcache_writeback_invalidate_region(p, sizeof(*p));

	spin_unlock_irq(&p->lock, flags);
}

static void pipeline_comp_trigger_sched_comp(struct pipeline *p,
					     struct comp_dev *comp, int cmd)
{
	/* only required by the scheduling component */
	if (p->sched_comp != comp)
		return;

	switch (cmd) {
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_XRUN:
		pipeline_schedule_cancel(p);
		p->status = COMP_STATE_PAUSED;
		break;
	case COMP_TRIGGER_RELEASE:
	case COMP_TRIGGER_START:
		p->xrun_bytes = 0;

		/* playback pipelines need to be scheduled now,
		 * capture pipelines are scheduled only for
		 * timer driven scheduling
		 */
		if (comp->params.direction == SOF_IPC_STREAM_PLAYBACK ||
		    pipeline_is_timer_driven(p)) {
			/* schedule initial pipeline fill when next idle */
			pipeline_schedule_copy_idle(p);
		}
		p->status = COMP_STATE_ACTIVE;
		break;
	case COMP_TRIGGER_SUSPEND:
	case COMP_TRIGGER_RESUME:
	default:
		break;
	}
}

static int pipeline_comp_trigger(struct comp_dev *current, void *data, int dir)
{
	struct pipeline_data *ppl_data = data;
	int is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	int is_same_sched =
		pipeline_is_same_sched_comp(current->pipeline,
					    ppl_data->start->pipeline);
	int err = 0;

	trace_pipe("pipeline_comp_trigger(), current->comp.id = %u, dir = %u",
		   current->comp.id, dir);

	/* trigger should propagate to the connected pipelines,
	 * which need to be scheduled together
	 */
	if (!is_single_ppl && !is_same_sched) {
		tracev_pipe_with_ids(current->pipeline, "pipeline_comp_trigger"
				     "(), current is from another pipeline");
		return 0;
	}

	/* send command to the component and update pipeline state */
	err = comp_trigger(current, ppl_data->cmd);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	pipeline_comp_trigger_sched_comp(current->pipeline, current,
					 ppl_data->cmd);
	return err;
}

/* trigger pipeline on slave core */
static int pipeline_trigger_on_core(struct pipeline *p, struct comp_dev *host,
				    int cmd)
{
	struct idc_msg pipeline_trigger = { IDC_MSG_PPL_TRIGGER,
		IDC_MSG_PPL_TRIGGER_EXT(cmd), p->ipc_pipe.core };
	int ret;

	/* check if requested core is enabled */
	if (!cpu_is_core_enabled(p->ipc_pipe.core)) {
		trace_pipe_error_with_ids(p, "pipeline_trigger_on_core() "
					  "error: Requested core is not "
					  "enabled, p->ipc_pipe.core = %u",
					  p->ipc_pipe.core);
		return -EINVAL;
	}

	/* writeback pipeline on start */
	if (cmd == COMP_TRIGGER_START)
		pipeline_cache(p, host, CACHE_WRITEBACK_INV);

	/* send IDC pipeline trigger message */
	ret = idc_send_msg(&pipeline_trigger, IDC_BLOCKING);
	if (ret < 0) {
		trace_pipe_error_with_ids(p, "pipeline_trigger_on_core() "
					  "error: idc_send_msg returned %d, "
					  "host->comp.id = %u, cmd = %d",
					  ret, host->comp.id, cmd);
		return ret;
	}

	/* invalidate pipeline on stop */
	if (cmd == COMP_TRIGGER_STOP)
		pipeline_cache(p, host, CACHE_INVALIDATE);

	return ret;
}

/*
 * trigger handler for pipelines in xrun, used for recovery from host only.
 * return values:
 *	0 -- success, further trigger in caller needed.
 *	PPL_STATUS_PATH_STOP -- done, no more further trigger needed.
 *	minus -- failed, caller should return failure.
 */
static int pipeline_xrun_handle_trigger(struct pipeline *p, int cmd)
{
	int ret = 0;

	/* it is expected in paused status for xrun pipeline */
	if (!p->xrun_bytes || p->status != COMP_STATE_PAUSED)
		return 0;

	/* in xrun, handle start/stop trigger */
	switch (cmd) {
	case COMP_TRIGGER_START:
		/* in xrun, prepare before trigger start needed */
		trace_pipe_with_ids(p, "in xrun, prepare it first");
		/* prepare the pipeline */
		ret = pipeline_prepare(p, p->source_comp);
		if (ret < 0) {
			trace_pipe_error_with_ids(p, "prepare error: ret = %d",
						  ret);
			return ret;
		}
		/* now ready for start, clear xrun_bytes */
		p->xrun_bytes = 0;
		break;
	case COMP_TRIGGER_STOP:
		/* in xrun, suppose pipeline is already stopped, ignore it */
		trace_pipe_with_ids(p, "already stopped in xrun");
		/* no more further trigger stop needed */
		ret = PPL_STATUS_PATH_STOP;
		break;
	default:
		break;
	}

	return ret;
}

/* trigger pipeline */
int pipeline_trigger(struct pipeline *p, struct comp_dev *host, int cmd)
{
	struct pipeline_data data;
	int ret;
	uint32_t flags;

	trace_pipe_with_ids(p, "pipeline_trigger()");

	/* handle pipeline global checks before going into each components */
	if (p->xrun_bytes) {
		ret = pipeline_xrun_handle_trigger(p, cmd);
		if (ret < 0) {
			trace_pipe_error_with_ids(p, "xrun handle error: "
						  "ret = %d", ret);
			return ret;
		} else if (ret == PPL_STATUS_PATH_STOP)
			/* no further action needed*/
			return 0;
	}

	/* if current core is different than requested */
	if (p->ipc_pipe.core != cpu_get_id())
		return pipeline_trigger_on_core(p, host, cmd);

	data.start = host;
	data.cmd = cmd;

	spin_lock_irq(&p->lock, flags);

	ret = pipeline_for_each_pipe(host->pipeline, host, &data,
				     pipeline_comp_trigger, NULL,
				     host->params.direction,
				     PPL_ACCORDING_TO_STREAM_DIR);

	if (ret < 0) {
		trace_ipc_error("pipeline_trigger() error: ret = %d, host->"
				"comp.id = %u, cmd = %d", ret, host->comp.id,
				cmd);
	}

	spin_unlock_irq(&p->lock, flags);
	return ret;
}

static int pipeline_comp_reset(struct comp_dev *current, void *data, int dir)
{
	struct pipeline *p = data;
	int stream_direction = p->source_comp->params.direction;
	int end_type;
	int err = 0;

	tracev_pipe("pipeline_comp_reset(), current->comp.id = %u, dir = %u",
		    current->comp.id, dir);

	if (!comp_is_single_pipeline(current, p->source_comp)) {
		/* If pipeline connected to the starting one is in improper
		 * direction (CAPTURE towards DAI, PLAYBACK towards HOST),
		 * stop propagation. Direction param of the pipeline can not be
		 * trusted at this point, as it might not be configured yet,
		 * hence checking for endpoint component type.
		 */
		end_type = comp_get_endpoint_type(current->pipeline->sink_comp);
		if (stream_direction == SOF_IPC_STREAM_PLAYBACK) {
			if (end_type == COMP_ENDPOINT_HOST ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}

		if (stream_direction == SOF_IPC_STREAM_CAPTURE) {
			if (end_type == COMP_ENDPOINT_DAI ||
			    end_type == COMP_ENDPOINT_NODE)
				return 0;
		}
	}

	err = comp_reset(current);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return err;
}

/* reset the whole pipeline */
int pipeline_reset(struct pipeline *p, struct comp_dev *host)
{
	int ret = 0;
	uint32_t flags;

	trace_pipe_with_ids(p, "pipeline_reset()");

	spin_lock_irq(&p->lock, flags);

	ret = pipeline_for_each_pipe(host->pipeline, host, p,
				     pipeline_comp_reset, NULL,
				     host->params.direction,
				     PPL_ACCORDING_TO_STREAM_DIR);

	if (ret < 0) {
		trace_pipe_error("pipeline_reset() error: ret = %d, host->comp."
				 "id = %u", ret, host->comp.id);
	}

	spin_unlock_irq(&p->lock, flags);
	return ret;
}

static int pipeline_comp_copy(struct comp_dev *current, void *data, int dir)
{
	struct pipeline_data *ppl_data = data;
	int is_single_ppl = comp_is_single_pipeline(current, ppl_data->start);
	int is_same_sched =
		pipeline_is_same_sched_comp(current->pipeline, ppl_data->p);
	int err = 0;

	trace_pipe("pipeline_comp_copy(), current->comp.id = %u, dir = %u",
		   current->comp.id, dir);
	trace_pipe("pipeline_comp_copy(), current->state = %u",
		   current->state);

	if (!is_single_ppl && !is_same_sched) {
		trace_pipe("pipeline_comp_copy(), current is from another "
			   "pipeline and can't be scheduled together");
		return err;
	}

	if (!comp_is_active(current)) {
		trace_pipe("pipeline_comp_copy(), current is not active");
		return err;
	}

	/* in case when pipeline is not in preload state comp_copy() on
	 * sched_comp is perform earlier in pipeline_copy()
	 */
	if (current->params.direction == SOF_IPC_STREAM_PLAYBACK &&
	    ppl_data->p->sched_comp == current && !ppl_data->p->preload) {
		trace_pipe("pipeline_comp_copy(), current is sched_comp, "
			   "pipline is not in preload state");
		return err;
	}

	err = comp_copy(current);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return err;
}

/* Copy data across all pipeline components.
 * For capture pipelines it always starts from source component
 * and continues downstream. For playback pipelines there are two
 * possibilities: for preload it starts from sink component and
 * continues upstream and if not preload, then it first copies
 * sink component itself and then goes upstream.
 */
static int pipeline_copy(struct pipeline *p)
{
	struct pipeline_data data;
	struct comp_dev *start;
	uint32_t dir;
	uint32_t ppl_processing;
	int ret = 0;

	tracev_pipe("pipeline_copy()");

	if (p->source_comp->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		dir = PPL_DIR_DOWNSTREAM;
		start = p->sink_comp;

		/* if not pipeline preload then copy sink comp first */
		if (!p->preload) {
			ret = comp_copy(start);
			if (ret < 0) {
				trace_pipe_error("pipeline_copy() error: "
						 "ret = %d", ret);
				return ret;
			}
		}
	} else {
		dir = PPL_DIR_DOWNSTREAM;
		start = p->source_comp;
	}

	data.start = start;
	data.p = p;

	ppl_processing = p->source_comp->params.direction ==
		SOF_IPC_STREAM_PLAYBACK ? PPL_CONTRARY_TO_STREAM_DIR :
		PPL_ACCORDING_TO_STREAM_DIR;

	ret = pipeline_for_each_pipe(start->pipeline, NULL, &data,
				     pipeline_comp_copy, NULL, dir,
				     ppl_processing);

	if (ret < 0)
		trace_pipe_error("pipeline_copy() error: ret = %d, start"
				 "->comp.id = %u, dir = %u", ret,
				 start->comp.id, dir);

	/* stop preload only after full walkthrough */
	if (ret != PPL_STATUS_PATH_STOP)
		p->preload = false;

	return ret;
}

/* Walk the graph to active components in any pipeline to find
 * the first active DAI and return it's timestamp.
 */
static int pipeline_comp_timestamp(struct comp_dev *current, void *data,
				   int dir)
{
	struct pipeline_data *ppl_data = data;

	if (!comp_is_active(current)) {
		tracev_pipe("pipeline_comp_timestamp(), "
			    "current is not active");
		return 0;
	}

	/* is component a DAI endpoint? */
	if (current != ppl_data->start &&
	    (current->comp.type == SOF_COMP_DAI ||
	    current->comp.type == SOF_COMP_SG_DAI)) {
		platform_dai_timestamp(current, ppl_data->posn);
		return -1;
	}

	return 0;
}

/* Get the timestamps for host and first active DAI found. */
void pipeline_get_timestamp(struct pipeline *p, struct comp_dev *host,
			    struct sof_ipc_stream_posn *posn)
{
	struct pipeline_data data;

	platform_host_timestamp(host, posn);

	data.start = host;
	data.posn = posn;

	pipeline_for_each_pipe(host->pipeline, host, &data,
			       pipeline_comp_timestamp, NULL,
			       host->params.direction,
			       PPL_ACCORDING_TO_STREAM_DIR);

	/* set timestamp resolution */
	posn->timestamp_ns = p->ipc_pipe.period * 1000;
}

static int pipeline_comp_xrun(struct comp_dev *current, void *data, int dir)
{
	struct pipeline_data *ppl_data = data;

	if (current->comp.type == SOF_COMP_HOST) {
		/* get host timestamps */
		platform_host_timestamp(current, ppl_data->posn);


		/* send XRUN to host */
		ipc_stream_send_xrun(current, ppl_data->posn);
	}

	return 0;
}

/* Send an XRUN to each host for this component. */
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev,
		   int32_t bytes)
{
	struct pipeline_data data;
	struct sof_ipc_stream_posn posn;
	int ret;

	/* don't flood host */
	if (p->xrun_bytes)
		return;

	/* only send when we are running */
	if (dev->state != COMP_STATE_ACTIVE)
		return;

	/* notify all pipeline comps we are in XRUN, and stop copying */
	ret = pipeline_trigger(p, p->source_comp, COMP_TRIGGER_XRUN);
	if (ret < 0)
		trace_pipe_error_with_ids(p, "pipeline_xrun() error: "
					  "Pipelines notification about XRUN "
					  "failed, ret = %d", ret);

	memset(&posn, 0, sizeof(posn));
	p->xrun_bytes = bytes;
	posn.xrun_size = bytes;
	posn.xrun_comp_id = dev->comp.id;
	data.posn = &posn;

	pipeline_for_each_pipe(dev->pipeline, dev, &data, pipeline_comp_xrun,
			       NULL, dev->params.direction,
			       PPL_ACCORDING_TO_STREAM_DIR);
}

#if NO_XRUN_RECOVERY
/* recover the pipeline from a XRUN condition */
static int pipeline_xrun_recover(struct pipeline *p)
{
	return -EINVAL;
}

#else
/* recover the pipeline from a XRUN condition */
static int pipeline_xrun_recover(struct pipeline *p)
{
	int ret;

	trace_pipe_error_with_ids(p, "pipeline_xrun_recover()");

	/* prepare the pipeline */
	ret = pipeline_prepare(p, p->source_comp);
	if (ret < 0) {
		trace_pipe_error_with_ids(p, "pipeline_xrun_recover() error: "
					  "pipeline_prepare() failed, "
					  "ret = %d", ret);
		return ret;
	}

	/* reset xrun status as we already in prepared */
	p->xrun_bytes = 0;

	/* restart pipeline comps */
	ret = pipeline_trigger(p, p->source_comp, COMP_TRIGGER_START);
	if (ret < 0) {
		trace_pipe_error_with_ids(p, "pipeline_xrun_recover() error: "
					  "pipeline_trigger() failed, "
					  "ret = %d", ret);
		return ret;
	}

	return 0;
}
#endif

/* notify pipeline that this component requires buffers emptied/filled */
void pipeline_schedule_copy(struct pipeline *p, uint64_t start)
{
	if (p->sched_comp->state == COMP_STATE_ACTIVE)
		schedule_task(&p->pipe_task, start, p->ipc_pipe.period, 0);
}

/* notify pipeline that this component requires buffers emptied/filled
 * when DSP is next idle. This is intended to be used to preload pipeline
 * buffers prior to trigger start.
 */
void pipeline_schedule_copy_idle(struct pipeline *p)
{
	schedule_task(&p->pipe_task, 0, p->ipc_pipe.period,
		      SOF_SCHEDULE_FLAG_IDLE);
}

void pipeline_schedule_cancel(struct pipeline *p)
{
	int err;

	/* cancel and wait for pipeline to complete */
	err = schedule_task_cancel(&p->pipe_task);
	if (err < 0)
		trace_pipe_error_with_ids(p, "pipeline_schedule_cancel() "
					  "error: schedule_task_cancel() "
					  "failed, err = %d", err);
}

static uint64_t pipeline_task(void *arg)
{
	struct pipeline *p = arg;
	int err;

	tracev_pipe_with_ids(p, "pipeline_task()");

	/* are we in xrun ? */
	if (p->xrun_bytes) {
		/* try to recover */
		err = pipeline_xrun_recover(p);
		if (err < 0)
			return 0;/* skip copy if still in xrun */
	}

	err = pipeline_copy(p);
	if (err < 0) {
		/* try to recover */
		err = pipeline_xrun_recover(p);
		if (err < 0) {
			trace_pipe_error_with_ids(p, "pipeline_task(): xrun "
						  "recover failed! pipeline "
						  "will be stopped!");
			return 0; /* failed - host will stop this pipeline */
		}
	}

	trace_pipe_with_ids(p, "pipeline_task() sched");

	/* automatically reschedule for timer or not finished preload */
	return (pipeline_is_timer_driven(p) || p->preload) ?
		p->ipc_pipe.period : 0;
}
