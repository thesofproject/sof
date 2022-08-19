/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Yan Wang <yan.wang@linux.intel.com>
 */

#ifndef __SOF_TRACE_DMA_TRACE_H__
#define __SOF_TRACE_DMA_TRACE_H__

#include <sof/lib/dma.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <ipc/trace.h>
#include <stdint.h>

struct ipc_msg;
struct sof;

struct dma_trace_buf {
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */
	uint32_t size;		/* size of buffer in bytes */
	uint32_t avail;		/* bytes available to read */
};

struct dma_trace_data {
	struct dma_sg_config config;
	struct dma_trace_buf dmatb;
#if CONFIG_DMA_GW
	struct dma_sg_config gw_config;
#endif
	struct dma_copy dc;
	struct sof_ipc_dma_trace_posn posn;
	struct ipc_msg *msg;
	uint32_t host_size;
	struct task dmat_work;
	uint32_t enabled;
	uint32_t copy_in_progress;
	uint32_t stream_tag;
	uint32_t active_stream_tag;
	uint32_t dma_copy_align;	/* Minimal chunk of data possible to be
					 *  copied by dma connected to host
					 */
	uint32_t dropped_entries;	/* amount of dropped entries */
	struct k_spinlock lock;		/* dma trace lock */
	uint64_t time_delta;		/* difference between the host time */
};

int dma_trace_init_early(struct sof *sof);
int dma_trace_init_complete(struct dma_trace_data *d);
int dma_trace_host_buffer(struct dma_trace_data *d,
			  struct dma_sg_elem_array *elem_array,
			  uint32_t host_size);
int dma_trace_enable(struct dma_trace_data *d);
void dma_trace_disable(struct dma_trace_data *d);
void dma_trace_flush(void *destination);
void dma_trace_on(void);
void dma_trace_off(void);

void dtrace_event(const char *e, uint32_t size);
void dtrace_event_atomic(const char *e, uint32_t length);

static inline bool dma_trace_initialized(const struct dma_trace_data *d)
{
	return d && d->dmatb.addr;
}

static inline struct dma_trace_data *dma_trace_data_get(void)
{
	return sof_get()->dmat;
}

static inline uint32_t dtrace_calc_buf_margin(struct dma_trace_buf *buffer)
{
	return (char *)buffer->end_addr - (char *)buffer->w_ptr;
}

#endif /* __SOF_TRACE_DMA_TRACE_H__ */
