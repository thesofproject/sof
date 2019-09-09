// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Yan Wang <yan.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <sof/trace/dma-trace.h>
#include <ipc/topology.h>
#include <config.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static struct dma_trace_data *trace_data;

static int dma_trace_get_avail_data(struct dma_trace_data *d,
				    struct dma_trace_buf *buffer,
				    int avail);

static enum task_state trace_work(void *data)
{
	struct dma_trace_data *d = (struct dma_trace_data *)data;
	struct dma_trace_buf *buffer = &d->dmatb;
	struct dma_sg_config *config = &d->config;
	unsigned long flags;
	uint32_t avail = buffer->avail;
	int32_t size;
	uint32_t overflow;

	/* make sure we don't write more than buffer */
	if (avail > DMA_TRACE_LOCAL_SIZE) {
		overflow = avail - DMA_TRACE_LOCAL_SIZE;
		avail = DMA_TRACE_LOCAL_SIZE;
	} else {
		overflow = 0;
	}

	/* dma gateway supports wrap mode copy, but GPDMA doesn't
	 * support, so do it differently based on HW features
	 */
	size = dma_trace_get_avail_data(d, buffer, avail);

	/* any data to copy ? */
	if (size == 0)
		return SOF_TASK_STATE_RESCHEDULE;

	d->overflow = overflow;

	/* DMA trace copying is working */
	d->copy_in_progress = 1;

	/* copy this section to host */
	size = dma_copy_to_host_nowait(&d->dc, config, d->host_offset,
		buffer->r_ptr, size);
	if (size < 0) {
		trace_buffer_error("trace_work() error: "
				   "dma_copy_to_host_nowait() failed");
		goto out;
	}

	/* update host pointer and check for wrap */
	d->host_offset += size;
	if (d->host_offset >= d->host_size)
		d->host_offset -= d->host_size;

	/* update local pointer and check for wrap */
	buffer->r_ptr += size;
	if (buffer->r_ptr >= buffer->end_addr)
		buffer->r_ptr -= DMA_TRACE_LOCAL_SIZE;

out:
	spin_lock_irq(d->lock, flags);

	/* disregard any old messages and don't resend them if we overflow */
	if (size > 0) {
		if (d->overflow)
			buffer->avail = DMA_TRACE_LOCAL_SIZE - size;
		else
			buffer->avail -= size;
	}

	/* DMA trace copying is done, allow reschedule */
	d->copy_in_progress = 0;

	spin_unlock_irq(d->lock, flags);

	/* reschedule the trace copying work */
	return SOF_TASK_STATE_RESCHEDULE;
}

int __dsp_printf(char *format, ...);

int dma_trace_init_early(struct sof *sof)
{
	trace_data = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
			     sizeof(*trace_data));

	dma_sg_init(&trace_data->config.elem_array);
	spinlock_init(&trace_data->lock);
	sof->dmat = trace_data;

	return 0;
}

int dma_trace_init_complete(struct dma_trace_data *d)
{
	int ret;

	trace_buffer("dma_trace_init_complete()");

	/* init DMA copy context */
	ret = dma_copy_new(&d->dc);
	if (ret < 0) {
		trace_buffer_error("dma_trace_init_complete() error: "
				   "dma_copy_new() failed");
		return ret;
	}
	trace_buffer("dma_trace_init_complete() got DMA copy context");

	ret = dma_get_attribute(d->dc.dmac, DMA_ATTR_COPY_ALIGNMENT,
				&d->dma_copy_align);

	if (ret < 0) {
		trace_buffer("dma_trace_init_complete() "
			     "error: dma_get_attribute()");

		return ret;
	}

	trace_buffer("dma_trace_init_complete() got DMA attribute");

	schedule_task_init(&d->dmat_work, SOF_SCHEDULE_LL,
			   SOF_TASK_PRI_MED, trace_work, d, 0, 0);

	trace_buffer("dma_trace_init_complete() -> Task initialized");

	return 0;
}

#if (CONFIG_HOST_PTABLE)
int dma_trace_host_buffer(struct dma_trace_data *d,
			  struct dma_sg_elem_array *elem_array,
			  uint32_t host_size)
{
	d->host_size = host_size;
	d->config.elem_array = *elem_array;

	return 0;
}
#endif

static int dma_trace_buffer_init(struct dma_trace_data *d)
{
	struct dma_trace_buf *buffer = &d->dmatb;

	/* allocate new buffer */
	buffer->addr = rballoc(RZONE_BUFFER,
			       SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
			       DMA_TRACE_LOCAL_SIZE);
	if (!buffer->addr) {
		trace_buffer_error("dma_trace_buffer_init() error: "
				   "alloc failed");
		return -ENOMEM;
	}

	bzero(buffer->addr, DMA_TRACE_LOCAL_SIZE);
	dcache_writeback_region(buffer->addr, DMA_TRACE_LOCAL_SIZE);

	/* initialise the DMA buffer */
	buffer->size = DMA_TRACE_LOCAL_SIZE;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->size;
	buffer->avail = 0;

	return 0;
}

#if CONFIG_DMA_GW

static int dma_trace_start(struct dma_trace_data *d)
{
	struct dma_sg_config config;
	uint32_t elem_size, elem_addr, elem_num;
	int err = 0;

	err = dma_copy_set_stream_tag(&d->dc, d->stream_tag);
	if (err < 0)
		return err;

	/* size of every trace record */
	elem_size = sizeof(uint64_t) * 2;

	/* Initialize address of local elem */
	elem_addr = (uint32_t)d->dmatb.addr;

	/* the number of elem list */
	elem_num = DMA_TRACE_LOCAL_SIZE / elem_size;

	config.direction = DMA_DIR_LMEM_TO_HMEM;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;

	err = dma_sg_alloc(&config.elem_array, RZONE_SYS,
			   config.direction,
			   elem_num, elem_size, elem_addr, 0);
	if (err < 0)
		return err;

	err = dma_set_config(d->dc.chan, &config);
	if (err < 0)
		return err;

	err = dma_start(d->dc.chan);

	return err;
}

static int dma_trace_get_avail_data(struct dma_trace_data *d,
				    struct dma_trace_buf *buffer,
				    int avail)
{
	/* there isn't DMA completion callback in GW DMA copying.
	 * so we send previous position always before the next copying
	 * for guaranteeing previous DMA copying is finished.
	 * This function will be called once every 500ms at least even
	 * if no new trace is filled.
	 */
	if (d->old_host_offset != d->host_offset) {
		ipc_dma_trace_send_position();
		d->old_host_offset = d->host_offset;
	}

	/* align data to HD-DMA burst size */
	return ALIGN_DOWN(avail, d->dma_copy_align);
}
#else
static int dma_trace_get_avail_data(struct dma_trace_data *d,
				    struct dma_trace_buf *buffer,
				    int avail)
{
	uint32_t hsize;
	uint32_t lsize;
	int32_t size;

	/* copy to host in sections if we wrap */
	lsize = avail;
	hsize = avail;

	if (avail == 0)
		return 0;

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

	return size;
}
#endif

int dma_trace_enable(struct dma_trace_data *d)
{
	int err;

	/* initialize dma trace buffer */
	err = dma_trace_buffer_init(d);
	trace_buffer("dma_trace_buffer_init returned %d", err);
	if (err < 0)
		return err;

#if CONFIG_DMA_GW
	/*
	 * GW DMA need finish DMA config and start before
	 * host driver trigger start DMA
	 */
	err = dma_trace_start(d);
	if (err < 0)
		return err;
#endif

	/* validate DMA context */
	if (!d->dc.dmac || !d->dc.chan) {
		trace_buffer_error_atomic("dma_trace_enable() error: not "
					  "valid");
		return -ENODEV;
	}

	d->enabled = 1;
	trace_buffer("About to schedule task for dma trace");
	schedule_task(&d->dmat_work, DMA_TRACE_PERIOD, DMA_TRACE_PERIOD);
	trace_buffer("Scheduled the task for dma trace");

	return 0;
}

void dma_trace_flush(void *t)
{
	struct dma_trace_buf *buffer = NULL;
	uint32_t avail;
	int32_t size;
	int32_t wrap_count;

	if (!trace_data || !trace_data->dmatb.addr)
		return;

	buffer = &trace_data->dmatb;
	avail = buffer->avail;

	/* number of bytes to flush */
	if (avail > DMA_FLUSH_TRACE_SIZE) {
		size = DMA_FLUSH_TRACE_SIZE;
	} else {
		/* check for buffer wrap */
		if (buffer->w_ptr > buffer->r_ptr)
			size = buffer->w_ptr - buffer->r_ptr;
		else
			size = buffer->end_addr - buffer->r_ptr +
				buffer->w_ptr - buffer->addr;
	}

	/* invalidate trace data */
	dcache_invalidate_region((void *)t, size);

	/* check for buffer wrap */
	if (buffer->w_ptr - size < buffer->addr) {
		wrap_count = buffer->w_ptr - buffer->addr;
		assert(!memcpy_s(t, size - wrap_count, buffer->end_addr -
				 (size - wrap_count), size - wrap_count));
		assert(!memcpy_s(t + (size - wrap_count), wrap_count,
				 buffer->addr, wrap_count));
	} else {
		assert(!memcpy_s(t, size, buffer->w_ptr - size, size));
	}

	/* writeback trace data */
	dcache_writeback_region((void *)t, size);
}

static int dtrace_calc_buf_overflow(struct dma_trace_buf *buffer,
				    uint32_t length)
{
	uint32_t margin;
	uint32_t overflow_margin;
	uint32_t overflow = 0;

	margin = dtrace_calc_buf_margin(buffer);

	/* overflow calculating */
	if (buffer->w_ptr < buffer->r_ptr)
		overflow_margin = buffer->r_ptr - buffer->w_ptr - 1;
	else
		overflow_margin = margin + buffer->r_ptr - buffer->addr - 1;

	if (overflow_margin < length)
		overflow = length - overflow_margin;

	return overflow;
}

static void dtrace_add_event(const char *e, uint32_t length)
{
	struct dma_trace_buf *buffer = &trace_data->dmatb;
	uint32_t margin;
	uint32_t overflow = 0;

	margin = dtrace_calc_buf_margin(buffer);
	overflow = dtrace_calc_buf_overflow(buffer, length);

	/* tracing dropped entries */
	if (trace_data->dropped_entries) {
		if (!overflow) {
			/*
			 * if any dropped entries have appeared and there
			 * is not any overflow, their amount will be logged
			 */
			uint32_t tmp_dropped_entries =
				trace_data->dropped_entries;
			trace_data->dropped_entries = 0;
			/*
			 * this trace_error invocation causes recursion,
			 * so after it we have to recalculate margin and
			 * overflow
			 */
			trace_error(0, "dtrace_add_event() error: "
				    "number of dropped logs = %u",
				    tmp_dropped_entries);
			margin = dtrace_calc_buf_margin(buffer);
			overflow = dtrace_calc_buf_overflow(buffer, length);
		}
	}

	/* checking overflow */
	if (!overflow) {
		/* check for buffer wrap */
		if (margin > length) {
			/* no wrap */
			dcache_invalidate_region(buffer->w_ptr, length);
			assert(!memcpy_s(buffer->w_ptr, length, e, length));
			dcache_writeback_region(buffer->w_ptr, length);
			buffer->w_ptr += length;
		} else {
			/* data is bigger than remaining margin so we wrap */
			dcache_invalidate_region(buffer->w_ptr, margin);
			assert(!memcpy_s(buffer->w_ptr, margin, e, margin));
			dcache_writeback_region(buffer->w_ptr, margin);
			buffer->w_ptr = buffer->addr;

			dcache_invalidate_region(buffer->w_ptr,
						 length - margin);
			assert(!memcpy_s(buffer->w_ptr, length - margin,
					 e + margin, length - margin));
			dcache_writeback_region(buffer->w_ptr,
						length - margin);
			buffer->w_ptr += length - margin;
		}

		buffer->avail += length;
		trace_data->messages++;
	} else {
		/* if there is not enough memory for new log, we drop it */
		trace_data->dropped_entries++;
	}
}

void dtrace_event(const char *e, uint32_t length)
{
	struct dma_trace_buf *buffer = NULL;
	unsigned long flags;

	if (!trace_data || !trace_data->dmatb.addr ||
	    length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0)
		return;

	buffer = &trace_data->dmatb;

	spin_lock_irq(trace_data->lock, flags);
	dtrace_add_event(e, length);

	/* if DMA trace copying is working or slave core
	 * don't check if local buffer is half full
	 */
	if (trace_data->copy_in_progress ||
	    cpu_get_id() != PLATFORM_MASTER_CORE_ID) {
		spin_unlock_irq(trace_data->lock, flags);
		return;
	}

	spin_unlock_irq(trace_data->lock, flags);

	/* schedule copy now if buffer > 50% full */
	if (trace_data->enabled &&
	    buffer->avail >= (DMA_TRACE_LOCAL_SIZE / 2)) {
		reschedule_task(&trace_data->dmat_work,
				DMA_TRACE_RESCHEDULE_TIME);
		/* reschedule should not be interrupted
		 * just like we are in copy progress
		 */
		trace_data->copy_in_progress = 1;
	}
}

void dtrace_event_atomic(const char *e, uint32_t length)
{
	if (!trace_data || !trace_data->dmatb.addr ||
	    length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0)
		return;

	dtrace_add_event(e, length);
}
