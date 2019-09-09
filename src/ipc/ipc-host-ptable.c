// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

/*
 * Parse the host page tables and create the audio DMA SG configuration
 * for host audio DMA buffer. This involves creating a dma_sg_elem for each
 * page table entry and adding each elem to a list in struct dma_sg_config.
 */
static int ipc_parse_page_descriptors(uint8_t *page_table,
				      struct sof_ipc_host_buffer *ring,
				      struct dma_sg_elem_array *elem_array,
				      uint32_t direction)
{
	int i;
	uint32_t idx;
	uint32_t phy_addr;
	struct dma_sg_elem *e;
	trace_ipc("ipc: ipc_parse_page_descriptors");

	/* the ring size may be not multiple of the page size, the last
	 * page may be not full used. The used size should be in range
	 * of (ring->pages - 1, ring->pages] * PAGES.
	 */
	if ((ring->size <= HOST_PAGE_SIZE * (ring->pages - 1)) ||
	    (ring->size > HOST_PAGE_SIZE * ring->pages)) {
		/* error buffer size */
		trace_ipc_error("ipc_parse_page_descriptors() error: "
				"error buffer size");
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

		trace_ipc("pfn i %i idx %d addr %p", i, idx, phy_addr);

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

static void dma_complete(void *data, uint32_t type, struct dma_cb_data *next)
{
	completion_t *complete = data;

	if (type == DMA_CB_TYPE_IRQ)
		wait_completed(complete);
}

/*
 * Copy the audio buffer page tables from the host to the DSP max of 4K.
 */
static int ipc_get_page_descriptors(struct dma *dmac, uint8_t *page_table,
				    struct sof_ipc_host_buffer *ring)
{
	struct dma_sg_config config;
	struct dma_sg_elem elem;
	completion_t complete;
	struct dma_chan_data *chan;
	int ret = 0;

	trace_ipc("ipc_get_page_descriptors");
	trace_ipc("dmac is %p", (uintptr_t)(void *)dmac);
	/* get DMA channel from DMAC */
	chan = dma_channel_get(dmac, 0);
	if (!chan) {
		trace_ipc_error("ipc_get_page_descriptors() error: chan is "
				"NULL");
		return -ENODEV;
	}

	/* set up DMA configuration */
	config.direction = DMA_DIR_HMEM_TO_LMEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	config.irq_disabled = false;
	dma_sg_init(&config.elem_array);

	/* set up DMA descriptor */
	elem.dest = (uint32_t)page_table;
	elem.src = ring->phy_addr;

	/* source buffer size is always PAGE_SIZE bytes */
	/* 20 bits for each page, round up to 32 */
	elem.size = (ring->pages * 5 * 16 + 31) / 32;
	config.elem_array.elems = &elem;
	config.elem_array.count = 1;

	ret = dma_set_config(chan, &config);
	if (ret < 0) {
		trace_ipc_error("ipc_get_page_descriptors() error: "
				"dma_set_config() failed");
		goto out;
	}

	/* set up callback */
	dma_set_cb(chan, DMA_CB_TYPE_IRQ, dma_complete, &complete);

	wait_init(&complete);

	/* start the copy of page table to DSP */
	ret = dma_start(chan);
	if (ret < 0) {
		trace_ipc_error("ipc_get_page_descriptors() error: "
				"dma_start() failed");
		goto out;
	}

	/* wait for DMA to complete */
	ret = poll_for_completion_delay(&complete, PLATFORM_DMA_TIMEOUT);
	if (ret < 0)
		trace_ipc_error("ipc_get_page_descriptors() error: "
				"poll_for_completion_delay() failed");

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put(chan);
	return ret;
}

int ipc_process_host_buffer(struct ipc *ipc,
			    struct sof_ipc_host_buffer *ring,
			    uint32_t direction,
			    struct dma_sg_elem_array *elem_array,
			    uint32_t *ring_size)
{
	struct ipc_data_host_buffer *data_host_buffer;
	int err;

	data_host_buffer = ipc_platform_get_host_buffer(ipc);
	dma_sg_init(elem_array);

	trace_ipc("data_host_buffer: %p", (uintptr_t)data_host_buffer);
	trace_ipc("data_host_buffer->dmac: %p", (uintptr_t)data_host_buffer->dmac);
	trace_ipc("data_host_buffer->page_table: %p", (uintptr_t)data_host_buffer->page_table);
	trace_ipc("Getting page table descriptors");
	/* use DMA to read in compressed page table ringbuffer from host */
	err = ipc_get_page_descriptors(data_host_buffer->dmac,
				       data_host_buffer->page_table,
				       ring);
	trace_ipc("Got 'em");
	if (err < 0) {
		trace_ipc_error("ipc: get descriptors failed %d", err);
		goto error;
	}

	*ring_size = ring->size;

	trace_ipc("Parsing page table descriptors");
	err = ipc_parse_page_descriptors(data_host_buffer->page_table,
					 ring,
					 elem_array, direction);
	if (err < 0) {
		trace_ipc_error("ipc: parse descriptors failed %d", err);
		goto error;
	}

	return 0;
error:
	dma_sg_free(elem_array);
	return err;
}
