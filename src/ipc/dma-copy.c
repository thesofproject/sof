// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/ipc/msg.h>
#include <sof/lib/dma.h>
#include <sof/lib/uuid.h>
#include <sof/platform.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* tracing */
LOG_MODULE_REGISTER(dma_copy, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(dma_copy);

DECLARE_TR_CTX(dmacpy_tr, SOF_UUID(dma_copy_uuid), LOG_LEVEL_INFO);

static struct dma_sg_elem *sg_get_elem_at(struct dma_sg_config *host_sg,
	int32_t *offset)
{
	struct dma_sg_elem *host_sg_elem;
	int i;
	int32_t _offset = *offset;

	/* find host element with host_offset */
	for (i = 0; i < host_sg->elem_array.count; i++) {

		host_sg_elem = host_sg->elem_array.elems + i;

		/* is offset in this elem ? */
		if (_offset >= 0 && _offset < host_sg_elem->size) {
			*offset = _offset;
			return host_sg_elem;
		}

		_offset -= host_sg_elem->size;
	}

	/* host offset in beyond end of SG buffer */
	tr_err(&dmacpy_tr, "host offset in beyond end of SG buffer");
	return NULL;
}

/* Copy DSP memory to host memory.
 * Copies DSP memory to host in a single PAGE_SIZE or smaller block. Does not
 * waits/sleeps and can be used in IRQ context.
 */

static int
dma_copy_to_host_flags(struct dma_copy *dc, struct dma_sg_config *host_sg,
		       int32_t host_offset, void *local_ptr, int32_t size,
		       uint32_t flags)
{
	struct dma_sg_config config;
	struct dma_sg_elem *host_sg_elem;
	struct dma_sg_elem local_sg_elem;
	int32_t err;
	int32_t offset = host_offset;

	if (size <= 0)
		return 0;

	/* find host element with host_offset */
	host_sg_elem = sg_get_elem_at(host_sg, &offset);
	if (!host_sg_elem)
		return -EINVAL;

	/* set up DMA configuration */
	config.direction = DMA_DIR_LMEM_TO_HMEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;
	config.irq_disabled = false;
	dma_sg_init(&config.elem_array);

	/* configure local DMA elem */
	local_sg_elem.dest = host_sg_elem->dest + offset;
	local_sg_elem.src = (uintptr_t)local_ptr;
	if (size >= HOST_PAGE_SIZE - offset)
		local_sg_elem.size = HOST_PAGE_SIZE - offset;
	else
		local_sg_elem.size = size;

	config.elem_array.elems = &local_sg_elem;
	config.elem_array.count = 1;

	/* start the DMA */
	err = dma_set_config_legacy(dc->chan, &config);
	if (err < 0)
		return err;

	err = dma_copy_legacy(dc->chan, local_sg_elem.size,
			      flags);
	if (err < 0)
		return err;

	/* bytes copied */
	return local_sg_elem.size;
}

int dma_copy_to_host(struct dma_copy *dc, struct dma_sg_config *host_sg,
		     int32_t host_offset, void *local_ptr, int32_t size)
{
	return dma_copy_to_host_flags(dc, host_sg, host_offset, local_ptr, size,
				      DMA_COPY_ONE_SHOT | DMA_COPY_BLOCKING);
}

int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
			    int32_t host_offset, void *local_ptr, int32_t size)
{
	return dma_copy_to_host_flags(dc, host_sg, host_offset, local_ptr, size,
				      DMA_COPY_ONE_SHOT);
}

int dma_copy_new(struct dma_copy *dc)
{
	uint32_t dir, cap, dev;

	/* request HDA DMA in the dir LMEM->HMEM with shared access */
	dir = DMA_DIR_LMEM_TO_HMEM;
	dev = DMA_DEV_HOST;
	cap = 0;
	dc->dmac = dma_get(dir, cap, dev, DMA_ACCESS_SHARED);
	if (!dc->dmac) {
		tr_err(&dmacpy_tr, "dc->dmac = NULL");
		return -ENODEV;
	}

	/* get DMA channel from DMAC0 */
	dc->chan = dma_channel_get_legacy(dc->dmac, CONFIG_TRACE_CHANNEL);
	if (!dc->chan) {
		tr_err(&dmacpy_tr, "dc->chan is NULL");
		return -ENODEV;
	}

	return 0;
}

int dma_copy_set_stream_tag(struct dma_copy *dc, uint32_t stream_tag)
{
	/* get DMA channel from DMAC */
	dc->chan = dma_channel_get_legacy(dc->dmac, stream_tag - 1);
	if (!dc->chan) {
		tr_err(&dmacpy_tr, "dc->chan is NULL");
		return -EINVAL;
	}

	return 0;
}

