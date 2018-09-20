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
 * Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <sof/dma.h>
#include <sof/atomic.h>
#include <sof/alloc.h>
#include <platform/dma.h>

/*
 * API to request a platform DMAC.
 * Users can request DMAC based on dev type, copy direction, capabilities
 * and access privilege.
 * For exclusive access, ret DMAC with no channels draining.
 * For shared access, ret DMAC with the least number of channels draining.
 */

struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	int i, ch_count;
	int min_ch_count = INT32_MAX;
	int dma_index = -1;

	for (i = 0; i < PLATFORM_NUM_DMACS; i++) {

		/* skip if this DMAC does not support the requested dir */
		if (dir && (dma[i].plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (dma[i].plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (dma[i].plat_data.devs & dev) == 0)
			continue;

		/* if exclusive access is requested */
		if (flags & DMA_ACCESS_EXCLUSIVE) {

			/* ret DMA with no channels draining */
			if (!atomic_read(&dma[i].num_channels_busy))
				return &dma[i];
		} else {

			/* get number of channels draining in this DMAC*/
			ch_count = atomic_read(&dma[i].num_channels_busy);

			/* pick DMAC with the least num of channels draining */
			if (ch_count < min_ch_count) {
				dma_index = i;
				min_ch_count = ch_count;
			}
		}
	}

	/* return DMAC */
	if (dma_index >= 0) {
		tracev_value(dma[dma_index].plat_data.id);
		return &dma[dma_index];
	}

	return NULL;
}

int dma_sg_alloc(struct dma_sg_elem_array *elem_array, uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	int i;

	elem_array->elems = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
				    sizeof(struct dma_sg_elem) * buffer_count);
	if (!elem_array->elems)
		return -ENOMEM;

	for (i = 0; i < buffer_count; i++) {
		elem_array->elems[i].size = buffer_bytes;
		// TODO: may count offsets once
		switch (direction) {
		case DMA_DIR_MEM_TO_DEV:
		case DMA_DIR_LMEM_TO_HMEM:
			elem_array->elems[i].src = dma_buffer_addr;
			elem_array->elems[i].dest = external_addr;
			break;
		default:
			elem_array->elems[i].src = external_addr;
			elem_array->elems[i].dest = dma_buffer_addr;
			break;
		}

		dma_buffer_addr += buffer_bytes;
	}
	elem_array->count = buffer_count;
	return 0;
}

void dma_sg_free(struct dma_sg_elem_array *elem_array)
{
	rfree(elem_array->elems);
	dma_sg_init(elem_array);
}
