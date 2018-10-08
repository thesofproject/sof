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
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/ipc.h>
#include <sof/debug.h>
#include <platform/platform.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/buffer.h>

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
		trace_ipc_error("eCe");
		trace_error_value(comp->id);
		return -EINVAL;
	}

	/* create component */
	cd = comp_new(comp);
	if (cd == NULL) {
		trace_ipc_error("eCn");
		return -EINVAL;
	}

	/* allocate the IPC component container */
	icd = rzalloc(RZONE_RUNTIME | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
		      sizeof(struct ipc_comp_dev));
	if (icd == NULL) {
		trace_ipc_error("eCm");
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
		trace_error_value(desc->comp.id);
		return -EINVAL;
	}

	/* register buffer with pipeline */
	buffer = buffer_new(desc);
	if (buffer == NULL) {
		trace_ipc_error("eBn");
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
		trace_ipc_error("eCr");
		trace_error_value(connect->source_id);
		return -EINVAL;
	}

	icd_sink = ipc_get_comp(ipc, connect->sink_id);
	if (icd_sink == NULL) {
		trace_ipc_error("eCn");
		trace_error_value(connect->sink_id);
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
		trace_error_value(connect->source_id);
		trace_error_value(connect->sink_id);
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
		trace_error_value(pipe_desc->comp_id);
		return -EINVAL;
	}

	/* find the scheduling component */
	icd = ipc_get_comp(ipc, pipe_desc->sched_id);
	if (icd == NULL) {
		trace_ipc_error("ePs");
		trace_error_value(pipe_desc->sched_id);
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
						trace_ipc_error("eCD");
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

#ifdef CONFIG_HOST_PTABLE
/*
 * Parse the host page tables and create the audio DMA SG configuration
 * for host audio DMA buffer. This involves creating a dma_sg_elem for each
 * page table entry and adding each elem to a list in struct dma_sg_config.
 */
int ipc_parse_page_descriptors(uint8_t *page_table,
			       struct sof_ipc_host_buffer *ring,
			       struct dma_sg_elem_array *elem_array,
			       uint32_t direction)
{
	int i;
	uint32_t idx;
	uint32_t phy_addr;
	struct dma_sg_elem *e;

	/* the ring size may be not multiple of the page size, the last
	 * page may be not full used. The used size should be in range
	 * of (ring->pages - 1, ring->pages] * PAGES.
	 */
	if ((ring->size <= HOST_PAGE_SIZE * (ring->pages - 1)) ||
	    (ring->size > HOST_PAGE_SIZE * ring->pages)) {
		/* error buffer size */
		trace_ipc_error("eBs");
		return -EINVAL;
	}

	elem_array->elems = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
				    sizeof(struct dma_sg_elem) * ring->pages);
	if (!elem_array->elems)
		return -ENOMEM;
	elem_array->count = ring->pages;

	for (i = 0; i < ring->pages; i++) {
		idx = (((i << 2) + i)) >> 1;
		phy_addr = page_table[idx] | (page_table[idx + 1] << 8)
				| (page_table[idx + 2] << 16);

		if (i & 0x1)
			phy_addr <<= 8;
		else
			phy_addr <<= 12;
		phy_addr &= 0xfffff000;

		e = elem_array->elems + i;

		if (direction == SOF_IPC_STREAM_PLAYBACK)
			e->src = phy_addr;
		else
			e->dest = phy_addr;

		/* the last page may be not full used */
		if (i == (ring->pages - 1))
			e->size = ring->size - HOST_PAGE_SIZE * i;
		else
			e->size = HOST_PAGE_SIZE;
	}

	return 0;
}

static void dma_complete(void *data, uint32_t type, struct dma_sg_elem *next)
{
	completion_t *complete = data;

	if (type == DMA_IRQ_TYPE_LLIST)
		wait_completed(complete);
}

/*
 * Copy the audio buffer page tables from the host to the DSP max of 4K.
 */
int ipc_get_page_descriptors(struct dma *dmac, uint8_t *page_table,
			     struct sof_ipc_host_buffer *ring)
{
	struct dma_sg_config config;
	struct dma_sg_elem elem;
	completion_t complete;
	int chan;
	int ret = 0;

	/* get DMA channel from DMAC */
	chan = dma_channel_get(dmac, 0);
	if (chan < 0) {
		trace_ipc_error("ePC");
		return chan;
	}

	/* set up DMA configuration */
	config.direction = DMA_DIR_HMEM_TO_LMEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	dma_sg_init(&config.elem_array);

	/* set up DMA descriptor */
	elem.dest = (uint32_t)page_table;
	elem.src = ring->phy_addr;

	/* source buffer size is always PAGE_SIZE bytes */
	/* 20 bits for each page, round up to 32 */
	elem.size = (ring->pages * 5 * 16 + 31) / 32;
	config.elem_array.elems = &elem;
	config.elem_array.count = 1;

	ret = dma_set_config(dmac, chan, &config);
	if (ret < 0) {
		trace_ipc_error("ePs");
		goto out;
	}

	/* set up callback */
	dma_set_cb(dmac, chan, DMA_IRQ_TYPE_LLIST, dma_complete, &complete);

	wait_init(&complete);

	/* start the copy of page table to DSP */
	ret = dma_start(dmac, chan);
	if (ret < 0) {
		trace_ipc_error("ePt");
		goto out;
	}

	/* wait for DMA to complete */
	ret = poll_for_completion_delay(&complete, PLATFORM_DMA_TIMEOUT);
	if (ret < 0)
		trace_ipc_error("eDt");

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put(dmac, chan);
	return ret;
}
#endif

int ipc_init(struct sof *sof)
{
	int i;
	trace_ipc("IPI");

	/* init ipc data */
	sof->ipc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*sof->ipc));
	sof->ipc->comp_data = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
				      SOF_IPC_MSG_MAX_SIZE);
	sof->ipc->dmat = sof->dmat;

	for (i = 0; i < PLATFORM_MAX_STREAMS; i++)
		sof->ipc->posn_map[i] = NULL;

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
