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

#ifndef __INCLUDE_DMA_TRACE__
#define __INCLUDE_DMA_TRACE__

#include <stdint.h>
#include <stdlib.h>
#include <sof/sof.h>
#include <sof/list.h>
#include <sof/mailbox.h>
#include <sof/debug.h>
#include <sof/timer.h>
#include <sof/dma.h>
#include <sof/work.h>
#include <platform/platform.h>
#include <platform/timer.h>

struct dma_trace_buf {
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */
	uint32_t size;		/* size of buffer in bytes */
	uint32_t avail;		/* avail bytes in buffer */
};

struct dma_trace_data {
	struct dma_sg_config config;
	struct dma_trace_buf dmatb;
	struct dma_copy dc;
	uint32_t old_host_offset;
	uint32_t host_offset;
	uint32_t overflow;
	uint32_t messages;
	uint32_t host_size;
	struct work dmat_work;
	uint32_t enabled;
	uint32_t copy_in_progress;
	uint32_t stream_tag;
	spinlock_t lock;
};

int dma_trace_init_early(struct sof *sof);
int dma_trace_init_complete(struct dma_trace_data *d);
int dma_trace_host_buffer(struct dma_trace_data *d,
			  struct dma_sg_elem_array *elem_array,
			  uint32_t host_size);
int dma_trace_enable(struct dma_trace_data *d);
void dma_trace_flush(void *t);

void dtrace_event(const char *e, uint32_t size);
void dtrace_event_atomic(const char *e, uint32_t length);

#endif
