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
#include <rtos/interrupt.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/uuid.h>
#include <sof/compiler_attributes.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/module.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(pipe, CONFIG_SOF_LOG_LEVEL);

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
	struct k_spinlock lock;			/**< lock mechanism */
};
/* the pipeline position lookup table */
static SHARED_DATA struct pipeline_posn pipeline_posn_shared;

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
	k_spinlock_key_t key;

	key = k_spin_lock(&pipeline_posn->lock);

	for (i = 0; i < PPL_POSN_OFFSETS; ++i) {
		if (!pipeline_posn->posn_offset[i]) {
			*posn_offset = i * sizeof(struct sof_ipc_stream_posn);
			pipeline_posn->posn_offset[i] = true;
			ret = 0;
			break;
		}
	}


	k_spin_unlock(&pipeline_posn->lock, key);

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
	k_spinlock_key_t key;

	key = k_spin_lock(&pipeline_posn->lock);

	pipeline_posn->posn_offset[i] = false;

	k_spin_unlock(&pipeline_posn->lock, key);
}

void pipeline_posn_init(struct sof *sof)
{
	sof->pipeline_posn = platform_shared_get(&pipeline_posn_shared,
						 sizeof(pipeline_posn_shared));
	k_spinlock_init(&sof->pipeline_posn->lock);
}

/* create new pipeline - returns pipeline id or negative error */
struct pipeline *pipeline_new(uint32_t pipeline_id, uint32_t priority, uint32_t comp_id)
{
	struct sof_ipc_stream_posn posn;
	struct pipeline *p;
	int ret;

	pipe_cl_info("pipeline new pipe_id %d priority %d",
		     pipeline_id, priority);

	/* show heap status */
	heap_trace_all(0);

	/* allocate new pipeline */
	p = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*p));
	if (!p) {
		pipe_cl_err("pipeline_new(): Out of Memory");
		return NULL;
	}

	/* init pipeline */
	p->comp_id = comp_id;
	p->priority = priority;
	p->pipeline_id = pipeline_id;
	p->status = COMP_STATE_INIT;
	p->trigger.cmd = COMP_TRIGGER_NO_ACTION;
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

	if (posn.rhdr.hdr.size) {
		p->msg = ipc_msg_init(posn.rhdr.hdr.cmd, posn.rhdr.hdr.size);
		if (!p->msg) {
			pipe_err(p, "pipeline_new(): ipc_msg_init failed");
			rfree(p);
			return NULL;
		}
	}

	return p;
}

static void buffer_set_comp(struct comp_buffer *buffer, struct comp_dev *comp,
			    int dir)
{
	struct comp_buffer __sparse_cache *buffer_c = buffer_acquire(buffer);

	if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)
		buffer_c->source = comp;
	else
		buffer_c->sink = comp;

	buffer_release(buffer_c);

	/* The buffer might be marked as shared later, write back the cache */
	if (!buffer->c.shared)
		dcache_writeback_invalidate_region(uncache_to_cache(buffer), sizeof(*buffer));
}

int pipeline_connect(struct comp_dev *comp, struct comp_buffer *buffer,
		     int dir)
{
	struct list_item *comp_list;
	uint32_t flags;

	if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)
		comp_info(comp, "connect buffer %d as sink", buffer->id);
	else
		comp_info(comp, "connect buffer %d as source", buffer->id);

	irq_local_disable(flags);
	comp_list = comp_buffer_list(comp, dir);
	/*
	 * There can already be buffers on the target component list. If we just
	 * link this buffer, we modify the first buffer's list header via
	 * uncached alias, so its cached copy can later be written back,
	 * overwriting the modified header. FIXME: this is still a problem with
	 * different cores.
	 */
	if (!list_is_empty(comp_list))
		dcache_writeback_invalidate_region(uncache_to_cache(comp_list->next),
						   sizeof(struct list_item));
	list_item_prepend(buffer_comp_list(buffer, dir), comp_list);
	buffer_set_comp(buffer, comp, dir);
	irq_local_enable(flags);

	return 0;
}

void pipeline_disconnect(struct comp_dev *comp, struct comp_buffer *buffer, int dir)
{
	struct list_item *buf_list, *comp_list;
	uint32_t flags;

	if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)
		comp_info(comp, "disconnect buffer %d as sink", buffer->id);
	else
		comp_info(comp, "disconnect buffer %d as source", buffer->id);

	irq_local_disable(flags);
	buf_list = buffer_comp_list(buffer, dir);
	comp_list = comp_buffer_list(comp, dir);
	/*
	 * There can be more buffers linked together with this one, that will
	 * still be staying on their respective pipelines and might get used via
	 * their cached aliases. If we just unlink this buffer, we modify their
	 * list header via uncached alias, so their cached copy can later be
	 * written back, overwriting the modified header. FIXME: this is still a
	 * problem with different cores.
	 */
	if (comp_list != buf_list->next)
		dcache_writeback_invalidate_region(uncache_to_cache(buf_list->next),
						   sizeof(struct list_item));
	if (comp_list != buf_list->prev)
		dcache_writeback_invalidate_region(uncache_to_cache(buf_list->prev),
						   sizeof(struct list_item));
	list_item_del(buf_list);
	irq_local_enable(flags);
}

/* pipelines must be inactive */
int pipeline_free(struct pipeline *p)
{
	pipe_info(p, "pipeline_free()");

	/*
	 * pipeline_free should always be called only after all the widgets in the pipeline have
	 * been freed.
	 */

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

	return pipeline_for_each_comp(current, ctx, dir);
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
	int ret;

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
	ret = walk_ctx.comp_func(source, NULL, &walk_ctx, PPL_DIR_DOWNSTREAM);

	p->source_comp = source;
	p->sink_comp = sink;
	p->status = COMP_STATE_READY;

	/* show heap status */
	heap_trace_all(0);

	return ret;
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

	/*
	 * Reset should propagate to the connected pipelines, which need to be
	 * scheduled together, except for IPC4, where each pipeline receives
	 * commands from the host separately
	 */
	if (!is_single_ppl && IPC4_MOD_ID(current->ipc_config.id))
		return 0;

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
	} else {
		 /* pipeline is reset to default state */
		p->status = COMP_STATE_READY;
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

	/* run this operation further */
	list_for_item(clist, buffer_list) {
		struct comp_buffer *buffer = buffer_from_list(clist, struct comp_buffer, dir);
		struct comp_buffer __sparse_cache *buffer_c;
		struct comp_dev *buffer_comp;
		int err = 0;

		if (ctx->incoming == buffer)
			continue;

		/* don't go back to the buffer which already walked */
		/*
		 * Note, that this access must be performed unlocked via
		 * uncached address. Trying to lock before checking the flag
		 * understandably leads to a deadlock when this function is
		 * called recursively from .comp_func() below. We do it in a
		 * safe way: this flag must *only* be accessed in this function
		 * only in these three cases: testing, setting and clearing.
		 * Note, that it is also assumed that buffers aren't shared
		 * across CPUs. See further comment below.
		 */
		dcache_writeback_invalidate_region(uncache_to_cache(buffer), sizeof(*buffer));
		if (buffer->walking)
			continue;

		buffer_comp = buffer_get_comp(buffer, dir);

		buffer_c = buffer_acquire(buffer);

		/* execute operation on buffer */
		if (ctx->buff_func)
			ctx->buff_func(buffer_c, ctx->buff_data);

		/* don't go further if this component is not connected */
		if (buffer_comp &&
		    (!ctx->skip_incomplete || buffer_comp->pipeline) &&
		    ctx->comp_func) {
			buffer_c->walking = true;
			buffer_release(buffer_c);

			err = ctx->comp_func(buffer_comp, buffer,
					     ctx, dir);

			buffer_c = buffer_acquire(buffer);
			buffer_c->walking = false;
		}

		buffer_release(buffer_c);

		if (err < 0 || err == PPL_STATUS_PATH_STOP)
			return err;
	}

	return 0;
}

/* visit connected pipeline to find the dai comp */
struct comp_dev *pipeline_get_dai_comp(uint32_t pipeline_id, int dir)
{
	struct ipc_comp_dev *crt;
	struct ipc *ipc = ipc_get();

	crt = ipc_get_ppl_comp(ipc, pipeline_id, dir);
	while (crt) {
		struct comp_buffer *buffer;
		struct comp_dev *comp;
		struct list_item *blist = comp_buffer_list(crt->cd, dir);

		/* if buffer list is empty then we have found a DAI */
		if (list_is_empty(blist))
			return crt->cd;

		buffer = buffer_from_list(blist->next, struct comp_buffer, dir);
		comp = buffer_get_comp(buffer, dir);

		/* buffer_comp is in another pipeline and it is not complete */
		if (!comp->pipeline)
			return NULL;

		crt = ipc_get_ppl_comp(ipc, comp->pipeline->pipeline_id, dir);
	}

	return NULL;
}

#if CONFIG_IPC_MAJOR_4
/* visit connected pipeline to find the dai comp and latency.
 * This function walks down through a pipelines chain looking for the target dai component.
 * Calculates the delay of each pipeline by determining the number of buffered blocks.
 */
struct comp_dev *pipeline_get_dai_comp_latency(uint32_t pipeline_id, uint32_t *latency)
{
	struct ipc_comp_dev *ipc_sink;
	struct ipc_comp_dev *ipc_source;
	struct comp_dev *source;
	struct ipc *ipc = ipc_get();

	*latency = 0;

	/* Walks through the ipc component list and get source endpoint component of the given
	 * pipeline.
	 */
	ipc_source = ipc_get_ppl_src_comp(ipc, pipeline_id);
	if (!ipc_source)
		return NULL;
	source = ipc_source->cd;

	/* Walks through the ipc component list and get sink endpoint component of the given
	 * pipeline. This function returns the first sink. We assume that dai is connected to pin 0.
	 */
	ipc_sink = ipc_get_ppl_sink_comp(ipc, pipeline_id);
	while (ipc_sink) {
		struct comp_buffer *buffer;
		uint64_t input_data, output_data;
		struct ipc4_base_module_cfg input_base_cfg;
		struct ipc4_base_module_cfg output_base_cfg;
		const struct comp_driver *drv;
		int ret;

		/* Calculate pipeline latency */
		input_data = comp_get_total_data_processed(source, 0, true);
		output_data = comp_get_total_data_processed(ipc_sink->cd, 0, false);
		if (!input_data || !output_data)
			return NULL;

		drv = source->drv;
		ret = drv->ops.get_attribute(source, COMP_ATTR_BASE_CONFIG, &input_base_cfg);
		if (ret < 0)
			return NULL;

		drv = ipc_sink->cd->drv;
		ret = drv->ops.get_attribute(ipc_sink->cd, COMP_ATTR_BASE_CONFIG, &output_base_cfg);
		if (ret < 0)
			return NULL;

		*latency += input_data / input_base_cfg.ibs - output_data / output_base_cfg.obs;

		/* If the component doesn't have a sink buffer, this is a dai. */
		if (list_is_empty(&ipc_sink->cd->bsink_list))
			return ipc_sink->cd;

		/* Get a component connected to our sink buffer - hop to a next pipeline */
		buffer = buffer_from_list(comp_buffer_list(ipc_sink->cd, PPL_DIR_DOWNSTREAM)->next,
					  struct comp_buffer, PPL_DIR_DOWNSTREAM);
		source = buffer_get_comp(buffer, PPL_DIR_DOWNSTREAM);

		/* buffer_comp is in another pipeline and it is not complete */
		if (!source->pipeline)
			return NULL;

		/* Get a next sink component */
		ipc_sink = ipc_get_ppl_sink_comp(ipc, source->pipeline->pipeline_id);
	}

	return NULL;
}
#endif
