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

struct dma_info {
	struct dma *dma_array;
	size_t num_dmas;
};

static struct dma_info lib_dma = {
	.dma_array = NULL,
	.num_dmas = 0
};

void dma_install(struct dma *dma_array, size_t num_dmas)
{
	lib_dma.dma_array = dma_array;
	lib_dma.num_dmas = num_dmas;
}

struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	int ch_count, ret;
	int min_ch_count = INT32_MAX;
	struct dma *d = NULL, *dmin = NULL;

	if (!lib_dma.num_dmas) {
		trace_error(TRACE_CLASS_DMA, "No DMAs installed");
		return NULL;
	}
	for (d = lib_dma.dma_array; d < lib_dma.dma_array + lib_dma.num_dmas;
	     d++) {
		/* skip if this DMAC does not support the requested dir */
		if (dir && (d->plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (d->plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (d->plat_data.devs & dev) == 0)
			continue;

		/* if exclusive access is requested */
		if (flags & DMA_ACCESS_EXCLUSIVE) {

			/* ret DMA with no channels draining */
			if (!atomic_read(&d->num_channels_busy))
				return d;
		} else {

			/* get number of channels draining in this DMAC*/
			ch_count = atomic_read(&d->num_channels_busy);

			/* pick DMAC with the least num of channels draining */
			if (ch_count < min_ch_count) {
				dmin = d;
				min_ch_count = ch_count;
			}
		}
	}

	/* return DMAC */
	if (dmin) {
		tracev_event(TRACE_CLASS_DMA, "dma-probe id %d",
			     dmin->plat_data.id);
		/* Shared DMA controllers with multiple channels
		 * may be requested many times, let the probe()
		 * do on-first-use initialization.
		 */
		spin_lock(&dmin->lock);
		ret = 0;
		if (dmin->sref == 0) {
			ret = dma_probe(dmin);
			if (ret < 0) {
				trace_error(TRACE_CLASS_DMA,
					    "dma-probe failed id %d ret %d",
					    dmin->plat_data.id, ret);
			}
		}
		if (!ret)
			dmin->sref++;
		trace_event(TRACE_CLASS_DMA, "dma-get %p sref %d",
			    (uintptr_t)dmin, dmin->sref);
		spin_unlock(&dmin->lock);
		return !ret ? dmin : NULL;
	}

	trace_error(TRACE_CLASS_DMA,
		    "dma-get dir 0x%x cap 0x%x dev 0x%x flags 0x%x not found",
		    dir, cap, dev, flags);
	return NULL;
}

int dma_sg_alloc(struct dma_sg_elem_array *elem_array,
		 int zone,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	int i;

	elem_array->elems = rzalloc(zone, SOF_MEM_CAPS_RAM,
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
