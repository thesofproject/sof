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

struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->cd->comp.id == id)
			return icd;
	}

	return NULL;
}

struct ipc_buffer_dev *ipc_get_buffer(struct ipc *ipc, uint32_t id)
{
	struct ipc_buffer_dev *icb;
	struct list_item *clist;

	list_for_item(clist, &ipc->buffer_list) {
		icb = container_of(clist, struct ipc_buffer_dev, list);
		if (icb->cb->ipc_buffer.comp.id == id)
			return icb;
	}

	return NULL;
}

struct ipc_pipeline_dev *ipc_get_pipeline(struct ipc *ipc, uint32_t id)
{
	struct ipc_pipeline_dev *ipd;
	struct list_item *clist;

	list_for_item(clist, &ipc->pipeline_list) {
		ipd = container_of(clist, struct ipc_pipeline_dev, list);
		if (ipd->pipeline->id == id)
			return ipd;
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

	/* add new component to the list */
	list_item_append(&icd->list, &ipc->comp_list);
	return ret;
}

void ipc_comp_free(struct ipc *ipc, uint32_t comp_id)
{
	struct ipc_comp_dev *icd;

	/* check whether component exists */
	icd = ipc_get_comp(ipc, comp_id);
	if (icd == NULL)
		return;

	/* free component and remove from list */
	comp_free(icd->cd);
	list_item_del(&icd->list);
	rfree(icd);
}

int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *desc)
{
	struct ipc_buffer_dev *ibd;
	struct comp_buffer *buffer;
	int ret = 0;

	/* check whether buffer already exists */
	ibd = ipc_get_buffer(ipc, desc->comp.id);
	if (ibd != NULL) {
		trace_ipc_error("eBe");
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (buffer == NULL) {
		trace_ipc_error("eBn");
		rfree(ibd);
		return -ENOMEM;
	}

	ibd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(struct ipc_buffer_dev));
	if (ibd == NULL) {
		rfree(buffer);
		return -ENOMEM;
	}
	ibd->cb = buffer;

	/* add new buffer to the list */
	list_item_append(&ibd->list, &ipc->buffer_list);
	return ret;
}

void ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id)
{
	struct ipc_buffer_dev *ibd;

	/* check whether buffer exists */
	ibd = ipc_get_buffer(ipc, buffer_id);
	if (ibd == NULL)
		return;

	/* free buffer and remove from list */
	buffer_free(ibd->cb);
	list_item_del(&ibd->list);
	rfree(ibd);
}

int ipc_comp_connect(struct ipc *ipc,
	struct sof_ipc_pipe_comp_connect *connect)
{
	struct ipc_comp_dev *icd_source, *icd_sink;
	struct ipc_buffer_dev *ibd;
	struct ipc_pipeline_dev *ipc_pipe;

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

	/* check whether the buffer already exists */
	ibd = ipc_get_buffer(ipc, connect->buffer_id);
	if (ibd == NULL) {
		trace_ipc_error("eCb");
		trace_value(connect->buffer_id);
		return -EINVAL;
	}

	/* check whether the pipeline already exists */
	ipc_pipe = ipc_get_pipeline(ipc, connect->pipeline_id);
	if (ipc_pipe == NULL) {
		trace_ipc_error("eCp");
		trace_value(connect->pipeline_id);
		return -EINVAL;
	}

	/* must be on same pipeline */
	//if (connect->pipeline_id != icd_source->)
		//return -EINVAL;

	return pipeline_comp_connect(ipc_pipe->pipeline,
		icd_source->cd, icd_sink->cd, ibd->cb);
}

int ipc_pipe_connect(struct ipc *ipc,
	struct sof_ipc_pipe_pipe_connect *connect)
{
	struct ipc_comp_dev *icd_source, *icd_sink;
	struct ipc_buffer_dev *ibd;
	struct ipc_pipeline_dev *ipc_pipe_source, *ipc_pipe_sink;

	/* check whether the components already exist */
	icd_source = ipc_get_comp(ipc, connect->comp_source_id);
	if (icd_source == NULL) {
		trace_ipc_error("eCr");
		trace_value(connect->comp_source_id);
		return EINVAL;
	}

	icd_sink = ipc_get_comp(ipc, connect->comp_sink_id);
	if (icd_sink == NULL) {
		trace_ipc_error("eCn");
		trace_value(connect->comp_sink_id);
		return EINVAL;
	}

	/* check whether the buffer already exists */
	ibd = ipc_get_buffer(ipc, connect->buffer_id);
	if (ibd == NULL) {
		trace_ipc_error("eCb");
		trace_value(connect->buffer_id);
		return EINVAL;
	}

	/* check whether the pipelines already exist */
	ipc_pipe_source = ipc_get_pipeline(ipc, connect->pipeline_source_id);
	if (ipc_pipe_source == NULL) {
		trace_ipc_error("eCp");
		trace_value(connect->pipeline_source_id);
		return EINVAL;
	}

	ipc_pipe_sink = ipc_get_pipeline(ipc, connect->pipeline_sink_id);
	if (ipc_pipe_sink == NULL) {
		trace_ipc_error("eCP");
		trace_value(connect->pipeline_sink_id);
		return EINVAL;
	}

	return pipeline_pipe_connect(ipc_pipe_source->pipeline,
		ipc_pipe_sink->pipeline, icd_source->cd, icd_sink->cd, ibd->cb);
}

int ipc_pipeline_new(struct ipc *ipc,
	struct sof_ipc_pipe_new *pipe_desc)
{
	struct ipc_pipeline_dev *ipc_pipe;
	struct pipeline *pipe;

	/* check whether the pipeline already exists */
	ipc_pipe = ipc_get_pipeline(ipc, pipe_desc->pipeline_id);
	if (ipc_pipe != NULL)
		return EINVAL;

	/* create the pipeline */
	pipe = pipeline_new(pipe_desc);
	if (pipe == NULL) {
		trace_ipc_error("ePn");
		return -ENOMEM;
	}

	/* allocate the IPC pipeline container */
	ipc_pipe = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		sizeof(struct ipc_pipeline_dev));
	if (ipc_pipe == NULL) {
		rfree(pipe);
		return -ENOMEM;
	}

	ipc_pipe->pipeline = pipe;

	/* add new pipeline to the list */
	list_item_append(&ipc_pipe->list, &ipc->pipeline_list);
	return 0;
}

void ipc_pipeline_free(struct ipc *ipc, uint32_t pipeline_id)
{
	struct ipc_pipeline_dev *ipc_pipe;

	/* check whether pipeline exists */
	ipc_pipe = ipc_get_pipeline(ipc, pipeline_id);
	if (ipc_pipe == NULL)
		return;

	/* free buffer and remove from list */
	pipeline_free(ipc_pipe->pipeline);
	list_item_del(&ipc_pipe->list);
	rfree(ipc_pipe);
}

int ipc_init(struct reef *reef)
{
	trace_ipc("IPI");

	/* init ipc data */
	reef->ipc = rzalloc(RZONE_SYS, RFLAGS_NONE, sizeof(*reef->ipc));
	reef->ipc->comp_data = rzalloc(RZONE_SYS, RFLAGS_NONE, SOF_IPC_MSG_MAX_SIZE);

	list_init(&reef->ipc->buffer_list);
	list_init(&reef->ipc->comp_list);
	list_init(&reef->ipc->pipeline_list);

	return platform_ipc_init(reef->ipc);
}
