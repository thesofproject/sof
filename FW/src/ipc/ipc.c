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
#include <reef/ipc.h>
#include <reef/debug.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <reef/audio/buffer.h>

/*
 * Components, buffers and pipelines all use the same set of monotonic ID
 * numbers passed in by the host. They are stored in different lists, hence
 * more than 1 list may need to be searched for the corresponding component.
 */

struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->comp_list) {
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

int ipc_comp_new(struct ipc *ipc, struct sof_ipc_comp *comp)
{
	struct comp_dev *cd;
	struct ipc_comp_dev *icd;
	int ret = 0;

	/* check whether component already exists */
	icd = ipc_get_comp(ipc, comp->id);
	if (icd != NULL) {
		trace_ipc_error("eCe");
		trace_value(comp->id);
		return -EINVAL;
	}

	/* create component */
	cd = comp_new(comp);
	if (cd == NULL) {
		trace_ipc_error("eCn");
		return -EINVAL;
	}

	/* allocate the IPC component container */
	icd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(struct ipc_comp_dev));
	if (icd == NULL) {
		trace_ipc_error("eCm");
		rfree(cd);
		return -ENOMEM;
	}
	icd->cd = cd;
	icd->type = COMP_TYPE_COMPONENT;

	/* add new component to the list */
	list_item_append(&icd->list, &ipc->comp_list);
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
		trace_ipc_error("eBe");
		trace_value(desc->comp.id);
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (buffer == NULL) {
		trace_ipc_error("eBn");
		rfree(ibd);
		return -ENOMEM;
	}

	ibd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(struct ipc_comp_dev));
	if (ibd == NULL) {
		rfree(buffer);
		return -ENOMEM;
	}
	ibd->cb = buffer;
	ibd->type = COMP_TYPE_BUFFER;

	/* add new buffer to the list */
	list_item_append(&ibd->list, &ipc->comp_list);
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
		trace_ipc_error("eCr");
		trace_value(connect->source_id);
		return -EINVAL;
	}

	icd_sink = ipc_get_comp(ipc, connect->sink_id);
	if (icd_sink == NULL) {
		trace_ipc_error("eCn");
		trace_value(connect->sink_id);
		return -EINVAL;
	}

	/* check source and sink types */
	if (icd_source->type == COMP_TYPE_BUFFER &&
		icd_sink->type == COMP_TYPE_COMPONENT)
		return pipeline_buffer_connect(icd_source->pipeline,
			icd_source->cb, icd_sink->cd);
	else if (icd_source->type == COMP_TYPE_COMPONENT &&
		icd_sink->type == COMP_TYPE_BUFFER)
		return pipeline_comp_connect(icd_source->pipeline,
			icd_source->cd, icd_sink->cb);
	else {
		trace_ipc_error("eCt");
		trace_value(connect->source_id);
		trace_value(connect->sink_id);
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
		trace_ipc_error("ePi");
		trace_value(pipe_desc->comp_id);
		return -EINVAL;
	}

	/* find the scheduling component */
	icd = ipc_get_comp(ipc, pipe_desc->sched_id);
	if (icd == NULL) {
		trace_ipc_error("ePs");
		trace_value(pipe_desc->sched_id);
		return -EINVAL;
	}
	if (icd->type != COMP_TYPE_COMPONENT) {
		trace_ipc_error("ePc");
		return -EINVAL;
	}

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc, icd->cd);
	if (pipe == NULL) {
		trace_ipc_error("ePn");
		return -ENOMEM;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		sizeof(struct ipc_comp_dev));
	if (ipc_pipe == NULL) {
		pipeline_free(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;
	ipc_pipe->type = COMP_TYPE_PIPELINE;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->comp_list);
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
		trace_ipc_error("ePf");
		return ret;
	}

	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);

	return 0;
}

int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *ipc_pipe;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_comp(ipc, comp_id);
	if (ipc_pipe == NULL)
		return -EINVAL;

	/* free buffer and remove from list */
	return pipeline_complete(ipc_pipe->pipeline);
}

int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	int ret = 0;

	/* for each component */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		switch (icd->type) {
		case COMP_TYPE_COMPONENT:

			/* make sure we only config DAI comps */
			switch (icd->cd->comp.type) {
			case SOF_COMP_DAI:
			case SOF_COMP_SG_DAI:
				ret = comp_dai_config(icd->cd, config);
				if (ret < 0) {
					trace_ipc_error("eCD");
					return ret;
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

int ipc_init(struct reef *reef)
{
	trace_ipc("IPI");

	/* init ipc data */
	reef->ipc = rzalloc(RZONE_SYS, RFLAGS_NONE, sizeof(*reef->ipc));
	reef->ipc->comp_data = rzalloc(RZONE_SYS, RFLAGS_NONE, SOF_IPC_MSG_MAX_SIZE);
	reef->ipc->dmat = reef->dmat;

	list_init(&reef->ipc->comp_list);

	return platform_ipc_init(reef->ipc);
}
