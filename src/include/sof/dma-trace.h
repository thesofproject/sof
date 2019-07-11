/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Yan Wang <yan.wang@linux.intel.com>
 */

#ifndef __SOF_DMA_TRACE_H__
#define __SOF_DMA_TRACE_H__

#include <stdint.h>
#include <stdlib.h>
#include <sof/sof.h>
#include <sof/list.h>
#include <sof/mailbox.h>
#include <sof/debug.h>
#include <sof/drivers/timer.h>
#include <sof/dma.h>
#include <sof/schedule/schedule.h>
#include <sof/platform.h>

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
	struct task dmat_work;
	uint32_t enabled;
	uint32_t copy_in_progress;
	uint32_t stream_tag;
	uint32_t dma_copy_align; /**< Minimal chunk of data possible to be
				   *  copied by dma connected to host
				   */
	uint32_t dropped_entries; /* amount of dropped entries */
	spinlock_t lock; /* dma trace lock */
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

static inline uint32_t dtrace_calc_buf_margin(struct dma_trace_buf *buffer)
{
	return buffer->end_addr - buffer->w_ptr;
}

#endif /* __SOF_DMA_TRACE_H__ */
