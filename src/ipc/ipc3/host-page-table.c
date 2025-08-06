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
		tr_err(&ipc_tr, "error buffer size");
		return -EINVAL;
	}

	elem_array->elems = rzalloc(SOF_MEM_FLAG_USER,
				    sizeof(struct dma_sg_elem) * ring->pages);
	if (!elem_array->elems) {
		tr_err(&ipc_tr, "There is no heap free with this block size: %zu",
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

#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
/*
 * Copy the audio buffer page tables from the host to the DSP max of 4K.
 */
static int ipc_get_page_descriptors(struct sof_dma *dmac, uint8_t *page_table,
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
#else
static int ipc_get_page_descriptors(struct dma *dmac, uint8_t *page_table,
				    struct sof_ipc_host_buffer *ring)
{
	struct dma_sg_config config;
	struct dma_sg_elem elem;
	struct dma_chan_data *chan;
	uint32_t dma_copy_align;
	int ret = 0;

	/* get DMA channel from DMAC */
	chan = dma_channel_get_legacy(dmac, 0);
	if (!chan) {
		tr_err(&ipc_tr, "chan is NULL");
		return -ENODEV;
	}

	/* set up DMA configuration */
	config.direction = DMA_DIR_HMEM_TO_LMEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	config.irq_disabled = false;
	dma_sg_init(&config.elem_array);

	ring->phy_addr = host_to_local(ring->phy_addr);
	/* set up DMA descriptor */
	elem.dest = (uintptr_t)page_table;
	elem.src = ring->phy_addr;

	/* source buffer size is always PAGE_SIZE bytes */
	/* 20 bits for each page, round up to minimum DMA copy size */
	ret = dma_get_attribute_legacy(dmac, DMA_ATTR_COPY_ALIGNMENT, &dma_copy_align);
	if (ret < 0) {
		tr_err(&ipc_tr, "dma_get_attribute() failed");
		goto out;
	}
	elem.size = SOF_DIV_ROUND_UP(ring->pages * 20, 8);
	elem.size = ALIGN_UP(elem.size, dma_copy_align);
	config.elem_array.elems = &elem;
	config.elem_array.count = 1;

	ret = dma_set_config_legacy(chan, &config);
	if (ret < 0) {
		tr_err(&ipc_tr, "dma_set_config() failed");
		goto out;
	}

	/* start the copy of page table to DSP */
	ret = dma_copy_legacy(chan, elem.size, DMA_COPY_ONE_SHOT | DMA_COPY_BLOCKING);
	if (ret < 0) {
		tr_err(&ipc_tr, "dma_start() failed");
		goto out;
	}

	/* compressed page tables now in buffer at _ipc->page_table */
out:
	dma_channel_put_legacy(chan);
	return ret;
}
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */

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
