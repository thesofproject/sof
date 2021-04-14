// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 4e934adb-b0ec-4d33-a086-c1022f921321 */
DECLARE_SOF_RT_UUID("pipe", pipe_uuid, 0x4e934adb, 0xb0ec, 0x4d33,
		    0xa0, 0x86, 0xc1, 0x02, 0x2f, 0x92, 0x13, 0x21);

DECLARE_TR_CTX(pipe_tr, SOF_UUID(pipe_uuid), LOG_LEVEL_INFO);

/* number of pipeline stream metadata objects we export in mailbox */
#define PPL_POSN_OFFSETS \
	(MAILBOX_STREAM_SIZE / sizeof(struct sof_ipc_stream_posn))

/* lookup table to determine busy/free pipeline metadata objects */
struct pipeline_posn {
	bool posn_offset[PPL_POSN_OFFSETS];	/**< available offsets */
	spinlock_t lock;			/**< lock mechanism */
};
/* the pipeline position lookup table */
static SHARED_DATA struct pipeline_posn pipeline_posn;

/**
 * \brief Retrieves pipeline position structure.
 * \return Pointer to pipeline position structure.
 */
static inline struct pipeline_posn *pipeline_posn_get(void)
{
	return sof_get()->pipeline_posn;
}

/**
 * \brief Retrieves first free pipeline position offset.
 * \param[in,out] posn_offset Pipeline position offset to be set.
 * \return Error code.
 */
static inline int pipeline_posn_offset_get(uint32_t *posn_offset)
{
	struct pipeline_posn *pipeline_posn = pipeline_posn_get();
	int ret = -EINVAL;
	uint32_t i;

	spin_lock(&pipeline_posn->lock);

	for (i = 0; i < PPL_POSN_OFFSETS; ++i) {
		if (!pipeline_posn->posn_offset[i]) {
			*posn_offset = i * sizeof(struct sof_ipc_stream_posn);
			pipeline_posn->posn_offset[i] = true;
			ret = 0;
			break;
		}
	}


	spin_unlock(&pipeline_posn->lock);

	return ret;
}

/**
 * \brief Frees pipeline position offset.
 * \param[in] posn_offset Pipeline position offset to be freed.
 */
static inline void pipeline_posn_offset_put(uint32_t posn_offset)
{
	struct pipeline_posn *pipeline_posn = pipeline_posn_get();
	int i = posn_offset / sizeof(struct sof_ipc_stream_posn);

	spin_lock(&pipeline_posn->lock);

	pipeline_posn->posn_offset[i] = false;


	spin_unlock(&pipeline_posn->lock);
}

void pipeline_posn_init(struct sof *sof)
{
	sof->pipeline_posn = platform_shared_get(&pipeline_posn,
						 sizeof(pipeline_posn));
	spinlock_init(&sof->pipeline_posn->lock);
}

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(struct comp_dev *cd, uint32_t pipeline_id,
			      uint32_t priority, uint32_t comp_id)
{
	struct sof_ipc_stream_posn posn;
	struct pipeline *p;
	int ret;

	pipe_cl_info("pipeline new pipe_id %d priority %d",
		     pipeline_id, priority);

	/* allocate new pipeline */
	p = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*p));
	if (!p) {
		pipe_cl_err("pipeline_new(): Out of Memory");
		return NULL;
	}

	/* init pipeline */
	p->sched_comp = cd;
	p->comp_id = comp_id;
	p->priority = priority;
	p->pipeline_id = pipeline_id;
	p->status = COMP_STATE_INIT;
	ret = memcpy_s(&p->tctx, sizeof(struct tr_ctx), &pipe_tr,
		       sizeof(struct tr_ctx));
	assert(!ret);

	ret = pipeline_posn_offset_get(&p->posn_offset);
	if (ret < 0) {
		pipe_err(p, "pipeline_new(): pipeline_posn_offset_get failed %d",
			 ret);
		rfree(p);
		return NULL;
	}

	/* just for retrieving valid ipc_msg header */
	ipc_build_stream_posn(&posn, SOF_IPC_STREAM_TRIG_XRUN, p->comp_id);

	p->msg = ipc_msg_init(posn.rhdr.hdr.cmd, sizeof(posn));
	if (!p->msg) {
		pipe_err(p, "pipeline_new(): ipc_msg_init failed");
		rfree(p);
		return NULL;
	}

	return p;
}

int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir)
{
	uint32_t flags;

	if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)
		comp_info(comp, "connect buffer %d as sink", buffer->id);
	else
		comp_info(comp, "connect buffer %d as source", buffer->id);

	irq_local_disable(flags);
	list_item_prepend(buffer_comp_list(buffer, dir),
			  comp_buffer_list(comp, dir));
	buffer_set_comp(buffer, comp, dir);
	comp_writeback(comp);
	irq_local_enable(flags);

	return 0;
}

void pipeline_disconnect(struct comp_dev *comp, struct comp_buffer *buffer, int dir)
{
	uint32_t flags;

	if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)
		comp_info(comp, "disconnect buffer %d as sink", buffer->id);
	else
		comp_info(comp, "disconnect buffer %d as source", buffer->id);

	irq_local_disable(flags);
	list_item_del(buffer_comp_list(buffer, dir));
	comp_writeback(comp);
	irq_local_enable(flags);
}

static int pipeline_comp_free(struct comp_dev *current,
			      struct comp_buffer *calling_buf,
			      struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	uint32_t flags;

	pipe_dbg(current->pipeline, "pipeline_comp_free(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		pipe_dbg(current->pipeline, "pipeline_comp_free(), current is from another pipeline");
		return 0;
	}

	/* complete component free */
	current->pipeline = NULL;

	pipeline_for_each_comp(current, ctx, dir);

	/* disconnect source from buffer */
	irq_local_disable(flags);
	list_item_del(comp_buffer_list(current, dir));
	irq_local_enable(flags);

	return 0;
}

/* pipelines must be inactive */
int pipeline_free(struct pipeline *p)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_free,
		.comp_data = &data,
	};

	pipe_info(p, "pipeline_free()");

	/* make sure we are not in use */
	if (p->source_comp) {
		if (p->source_comp->state > COMP_STATE_READY) {
			pipe_err(p, "pipeline_free(): Pipeline in use, %u, %u",
				 dev_comp_id(p->source_comp),
				 p->source_comp->state);
			return -EBUSY;
		}

		data.start = p->source_comp;

		/* disconnect components */
		walk_ctx.comp_func(p->source_comp, NULL, &walk_ctx,
				   PPL_DIR_DOWNSTREAM);
	}

	/* remove from any scheduling */
	if (p->pipe_task) {
		schedule_task_free(p->pipe_task);
		rfree(p->pipe_task);
	}

	ipc_msg_free(p->msg);

	pipeline_posn_offset_put(p->posn_offset);

	/* now free the pipeline */
	rfree(p);

	/* show heap status */
	heap_trace_all(0);

	return 0;
}

static int pipeline_comp_complete(struct comp_dev *current,
				  struct comp_buffer *calling_buf,
				  struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;

	pipe_dbg(ppl_data->p, "pipeline_comp_complete(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	if (!comp_is_single_pipeline(current, ppl_data->start)) {
		pipe_dbg(ppl_data->p, "pipeline_comp_complete(), current is from another pipeline");
		return 0;
	}

	/* complete component init */
	current->pipeline = ppl_data->p;
	current->period = ppl_data->p->period;
	current->priority = ppl_data->p->priority;

	pipeline_for_each_comp(current, ctx, dir);

	return 0;
}

int pipeline_complete(struct pipeline *p, struct comp_dev *source,
		      struct comp_dev *sink)
{
	struct pipeline_data data;
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_complete,
		.comp_data = &data,
	};

#if !UNIT_TEST && !CONFIG_LIBRARY
	int freq = clock_get_freq(cpu_get_id());
#else
	int freq = 0;
#endif

	pipe_info(p, "pipeline complete, clock freq %dHz", freq);

	/* check whether pipeline is already completed */
	if (p->status != COMP_STATE_INIT) {
		pipe_err(p, "pipeline_complete(): Pipeline already completed");
		return -EINVAL;
	}

	data.start = source;
	data.p = p;

	/* now walk downstream from source component and
	 * complete component task and pipeline initialization
	 */
	walk_ctx.comp_func(source, NULL, &walk_ctx, PPL_DIR_DOWNSTREAM);

	p->source_comp = source;
	p->sink_comp = sink;
	p->status = COMP_STATE_READY;

	/* show heap status */
	heap_trace_all(0);

	return 0;
}

static int pipeline_comp_reset(struct comp_dev *current,
			       struct comp_buffer *calling_buf,
			       struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline *p = ctx->comp_data;
	int stream_direction = dir;
	int end_type;
	int is_single_ppl = comp_is_single_pipeline(current, p->source_comp);
	int is_same_sched =
		pipeline_is_same_sched_comp(current->pipeline, p);
	int err;

	pipe_dbg(current->pipeline, "pipeline_comp_reset(), current->comp.id = %u, dir = %u",
		 dev_comp_id(current), dir);

	/* reset should propagate to the connected pipelines,
	 * which need to be scheduled together
	 */
	if (!is_single_ppl && !is_same_sched) {
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

	return pipeline_for_each_comp(current, ctx, dir);
}

/* reset the whole pipeline */
int pipeline_reset(struct pipeline *p, struct comp_dev *host)
{
	struct pipeline_walk_context walk_ctx = {
		.comp_func = pipeline_comp_reset,
		.comp_data = p,
		.buff_func = buffer_reset_params,
		.skip_incomplete = true,
	};
	int ret;

	pipe_info(p, "pipe reset");

	ret = walk_ctx.comp_func(host, NULL, &walk_ctx, host->direction);
	if (ret < 0) {
		pipe_err(p, "pipeline_reset(): ret = %d, host->comp.id = %u",
			 ret, dev_comp_id(host));
	}

	return ret;
}

/* Generic method for walking the graph upstream or downstream.
 * It requires function pointer for recursion.
 */
int pipeline_for_each_comp(struct comp_dev *current,
			   struct pipeline_walk_context *ctx, int dir)
{
	struct list_item *buffer_list = comp_buffer_list(current, dir);
	struct list_item *clist;
	struct comp_buffer *buffer;
	struct comp_dev *buffer_comp;
	uint32_t flags;

	/* run this operation further */
	list_for_item(clist, buffer_list) {
		buffer = buffer_from_list(clist, struct comp_buffer, dir);

		/* don't go back to the buffer which already walked */
		if (buffer->walking)
			continue;

		/* execute operation on buffer */
		if (ctx->buff_func)
			ctx->buff_func(buffer, ctx->buff_data);

		buffer_comp = buffer_get_comp(buffer, dir);

		/* don't go further if this component is not connected */
		if (!buffer_comp ||
		    (ctx->skip_incomplete && !buffer_comp->pipeline))
			continue;

		/* continue further */
		if (ctx->comp_func) {
			buffer_lock(buffer, &flags);
			buffer->walking = true;
			buffer_unlock(buffer, flags);

			int err = ctx->comp_func(buffer_comp, buffer,
						 ctx, dir);
			buffer_lock(buffer, &flags);
			buffer->walking = false;
			buffer_unlock(buffer, flags);
			if (err < 0)
				return err;
		}
	}

	return 0;
}
