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

static struct ipc *_ipc;

/* each componnet has unique ID within the pipeline */
struct ipc_comp_dev *ipc_get_comp(uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *plist;

	list_for_item(plist, &_ipc->comp_list) {

		icd = container_of(plist, struct ipc_comp_dev, list);

		if (icd->cd->id == id)
			return icd;
	}

	return NULL;
}

/* each buffer has unique ID within the pipeline */
struct ipc_buffer_dev *ipc_get_buffer(uint32_t id)
{
	struct ipc_comp_dev *icd;
	struct list_item *plist;

	list_for_item(plist, &_ipc->buffer_list) {

		icd = container_of(plist, struct ipc_comp_dev, list);

		if (icd->cb->id == id)
			return (struct ipc_buffer_dev *)icd;
	}

	return NULL;
}

// todo make this type safe and provide new calls and accessor for each type
int ipc_comp_new(int pipeline_id, uint32_t type, uint32_t index,
	uint8_t direction)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);
	struct ipc_comp_dev *icd;

	if (p == NULL)
		return -EINVAL;

	/* allocate correct size for structure type */
	switch (type) {
	case COMP_TYPE_DAI_SSP:
	case COMP_TYPE_DAI_HDA:
		icd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(struct ipc_dai_dev));
		break;
	case COMP_TYPE_HOST:
		icd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(struct ipc_pcm_dev));
		break;
	default:
		icd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*icd));
		break;
	}
	if (icd == NULL)
		return -ENOMEM;

	/* register component with piepline */
	icd->cd = pipeline_comp_new(p, type, index, direction);
	if (icd->cd == NULL) {
		rfree(icd);
		return -ENOMEM;
	}

	/* init IPC comp */
	icd->pipeline_id = pipeline_id;
	icd->p = p;

	list_item_prepend(&icd->list, &_ipc->comp_list);
	return icd->cd->id;
}

void ipc_comp_free(uint32_t comp_id)
{
	struct ipc_comp_dev *icd = ipc_get_comp(comp_id);

	if (icd == NULL)
		return;

	pipeline_comp_free(icd->p, icd->cd);
	list_item_del(&icd->list);
	rfree(icd);
}

int ipc_buffer_new(int pipeline_id, struct buffer_desc *desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);
	struct ipc_comp_dev *icb;

	if (p == NULL)
		return -EINVAL;

	icb = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(struct ipc_buffer_dev));
	if (icb == NULL)
		return -ENOMEM;

	/* register buffer with piepline */
	icb->cb = pipeline_buffer_new(p, desc);
	if (icb->cb == NULL) {
		rfree(icb);
		return -ENOMEM;
	}

	/* init IPC buffer */
	icb->pipeline_id = pipeline_id;
	icb->p = p;

	list_item_prepend(&icb->list, &_ipc->buffer_list);
	return icb->cb->id;
}

void ipc_buffer_free(uint32_t comp_id)
{
	struct ipc_buffer_dev *icb = ipc_get_buffer(comp_id);

	if (icb == NULL)
		return;

	pipeline_buffer_free(icb->dev.p, icb->dev.cb);
	list_item_del(&icb->dev.list);
	rfree(icb);
}

int ipc_comp_connect(uint32_t source_id, uint32_t sink_id, uint32_t buffer_id)
{
	struct ipc_comp_dev *icd_source = ipc_get_comp(source_id);
	struct ipc_comp_dev *icd_sink = ipc_get_comp(sink_id);
	struct ipc_buffer_dev *ibd = ipc_get_buffer(buffer_id);

	/* valid source and sink */
	if (icd_sink == NULL || icd_source == NULL || ibd == NULL)
		return -EINVAL;

	/* must be on same pipeline */
	if (icd_sink->p != icd_source->p)
		return -EINVAL;

	return pipeline_comp_connect(icd_sink->p,
		icd_source->cd, icd_sink->cd, ibd->dev.cb);
}

int ipc_init(void)
{
	/* init ipc data */
	_ipc = rzalloc(RZONE_SYS, RFLAGS_NONE, sizeof(*_ipc));
	list_init(&_ipc->comp_list);
	list_init(&_ipc->buffer_list);
	_ipc->host_pending = 0;

	return platform_ipc_init(_ipc);
}
