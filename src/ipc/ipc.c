// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* Returns pipeline source component */
#define ipc_get_ppl_src_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_UPSTREAM)

/* Returns pipeline sink component */
#define ipc_get_ppl_sink_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_DOWNSTREAM)

/*
 * Components, buffers and pipelines all use the same set of monotonic ID
 * numbers passed in by the host. They are stored in different lists, hence
 * more than 1 list may need to be searched for the corresponding component.
 */

struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->shared_ctx->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		switch (icd->type) {
		case COMP_TYPE_COMPONENT:
			if (icd->cd->comp.id == id)
				return icd;
			break;
		case COMP_TYPE_BUFFER:
			if (icd->cb->ipc_buffer.comp.id == id)
				return icd;
			break;
		case COMP_TYPE_PIPELINE:
			if (icd->pipeline->ipc_pipe.comp_id == id)
				return icd;
			break;
		default:
			break;
		}
	}

	return NULL;
}

static struct ipc_comp_dev *ipc_get_ppl_comp(struct ipc *ipc,
					     uint32_t pipeline_id, int dir)
{
	struct ipc_comp_dev *icd;
	struct comp_buffer *buffer;
	struct comp_dev *buff_comp;
	struct list_item *clist;

	/* first try to find the module in the pipeline */
	list_for_item(clist, &ipc->shared_ctx->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type == COMP_TYPE_COMPONENT &&
		    icd->cd->comp.pipeline_id == pipeline_id &&
		    list_is_empty(comp_buffer_list(icd->cd, dir)))
			return icd;
	}

	/* it's connected pipeline, so find the connected module */
	list_for_item(clist, &ipc->shared_ctx->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type == COMP_TYPE_COMPONENT &&
		    icd->cd->comp.pipeline_id == pipeline_id) {
			buffer = buffer_from_list
					(comp_buffer_list(icd->cd, dir)->next,
					 struct comp_buffer, dir);
			buff_comp = buffer_get_comp(buffer, dir);
			if (buff_comp &&
			    buff_comp->comp.pipeline_id != pipeline_id)
				return icd;
		}
	}

	return NULL;
}

int ipc_get_posn_offset(struct ipc *ipc, struct pipeline *pipe)
{
	int i;
	uint32_t posn_size = sizeof(struct sof_ipc_stream_posn);

	for (i = 0; i < PLATFORM_MAX_STREAMS; i++) {
		if (ipc->posn_map[i] == pipe)
			return pipe->posn_offset;
	}

	for (i = 0; i < PLATFORM_MAX_STREAMS; i++) {
		if (ipc->posn_map[i] == NULL) {
			ipc->posn_map[i] = pipe;
			pipe->posn_offset = i * posn_size;
			return pipe->posn_offset;
		}
	}

	return -EINVAL;
}

int ipc_comp_new(struct ipc *ipc, struct sof_ipc_comp *comp)
{
	struct comp_dev *cd;
	struct ipc_comp_dev *icd;
	int ret = 0;

	/* check whether component already exists */
	icd = ipc_get_comp(ipc, comp->id);
	if (icd != NULL) {
		trace_ipc_error("ipc_comp_new() error: comp->id = %u",
				comp->id);
		return -EINVAL;
	}

	/* create component */
	cd = comp_new(comp);
	if (cd == NULL) {
		trace_ipc_error("ipc_comp_new() error: component cd = NULL");
		return -EINVAL;
	}

	/* allocate the IPC component container */
	icd = rzalloc(RZONE_RUNTIME | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (icd == NULL) {
		trace_ipc_error("ipc_comp_new() error: alloc failed");
		rfree(cd);
		return -ENOMEM;
	}
	icd->cd = cd;
	icd->type = COMP_TYPE_COMPONENT;

	/* add new component to the list */
	list_item_append(&icd->list, &ipc->shared_ctx->comp_list);
	return ret;
}

int ipc_comp_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *icd;

	/* check whether component exists */
	icd = ipc_get_comp(ipc, comp_id);
	if (icd == NULL)
		return -ENODEV;

	/* free component and remove from list */
	comp_free(icd->cd);

	/* set pipeline sink/source/sched pointers to NULL if needed */
	if (icd->cd->pipeline) {
		if (icd->cd == icd->cd->pipeline->source_comp)
			icd->cd->pipeline->source_comp = NULL;
		if (icd->cd == icd->cd->pipeline->sink_comp)
			icd->cd->pipeline->sink_comp = NULL;
		if (icd->cd == icd->cd->pipeline->sched_comp)
			icd->cd->pipeline->sched_comp = NULL;
	}

	icd->cd = NULL;

	list_item_del(&icd->list);
	rfree(icd);

	return 0;
}

int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *desc)
{
	struct ipc_comp_dev *ibd;
	struct comp_buffer *buffer;
	int ret = 0;

	/* check whether buffer already exists */
	ibd = ipc_get_comp(ipc, desc->comp.id);
	if (ibd != NULL) {
		trace_ipc_error("ipc_buffer_new() error: "
				"buffer already exists, desc->comp.id = %u",
				desc->comp.id);
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (buffer == NULL) {
		trace_ipc_error("ipc_buffer_new() error: buffer_new() failed");
		rfree(ibd);
		return -ENOMEM;
	}

	ibd = rzalloc(RZONE_RUNTIME | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (ibd == NULL) {
		rfree(buffer);
		return -ENOMEM;
	}
	ibd->cb = buffer;
	ibd->type = COMP_TYPE_BUFFER;

	/* add new buffer to the list */
	list_item_append(&ibd->list, &ipc->shared_ctx->comp_list);
	return ret;
}

int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id)
{
	struct ipc_comp_dev *ibd;

	/* check whether buffer exists */
	ibd = ipc_get_comp(ipc, buffer_id);
	if (ibd == NULL)
		return -ENODEV;

	/* free buffer and remove from list */
	buffer_free(ibd->cb);
	list_item_del(&ibd->list);
	rfree(ibd);

	return 0;
}

int ipc_comp_connect(struct ipc *ipc,
	struct sof_ipc_pipe_comp_connect *connect)
{
	struct ipc_comp_dev *icd_source;
	struct ipc_comp_dev *icd_sink;

	/* check whether the components already exist */
	icd_source = ipc_get_comp(ipc, connect->source_id);
	if (icd_source == NULL) {
		trace_ipc_error("ipc_comp_connect() error: components already"
				" exist, connect->source_id = %u",
				connect->source_id);
		return -EINVAL;
	}

	icd_sink = ipc_get_comp(ipc, connect->sink_id);
	if (icd_sink == NULL) {
		trace_ipc_error("ipc_comp_connect() error: components already"
				" exist, connect->sink_id = %u",
				connect->sink_id);
		return -EINVAL;
	}

	/* check source and sink types */
	if (icd_source->type == COMP_TYPE_BUFFER &&
		icd_sink->type == COMP_TYPE_COMPONENT)
		return pipeline_connect(icd_sink->cd, icd_source->cb,
					PPL_CONN_DIR_BUFFER_TO_COMP);
	else if (icd_source->type == COMP_TYPE_COMPONENT &&
		icd_sink->type == COMP_TYPE_BUFFER)
		return pipeline_connect(icd_source->cd, icd_sink->cb,
					PPL_CONN_DIR_COMP_TO_BUFFER);
	else {
		trace_ipc_error("ipc_comp_connect() error: invalid source and"
				" sink types, connect->source_id = %u, "
				"connect->sink_id = %u",
				connect->source_id, connect->sink_id);
		return -EINVAL;
	}
}


int ipc_pipeline_new(struct ipc *ipc,
	struct sof_ipc_pipe_new *pipe_desc)
{
	struct ipc_comp_dev *ipc_pipe;
	struct pipeline *pipe;
	struct ipc_comp_dev *icd;

	/* check whether the pipeline already exists */
	ipc_pipe = ipc_get_comp(ipc, pipe_desc->comp_id);
	if (ipc_pipe != NULL) {
		trace_ipc_error("ipc_pipeline_new() error: pipeline already"
				" exists, pipe_desc->comp_id = %u",
				pipe_desc->comp_id);
		return -EINVAL;
	}

	/* find the scheduling component */
	icd = ipc_get_comp(ipc, pipe_desc->sched_id);
	if (icd == NULL) {
		trace_ipc_error("ipc_pipeline_new() error: cannot find the "
				"scheduling component, pipe_desc->sched_id"
				" = %u", pipe_desc->sched_id);
		return -EINVAL;
	}
	if (icd->type != COMP_TYPE_COMPONENT) {
		trace_ipc_error("ipc_pipeline_new() error: "
				"icd->type != COMP_TYPE_COMPONENT");
		return -EINVAL;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc, icd->cd);
	if (pipe == NULL) {
		trace_ipc_error("ipc_pipeline_new() error: "
				"pipeline_new() failed");
		return -ENOMEM;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(RZONE_RUNTIME | RZONE_FLAG_UNCACHED,
			   SOF_MEM_CAPS_RAM, sizeof(struct ipc_comp_dev));
	if (ipc_pipe == NULL) {
		pipeline_free(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->shared_ctx->comp_list);
	return 0;
}

int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;
	int ret;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp(ipc, comp_id);
	if (ipc_pipe == NULL)
		return -ENODEV;

	/* free buffer and remove from list */
	ret = pipeline_free(ipc_pipe->pipeline);
	if (ret < 0) {
		trace_ipc_error("ipc_pipeline_free() error: "
				"pipeline_free() failed");
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

	trace_pipe("ipc_pipeline_complete");
	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp(ipc, comp_id);
	if (!ipc_pipe)
		return -EINVAL;

	pipeline_id = ipc_pipe->pipeline->ipc_pipe.pipeline_id;

	/* get pipeline source component */
	ipc_ppl_source = ipc_get_ppl_src_comp(ipc, pipeline_id);
	if (!ipc_ppl_source)
		return -EINVAL;

	/* get pipeline sink component */
	ipc_ppl_sink = ipc_get_ppl_sink_comp(ipc, pipeline_id);
	if (!ipc_ppl_sink)
		return -EINVAL;

	trace_pipe("ipc_pipeline_complete about to call pipeline_complete");
	return pipeline_complete(ipc_pipe->pipeline, ipc_ppl_source->cd,
				 ipc_ppl_sink->cd);
}

int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config)
{
	struct sof_ipc_comp_dai *dai;
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	struct comp_dev *dev;
	int ret = 0;

	/* for each component */
	list_for_item(clist, &ipc->shared_ctx->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		switch (icd->type) {
		case COMP_TYPE_COMPONENT:

			/* make sure we only config DAI comps */
			switch (icd->cd->comp.type) {
			case SOF_COMP_DAI:
			case SOF_COMP_SG_DAI:
				dev = icd->cd;
				dai = (struct sof_ipc_comp_dai *)&dev->comp;

				/*
				 * set config if comp dai_index matches
				 * config dai_index.
				 */
				if (dai->dai_index == config->dai_index &&
				    dai->type == config->type) {
					ret = comp_dai_config(dev, config);
					if (ret < 0) {
						trace_ipc_error
						("ipc_comp_dai_config() "
						"error: comp_dai_config() "
						"failed");
						return ret;
					}
				}
				break;
			default:
				break;
			}

			break;
		/* ignore non components */
		default:
			break;
		}
	}

	return ret;
}

int ipc_init(struct sof *sof)
{
	int i;

	trace_ipc("ipc_init()");

	/* init ipc data */
	sof->ipc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*sof->ipc));
	sof->ipc->comp_data = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
				      SOF_IPC_MSG_MAX_SIZE);
	sof->ipc->dmat = sof->dmat;

	spinlock_init(&sof->ipc->lock);

	sof->ipc->shared_ctx = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED,
				       SOF_MEM_CAPS_RAM,
				       sizeof(*sof->ipc->shared_ctx));

	dcache_writeback_region(sof->ipc, sizeof(*sof->ipc));

	sof->ipc->shared_ctx->dsp_msg = NULL;
	list_init(&sof->ipc->shared_ctx->empty_list);
	list_init(&sof->ipc->shared_ctx->msg_list);
	list_init(&sof->ipc->shared_ctx->comp_list);

	for (i = 0; i < MSG_QUEUE_SIZE; i++)
		list_item_prepend(&sof->ipc->shared_ctx->message[i].list,
				  &sof->ipc->shared_ctx->empty_list);

	return platform_ipc_init(sof->ipc);
}
