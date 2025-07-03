// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/ipc/driver.h>
#include <rtos/alloc.h>
#include <sof/lib/dma.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

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

	/* the ring size may be not multiple of the page size, the last
	 * page may be not full used. The used size should be in range
	 * of (ring->pages - 1, ring->pages] * PAGES.
	 */
	if ((ring->size <= HOST_PAGE_SIZE * (ring->pages - 1)) ||
	    (ring->size > HOST_PAGE_SIZE * ring->pages)) {
		/* error buffer size */
		tr_err(&ipc_tr, "ipc_parse_page_descriptors(): error buffer size");
		return -EINVAL;
	}

	elem_array->elems = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				    sizeof(struct dma_sg_elem) * ring->pages);
	if (!elem_array->elems) {
		tr_err(&ipc_tr, "ipc_parse_page_descriptors(): There is no heap free with this block size: %zu",
		       sizeof(struct dma_sg_elem) * ring->pages);
		return -ENOMEM;
	}

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
		phy_addr = host_to_local(phy_addr);

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

/*
 * Copy the audio buffer page tables from the host to the DSP max of 4K.
 */
static int ipc_get_page_descriptors(struct dma *dmac, uint8_t *page_table,
				    struct sof_ipc_host_buffer *ring)
{
	struct dma_config cfg;
	struct dma_block_config blk;
	int channel, ret;
	uint32_t align;

	/* TODO: ATM, all of this is somewhat NXP-specific as the
	 * DMA driver used by NXP performs the transfer via the
	 * reload() function which may not be the case for all
	 * vendors.
	 */
	if (!IS_ENABLED(CONFIG_DMA_NXP_SOF_HOST_DMA)) {
		tr_err(&ipc_tr, "DMAC not supported for page transfer");
		return -ENOTSUP;
	}

	channel = dma_request_channel(dmac->z_dev, 0);
	if (channel < 0) {
		tr_err(&ipc_tr, "failed to request channel");
		return channel;
	}

	/* fetch copy alignment */
	ret = dma_get_attribute(dmac->z_dev, DMA_ATTR_COPY_ALIGNMENT, &align);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to fetch copy alignment");
		goto out_release_channel;
	}

	/* prepare DMA configuration */
	cfg.source_data_size = sizeof(uint32_t);
	cfg.dest_data_size = sizeof(uint32_t);
	cfg.block_count = 1;
	cfg.head_block = &blk;
	cfg.channel_direction = HOST_TO_MEMORY;

	blk.source_address = POINTER_TO_UINT(host_to_local(ring->phy_addr));
	blk.dest_address = POINTER_TO_UINT(page_table);
	blk.block_size = ALIGN_UP(SOF_DIV_ROUND_UP(ring->pages * 20, 8), align);

	/* commit configuration */
	ret = dma_config(dmac->z_dev, channel, &cfg);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to commit configuration");
		goto out_release_channel;
	}

	/* do transfer */
	ret = dma_reload(dmac->z_dev, channel, 0, 0, 0);
	if (ret < 0)
		tr_err(&ipc_tr, "failed to perform transfer");

out_release_channel:
	dma_release_channel(dmac->z_dev, channel);

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

	/* use DMA to read in compressed page table ringbuffer from host */
	err = ipc_get_page_descriptors(data_host_buffer->dmac,
				       data_host_buffer->page_table,
				       ring);
	if (err < 0) {
		tr_err(&ipc_tr, "ipc: get descriptors failed %d", err);
		goto error;
	}

	*ring_size = ring->size;

	err = ipc_parse_page_descriptors(data_host_buffer->page_table,
					 ring,
					 elem_array, direction);
	if (err < 0) {
		tr_err(&ipc_tr, "ipc: parse descriptors failed %d", err);
		goto error;
	}

	return 0;
error:
	dma_sg_free(elem_array);
	return err;
}
