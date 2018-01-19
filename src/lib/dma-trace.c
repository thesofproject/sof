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
 * Author: Yan Wang <yan.wang@linux.intel.com>
 */

#include <reef/trace.h>
#include <reef/dma-trace.h>
#include <reef/ipc.h>
#include <reef/reef.h>
#include <reef/alloc.h>
#include <arch/cache.h>
#include <platform/timer.h>
#include <platform/dma.h>
#include <reef/lock.h>
#include <stdint.h>

static struct dma_trace_data *trace_data = NULL;

static uint64_t trace_work(void *data, uint64_t delay)
{
	struct dma_trace_data *d = (struct dma_trace_data *)data;
	struct dma_trace_buf *buffer = &d->dmatb;
	struct dma_sg_config *config = &d->config;
	unsigned long flags;
	uint32_t avail = buffer->avail;
	int32_t size;
	uint32_t hsize;
	uint32_t lsize;

	/* any data to copy ? */
	if (avail == 0)
		return DMA_TRACE_PERIOD;

	/* DMA trace copying is working */
	d->copy_in_progress = 1;

	/* make sure we dont write more than buffer */
	if (avail > DMA_TRACE_LOCAL_SIZE) {
		d->overflow = avail - DMA_TRACE_LOCAL_SIZE;
		avail = DMA_TRACE_LOCAL_SIZE;
	} else {
		d->overflow = 0;
	}

	/* copy to host in sections if we wrap */
	lsize = hsize = avail;

	/* host buffer wrap ? */
	if (d->host_offset + avail > d->host_size)
		hsize = d->host_size - d->host_offset;

	/* local buffer wrap ? */
	if (buffer->r_ptr + avail > buffer->end_addr)
		lsize = buffer->end_addr - buffer->r_ptr;

	/* get smallest size */
	if (hsize < lsize)
		size = hsize;
	else
		size = lsize;

	/* writeback trace data */
	dcache_writeback_region((void*)buffer->r_ptr, size);

	/* copy this section to host */
	size = dma_copy_to_host_nowait(&d->dc, config, d->host_offset,
		buffer->r_ptr, size);
	if (size < 0) {
		trace_buffer_error("ebb");
		goto out;
	}

	/* update host pointer and check for wrap */
	d->host_offset += size;
	if (d->host_offset == d->host_size)
		d->host_offset = 0;

	/* update local pointer and check for wrap */
	buffer->r_ptr += size;
	if (buffer->r_ptr == buffer->end_addr)
		buffer->r_ptr = buffer->addr;

out:
	spin_lock_irq(&d->lock, flags);

	/* disregard any old messages and dont resend them if we overflow */
	if (d->overflow) {
		buffer->avail = DMA_TRACE_LOCAL_SIZE - size;
	} else {
		buffer->avail -= size;
	}

	/* DMA trace copying is done */
	d->copy_in_progress = 0;

	spin_unlock_irq(&d->lock, flags);

	/* reschedule the trace copying work */
	return DMA_TRACE_PERIOD;
}

int dma_trace_init_early(struct dma_trace_data *d)
{
	struct dma_trace_buf *buffer = &d->dmatb;

	/* allocate new buffer */
	buffer->addr = rballoc(RZONE_RUNTIME, RFLAGS_NONE, DMA_TRACE_LOCAL_SIZE);
	if (buffer->addr == NULL) {
		trace_buffer_error("ebm");
		return -ENOMEM;
	}

	bzero(buffer->addr, DMA_TRACE_LOCAL_SIZE);

	/* initialise the DMA buffer */
	buffer->size = DMA_TRACE_LOCAL_SIZE;
	buffer->w_ptr = buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->size;
	buffer->avail = 0;
	d->host_offset = 0;
	d->overflow = 0;
	d->messages = 0;
	d->enabled = 0;
	d->copy_in_progress = 0;

	list_init(&d->config.elem_list);
	spinlock_init(&d->lock);
	trace_data = d;

	return 0;
}

int dma_trace_init_complete(struct dma_trace_data *d)
{
	struct dma_trace_buf *buffer = &d->dmatb;
	int ret;

	trace_buffer("dtn");

	/* init DMA copy context */
	ret = dma_copy_new(&d->dc, PLATFORM_TRACE_DMAC);
	if (ret < 0) {
		trace_buffer_error("edm");
		rfree(buffer->addr);
		return ret;
	}

	work_init(&d->dmat_work, trace_work, d, WORK_ASYNC);

	return 0;
}

int dma_trace_host_buffer(struct dma_trace_data *d, struct dma_sg_elem *elem,
		uint32_t host_size)
{
	struct dma_sg_elem *e;

	/* allocate new host DMA elem and add it to our list */
	e = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*e));
	if (e == NULL)
		return -ENOMEM;

	/* copy fields - excluding possibly non-initialized elem->src */
	e->dest = elem->dest;
	e->size = elem->size;

	d->host_size = host_size;

	list_item_append(&e->list, &d->config.elem_list);
	return 0;
}

int dma_trace_enable(struct dma_trace_data *d)
{
	/* validate DMA context */
	if (d->dc.dmac == NULL || d->dc.chan < 0) {
		trace_error_atomic(TRACE_CLASS_BUFFER, "eem");
		return -ENODEV;
	}

	d->enabled = 1;
	work_schedule_default(&d->dmat_work, DMA_TRACE_PERIOD);
	return 0;
}

static void dtrace_add_event(const char *e, uint32_t length)
{
	struct dma_trace_buf *buffer = &trace_data->dmatb;
	int margin;

	margin = buffer->end_addr - buffer->w_ptr;

	/* check for buffer wrap */
	if (margin > length) {
		/* no wrap */
		memcpy(buffer->w_ptr, e, length);
		buffer->w_ptr += length;
	} else {

		/* data is bigger than remaining margin so we wrap */
		memcpy(buffer->w_ptr, e, margin);
		buffer->w_ptr = buffer->addr;

		memcpy(buffer->w_ptr, e + margin, length - margin);
		buffer->w_ptr += length - margin;
	}

	buffer->avail += length;
	trace_data->messages++;
}

void dtrace_event(const char *e, uint32_t length)
{
	struct dma_trace_buf *buffer = NULL;
	unsigned long flags;

	if (trace_data == NULL ||
		length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0)
		return;

	buffer = &trace_data->dmatb;

	spin_lock_irq(&trace_data->lock, flags);
	dtrace_add_event(e, length);

	/* if DMA trace copying is working */
	/* don't check if local buffer is half full */
	if (trace_data->copy_in_progress) {
		spin_unlock_irq(&trace_data->lock, flags);
		return;
	}

	spin_unlock_irq(&trace_data->lock, flags);

	/* schedule copy now if buffer > 50% full */
	if (trace_data->enabled && buffer->avail >= (DMA_TRACE_LOCAL_SIZE / 2))
		work_reschedule_default(&trace_data->dmat_work,
		DMA_TRACE_RESCHEDULE_TIME);
}

void dtrace_event_atomic(const char *e, uint32_t length)
{
	if (trace_data == NULL ||
		length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0)
		return;

	dtrace_add_event(e, length);
}
