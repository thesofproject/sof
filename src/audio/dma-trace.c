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
#include <reef/audio/dma-trace.h>
#include <reef/ipc.h>
#include <reef/reef.h>
#include <reef/alloc.h>
#include <arch/cache.h>
#include <platform/timer.h>
#include <platform/dma.h>
#include <reef/lock.h>
#include <stdint.h>

static struct dma_trace_data *trace_data = NULL;

static int dma_trace_new_buffer(struct dma_trace_data *d, uint32_t buffer_size)
{
	struct dma_trace_buf *buffer = &d->dmatb;

	trace_buffer("nlb");

	/* validate request */
	if (buffer_size == 0 || buffer_size > HEAP_BUFFER_SIZE) {
		trace_buffer_error("ebg");
		trace_value(buffer_size);
		return -ENOMEM;
	}

	/* allocate new buffer */
	buffer->addr = rballoc(RZONE_RUNTIME, RFLAGS_NONE, buffer_size);
	if (buffer->addr == NULL) {
		trace_buffer_error("ebm");
		return -ENOMEM;
	}

	bzero(buffer->addr, buffer_size);

	buffer->size = buffer_size;
	buffer->w_ptr = buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->size;

	return 0;
}

static void trace_send(struct dma_trace_data *d)
{
	struct dma_trace_buf *buffer = &d->dmatb;
	struct dma_sg_config *config = &d->config;
	uint32_t size = 0;
	int32_t offset = 0;

	if (buffer->w_ptr == buffer->r_ptr)
		return;

	size = buffer->w_ptr - buffer->r_ptr;
	if (d->host_offset + size > d->host_size)
		d->host_offset = 0;

	offset = dma_copy_to_host(config, d->host_offset,
		buffer->r_ptr, size);
	if (offset < 0) {
		trace_buffer_error("ebb");
		return;
	}

	d->host_offset += size;
	buffer->r_ptr += size;
}

static uint64_t trace_work(void *data, uint64_t delay)
{
	struct dma_trace_data *d = (struct dma_trace_data *)data;

	trace_send(d);

	/* reschedule the trace copying work */
	return DMA_TRACE_US;
}

int dma_trace_init(struct dma_trace_data *d)
{
	int err;

	trace_buffer("dtn");

	/* init buffer elems */
	list_init(&d->config.elem_list);

	/* allocate local DMA buffer */
	err = dma_trace_new_buffer(d, DMA_TRACE_LOCAL_SIZE);
	if (err < 0) {
		trace_buffer_error("ePb");
		return err;
	}

	d->host_offset = 0;
	trace_data = d;

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

	*e = *elem;
	d->host_size = host_size;

	list_item_append(&e->list, &d->config.elem_list);
	return 0;
}

void dma_trace_config_ready(struct dma_trace_data *d)
{
	work_schedule_default(&d->dmat_work, DMA_TRACE_US);
	d->ready = 1;
}

void dtrace_event(const char *e, uint32_t length)
{
	struct dma_trace_buf *buffer = NULL;
	int margin = 0;

	if (trace_data == NULL || length < 1)
		return;

	buffer = &trace_data->dmatb;
	if (buffer == NULL)
		return;

	margin = buffer->end_addr - buffer->w_ptr;

	if (margin > length) {
		memcpy(buffer->w_ptr, e, length);
		buffer->w_ptr += length;
	} else {
		memcpy(buffer->w_ptr, e, margin);
		buffer->w_ptr += margin;

		if(trace_data->ready)
			trace_send(trace_data);

		buffer->w_ptr = buffer->r_ptr = buffer->addr;
		bzero(buffer->addr, buffer->size);

		memcpy(buffer->w_ptr, e + margin, length - margin);
		buffer->w_ptr += length -margin;
	}

	length = buffer->w_ptr - buffer->r_ptr;
	if (trace_data->ready && length >= (DMA_TRACE_LOCAL_SIZE / 2))
		trace_send(trace_data);
}
