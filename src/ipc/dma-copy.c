/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <uapi/intel-ipc.h>
#include <reef/reef.h>
#include <reef/debug.h>
#include <reef/trace.h>
#include <reef/dma.h>
#include <reef/wait.h>
#include <platform/dma.h>

static struct dma_sg_elem *sg_get_elem_at(struct dma_sg_config *host_sg,
	int32_t *offset)
{
	struct dma_sg_elem *host_sg_elem;
	struct list_head *plist;
	int32_t _offset = *offset;
 
	/* find host element with host_offset */
	list_for_each(plist, &host_sg->elem_list) {

		host_sg_elem = container_of(plist, struct dma_sg_elem, list);

		/* is offset in this elem ? */
		if (_offset >= 0 && _offset < host_sg_elem->size) {
			*offset = _offset;
			return host_sg_elem;
		}

		_offset -= host_sg_elem->size;
	}

	/* host offset in beyond end of SG buffer */
	//trace_mem_error("eMs");
	return NULL;
}

int dma_copy_to_host(struct dma_sg_config *host_sg, int32_t host_offset,
	void *local_ptr, int32_t size)
{
	struct dma_sg_config config;
	struct dma_sg_elem *host_sg_elem, local_sg_elem;
	struct dma *dma = dma_get(DMA_ID_DMAC0);
	completion_t complete;
	int32_t err, offset = host_offset, chan;

	/* get DMA channel from DMAC0 */
	chan = dma_channel_get(dma);
	if (chan < 0) {
		//trace_ipc_error("ePC");
		return chan;
	}

	/* find host element with host_offset */
	host_sg_elem = sg_get_elem_at(host_sg, &offset);
	if (host_sg_elem == NULL)
		return -EINVAL;

	/* set up DMA configuration */
	complete.timeout = 100;	/* wait 1 msecs for DMA to finish */
	config.direction = DMA_DIR_MEM_TO_MEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	list_init(&config.elem_list);

	/* configure local DMA elem */
	local_sg_elem.dest = host_sg_elem->dest + offset;
	local_sg_elem.src = (uint32_t)local_ptr;
	local_sg_elem.size = HOST_PAGE_SIZE - offset;
	list_add(&local_sg_elem.list, &config.elem_list);

	/* transfer max PAGE size at a time to SG buffer */
	while (size > 0) {

		/* start the DMA */
		wait_init(&complete);
		dma_set_config(dma, chan, &config);
		dma_start(dma, chan);
	
		/* wait for DMA to complete */
		err = wait_for_completion_timeout(&complete);
		if (err < 0) {
			//trace_comp_error("eAp");
			dma_channel_put(dma, chan);
			return -EIO;
		}

		/* update offset and bytes remaining */
		size -= local_sg_elem.size;
		host_offset += local_sg_elem.size;

		/* next dest host address is in next host elem */
		host_sg_elem = list_next_entry(host_sg_elem, list);
		local_sg_elem.dest = host_sg_elem->dest;

		/* local address is continuous */
		local_sg_elem.src = (uint32_t)local_ptr + local_sg_elem.size;

		/* do we have less than 1 PAGE to copy ? */
		if (size >= HOST_PAGE_SIZE)
			local_sg_elem.size = HOST_PAGE_SIZE;
		else
			local_sg_elem.size = size;
	}

	/* new host offset in SG buffer */
	dma_channel_put(dma, chan);
	return host_offset;
}

int dma_copy_from_host(struct dma_sg_config *host_sg, int32_t host_offset,
	void *local_ptr, int32_t size)
{
	struct dma_sg_config config;
	struct dma_sg_elem *host_sg_elem, local_sg_elem;
	struct dma *dma = dma_get(DMA_ID_DMAC0);
	completion_t complete;
	int32_t err, offset = host_offset, chan;

	/* get DMA channel from DMAC0 */
	chan = dma_channel_get(dma);
	if (chan < 0) {
		//trace_ipc_error("ePC");
		return chan;
	}

	/* find host element with host_offset */
	host_sg_elem = sg_get_elem_at(host_sg, &offset);
	if (host_sg_elem == NULL)
		return -EINVAL;

	/* set up DMA configuration */
	complete.timeout = 100;	/* wait 1 msecs for DMA to finish */
	config.direction = DMA_DIR_MEM_TO_MEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	list_init(&config.elem_list);

	/* configure local DMA elem */
	local_sg_elem.dest = (uint32_t)local_ptr;
	local_sg_elem.src = host_sg_elem->src + offset;
	local_sg_elem.size = HOST_PAGE_SIZE - offset;
	list_add(&local_sg_elem.list, &config.elem_list);

	/* transfer max PAGE size at a time to SG buffer */
	while (size > 0) {

		/* start the DMA */
		wait_init(&complete);
		dma_set_config(dma, chan, &config);
		dma_start(dma, chan);
	
		/* wait for DMA to complete */
		err = wait_for_completion_timeout(&complete);
		if (err < 0) {
			//trace_comp_error("eAp");
			dma_channel_put(dma, chan);
			return -EIO;
		}

		/* update offset and bytes remaining */
		size -= local_sg_elem.size;
		host_offset += local_sg_elem.size;

		/* next dest host address is in next host elem */
		host_sg_elem = list_next_entry(host_sg_elem, list);
		local_sg_elem.src = host_sg_elem->src;

		/* local address is continuous */
		local_sg_elem.dest = (uint32_t)local_ptr + local_sg_elem.size;

		/* do we have less than 1 PAGE to copy ? */
		if (size >= HOST_PAGE_SIZE)
			local_sg_elem.size = HOST_PAGE_SIZE;
		else
			local_sg_elem.size = size;
	}

	/* new host offset in SG buffer */
	dma_channel_put(dma, chan);
	return host_offset;
}
