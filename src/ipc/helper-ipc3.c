// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/drivers/idc.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int ipc_pipeline_new(struct ipc *ipc,
	struct sof_ipc_pipe_new *pipe_desc)
{
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;
	struct ipc_comp_dev *icd;

	/* check whether the pipeline already exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, pipe_desc->comp_id);
	if (ipc_pipe != NULL) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline already exists, pipe_desc->comp_id = %u",
		       pipe_desc->comp_id);
		return -EINVAL;
	}

	/* check whether pipeline id is already taken */
	ipc_pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
					  pipe_desc->pipeline_id);
	if (ipc_pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline id is already taken, pipe_desc->pipeline_id = %u",
		       pipe_desc->pipeline_id);
		return -EINVAL;
	}

	/* find the scheduling component */
	icd = ipc_get_comp_by_id(ipc, pipe_desc->sched_id);
	if (!icd) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): cannot find the scheduling component, pipe_desc->sched_id = %u",
		       pipe_desc->sched_id);
		return -EINVAL;
	}

	if (icd->type != COMP_TYPE_COMPONENT) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): icd->type (%d) != COMP_TYPE_COMPONENT for pipeline scheduling component icd->id %d",
		       icd->type, icd->id);
		return -EINVAL;
	}

	if (icd->core != pipe_desc->core) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): icd->core (%d) != pipe_desc->core (%d) for pipeline scheduling component icd->id %d",
		       icd->core, pipe_desc->core, icd->id);
		return -EINVAL;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc, icd->cd);
	if (!pipe) {
		tr_err(&ipc_tr, "ipc_pipeline_new(): pipeline_new() failed");
		return -ENOMEM;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			   sizeof(struct ipc_comp_dev));
	if (!ipc_pipe) {
		pipeline_free(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;
	ipc_pipe->core = pipe_desc->core;
	ipc_pipe->id = pipe_desc->comp_id;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);

	return 0;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core);

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_pipeline_free(): pipeline_free() failed");
		return ret;
	}
	ipc_pipe->pipeline = NULL;
	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return 0;
}

int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	uint32_t pipeline_id;
	struct ipc_comp_dev *ipc_ppl_source;
	struct ipc_comp_dev *ipc_ppl_sink;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp_by_id(ipc, comp_id);
	if (!ipc_pipe) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipe component id %d failed",
		       comp_id);
		return -EINVAL;
	}

	/* check core */
	if (!cpu_is_me(ipc_pipe->core))
		return ipc_process_on_core(ipc_pipe->core);

	pipeline_id = ipc_pipe->pipeline->ipc_pipe.pipeline_id;

	tr_dbg(&ipc_tr, "ipc: pipe %d -> complete on comp %d", pipeline_id,
	       comp_id);

	/* get pipeline source component */
	ipc_ppl_source = ipc_get_ppl_src_comp(ipc, pipeline_id);
	if (!ipc_ppl_source) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline source failed");
		return -EINVAL;
	}

	/* get pipeline sink component */
	ipc_ppl_sink = ipc_get_ppl_sink_comp(ipc, pipeline_id);
	if (!ipc_ppl_sink) {
		tr_err(&ipc_tr, "ipc: ipc_pipeline_complete looking for pipeline sink failed");
		return -EINVAL;
	}

	ret = pipeline_complete(ipc_pipe->pipeline, ipc_ppl_source->cd,
				ipc_ppl_sink->cd);

	return ret;
}

int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config)
{
	bool comp_on_core[CONFIG_CORE_COUNT] = { false };
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_reply reply;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	int ret = -ENODEV;
	int i;

	/* for each component */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		/* make sure we only config DAI comps */
		if (icd->type != COMP_TYPE_COMPONENT) {
			continue;
		}

		if (!cpu_is_me(icd->core)) {
			comp_on_core[icd->core] = true;
			ret = 0;
			continue;
		}

		if (dev_comp_type(icd->cd) == SOF_COMP_DAI ||
		    dev_comp_type(icd->cd) == SOF_COMP_SG_DAI) {
			dai = COMP_GET_IPC(icd->cd, sof_ipc_comp_dai);
			/*
			 * set config if comp dai_index matches
			 * config dai_index.
			 */
			if (dai->dai_index == config->dai_index &&
			    dai->type == config->type) {
				ret = comp_dai_config(icd->cd, config);
				if (ret < 0)
					break;
			}
		}
	}

	if (ret < 0) {
		tr_err(&ipc_tr, "ipc_comp_dai_config(): comp_dai_config() failed");
		return ret;
	}

	/* message forwarded only by primary core */
	if (!cpu_is_secondary(cpu_get_id())) {
		for (i = 0; i < CONFIG_CORE_COUNT; ++i) {
			if (!comp_on_core[i])
				continue;

			ret = ipc_process_on_core(i);
			if (ret < 0)
				return ret;

			/* check whether IPC failed on secondary core */
			mailbox_hostbox_read(&reply, sizeof(reply), 0,
					     sizeof(reply));
			if (reply.error < 0)
				/* error reply already written */
				return 1;
		}
	}

	return ret;
}

int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *desc)
{
	struct ipc_comp_dev *ibd;
	struct comp_buffer *buffer;
	int ret = 0;

	/* check whether buffer already exists */
	ibd = ipc_get_comp_by_id(ipc, desc->comp.id);
	if (ibd != NULL) {
		tr_err(&ipc_tr, "ipc_buffer_new(): buffer already exists, desc->comp.id = %u",
		       desc->comp.id);
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (!buffer) {
		tr_err(&ipc_tr, "ipc_buffer_new(): buffer_new() failed");
		return -ENOMEM;
	}

	ibd = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (!ibd) {
		buffer_free(buffer);
		return -ENOMEM;
	}
	ibd->cb = buffer;
	ibd->type = COMP_TYPE_BUFFER;
	ibd->core = desc->comp.core;
	ibd->id = desc->comp.id;

	/* add new buffer to the list */
	list_item_append(&ibd->list, &ipc->comp_list);

	return ret;
}

int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id)
{
	struct ipc_comp_dev *ibd;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct comp_dev *sink = NULL, *source = NULL;
	bool sink_active = false;
	bool source_active = false;

	/* check whether buffer exists */
	ibd = ipc_get_comp_by_id(ipc, buffer_id);
	if (!ibd)
		return -ENODEV;

	/* check core */
	if (!cpu_is_me(ibd->core))
		return ipc_process_on_core(ibd->core);

	/* try to find sink/source components to check if they still exists */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		/* check comp state if sink and source are valid */
		if (ibd->cb->sink == icd->cd &&
		    ibd->cb->sink->state != COMP_STATE_READY) {
			sink = ibd->cb->sink;
			sink_active = true;
		}

		if (ibd->cb->source == icd->cd &&
		    ibd->cb->source->state != COMP_STATE_READY) {
			source = ibd->cb->source;
			source_active = true;
		}
	}

	/*
	 * A buffer could be connected to 2 different pipelines. When one pipeline is freed, the
	 * buffer comp that belongs in this pipeline will need to be freed even when the other
	 * pipeline that the buffer is connected to is active. Check if both ends are active before
	 * freeing the buffer.
	 */
	if (sink_active && source_active)
		return -EINVAL;

	/*
	 * Disconnect the buffer from the active component before freeing it.
	 */
	if (sink)
		pipeline_disconnect(sink, ibd->cb, PPL_CONN_DIR_BUFFER_TO_COMP);

	if (source)
		pipeline_disconnect(source, ibd->cb, PPL_CONN_DIR_COMP_TO_BUFFER);

	/* free buffer and remove from list */
	buffer_free(ibd->cb);
	list_item_del(&ibd->list);
	rfree(ibd);

	return 0;
}

static int ipc_comp_to_buffer_connect(struct ipc_comp_dev *comp,
				      struct ipc_comp_dev *buffer)
{
	int ret;

	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core);

	tr_dbg(&ipc_tr, "ipc: comp sink %d, source %d  -> connect", buffer->id,
	       comp->id);

	/* check if it's a connection between cores */
	if (buffer->core != comp->core) {
		dcache_invalidate_region(buffer->cb, sizeof(*buffer->cb));

		buffer->cb->inter_core = true;

		if (!comp->cd->is_shared) {
			comp->cd = comp_make_shared(comp->cd);
			if (!comp->cd)
				return -ENOMEM;
		}
	}

	ret = pipeline_connect(comp->cd, buffer->cb,
			       PPL_CONN_DIR_COMP_TO_BUFFER);

	dcache_writeback_invalidate_region(buffer->cb, sizeof(*buffer->cb));

	return ret;
}

static int ipc_buffer_to_comp_connect(struct ipc_comp_dev *buffer,
				      struct ipc_comp_dev *comp)
{
	int ret;

	if (!cpu_is_me(comp->core))
		return ipc_process_on_core(comp->core);

	tr_dbg(&ipc_tr, "ipc: comp sink %d, source %d  -> connect", comp->id,
	       buffer->id);

	/* check if it's a connection between cores */
	if (buffer->core != comp->core) {
		dcache_invalidate_region(buffer->cb, sizeof(*buffer->cb));

		buffer->cb->inter_core = true;

		if (!comp->cd->is_shared) {
			comp->cd = comp_make_shared(comp->cd);
			if (!comp->cd)
				return -ENOMEM;
		}
	}

	ret = pipeline_connect(comp->cd, buffer->cb,
			       PPL_CONN_DIR_BUFFER_TO_COMP);

	dcache_writeback_invalidate_region(buffer->cb, sizeof(*buffer->cb));

	return ret;
}

int ipc_comp_connect(struct ipc *ipc,
	struct sof_ipc_pipe_comp_connect *connect)
{
	struct ipc_comp_dev *icd_source;
	struct ipc_comp_dev *icd_sink;

	/* check whether the components already exist */
	icd_source = ipc_get_comp_by_id(ipc, connect->source_id);
	if (!icd_source) {
		tr_err(&ipc_tr, "ipc_comp_connect(): source component does not exist, source_id = %u sink_id = %u",
		       connect->source_id, connect->sink_id);
		return -EINVAL;
	}

	icd_sink = ipc_get_comp_by_id(ipc, connect->sink_id);
	if (!icd_sink) {
		tr_err(&ipc_tr, "ipc_comp_connect(): sink component does not exist, source_id = %d sink_id = %u",
		       connect->sink_id, connect->source_id);
		return -EINVAL;
	}

	/* check source and sink types */
	if (icd_source->type == COMP_TYPE_BUFFER &&
	    icd_sink->type == COMP_TYPE_COMPONENT)
		return ipc_buffer_to_comp_connect(icd_source, icd_sink);
	else if (icd_source->type == COMP_TYPE_COMPONENT &&
		 icd_sink->type == COMP_TYPE_BUFFER)
		return ipc_comp_to_buffer_connect(icd_source, icd_sink);
	else {
		tr_err(&ipc_tr, "ipc_comp_connect(): invalid source and sink types, connect->source_id = %u, connect->sink_id = %u",
		       connect->source_id, connect->sink_id);
		return -EINVAL;
	}
}
