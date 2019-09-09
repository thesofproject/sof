// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/drivers/ipc.h>
#include <sof/lib/dma.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <config.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* tracing */
#define trace_dma(__e)	trace_event(TRACE_CLASS_DMA, __e)
#define trace_dma_error(__e)	trace_error(TRACE_CLASS_DMA, __e)
#define tracev_dma(__e)	tracev_event(TRACE_CLASS_DMA, __e)

#if !CONFIG_DMA_GW
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
	trace_dma_error("sg_get_elem_at() error: "
			"host offset in beyond end of SG buffer");
	return NULL;
}
#endif

#if !CONFIG_DMA_GW

static void dma_complete(void *data, uint32_t type, struct dma_cb_data *next)
{
	completion_t *comp = (completion_t *)data;

	if (type == DMA_CB_TYPE_IRQ)
		wait_completed(comp);

	ipc_dma_trace_send_position();

	next->status = DMA_CB_STATUS_END;
}

#endif

/* Copy DSP memory to host memory.
 * Copies DSP memory to host in a single PAGE_SIZE or smaller block. Does not
 * waits/sleeps and can be used in IRQ context.
 */
#if CONFIG_DMA_GW

int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
			    int32_t host_offset, void *local_ptr, int32_t size)
{
	int ret;

	/* tell gateway to copy */
	ret = dma_copy(dc->chan, size, 0);
	if (ret < 0)
		return ret;

	/* bytes copied */
	return size;
}

#else

int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
			    int32_t host_offset, void *local_ptr, int32_t size)
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
	if (host_sg_elem == NULL)
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
	local_sg_elem.src = (uint32_t)local_ptr;
	if (size >= HOST_PAGE_SIZE - offset)
		local_sg_elem.size = HOST_PAGE_SIZE - offset;
	else
		local_sg_elem.size = size;

	config.elem_array.elems = &local_sg_elem;
	config.elem_array.count = 1;

	/* start the DMA */
	err = dma_set_config(dc->chan, &config);
	if (err < 0)
		return err;

	err = dma_start(dc->chan);
	if (err < 0)
		return err;

	/* bytes copied */
	return local_sg_elem.size;
}

#endif

int dma_copy_new(struct dma_copy *dc)
{
	uint32_t dir, cap, dev;

	/* request HDA DMA in the dir LMEM->HMEM with shared access */
	dir = DMA_DIR_LMEM_TO_HMEM;
	dev = DMA_DEV_HOST;
	cap = 0;
	dc->dmac = dma_get(dir, cap, dev, DMA_ACCESS_SHARED);
	if (dc->dmac == NULL) {
		trace_dma_error("dma_copy_new() error: dc->dmac = NULL");
		return -ENODEV;
	}

#if !CONFIG_DMA_GW
	/* get DMA channel from DMAC0 */
	dc->chan = dma_channel_get(dc->dmac, 0);
	if (!dc->chan) {
		trace_dma_error("dma_copy_new() error: dc->chan is NULL");
		return -ENODEV;
	}
	trace_dma("dma_copy_new() got DMA channel");

	dc->complete.timeout = 100;	/* wait 100 usecs for DMA to finish */
	dma_set_cb(dc->chan, DMA_CB_TYPE_IRQ, dma_complete, &dc->complete);
#endif

	return 0;
}

#if CONFIG_DMA_GW

int dma_copy_set_stream_tag(struct dma_copy *dc, uint32_t stream_tag)
{
	/* get DMA channel from DMAC */
	dc->chan = dma_channel_get(dc->dmac, stream_tag - 1);
	if (!dc->chan) {
		trace_dma_error("dma_copy_set_stream_tag() error: "
				"dc->chan is NULL");
		return -EINVAL;
	}

	return 0;
}

#endif
