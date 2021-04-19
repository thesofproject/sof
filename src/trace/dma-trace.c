// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Yan Wang <yan.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <sof/trace/dma-trace.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <kernel/abi.h>
#include <user/abi_dbg.h>
#include <version.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* 58782c63-1326-4185-8459-22272e12d1f1 */
DECLARE_SOF_UUID("dma-trace", dma_trace_uuid, 0x58782c63, 0x1326, 0x4185,
		 0x84, 0x59, 0x22, 0x27, 0x2e, 0x12, 0xd1, 0xf1);

DECLARE_TR_CTX(dt_tr, SOF_UUID(dma_trace_uuid), LOG_LEVEL_INFO);

/* 2b972272-c5b1-4b7e-926f-0fc5cb4c4690 */
DECLARE_SOF_UUID("dma-trace-task", dma_trace_task_uuid, 0x2b972272, 0xc5b1,
		 0x4b7e, 0x92, 0x6f, 0x0f, 0xc5, 0xcb, 0x4c, 0x46, 0x90);

static int dma_trace_get_avail_data(struct dma_trace_data *d,
				    struct dma_trace_buf *buffer,
				    int avail);

/** Periodically runs and starts the DMA even when the buffer is not
 * full.
 */
static enum task_state trace_work(void *data)
{
	struct dma_trace_data *d = data;
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
	if (size == 0) {
		return SOF_TASK_STATE_RESCHEDULE;
	}

	d->posn.overflow = overflow;

	/* DMA trace copying is working */
	d->copy_in_progress = 1;

	/* copy this section to host */
	size = dma_copy_to_host_nowait(&d->dc, config, d->posn.host_offset,
				       buffer->r_ptr, size);
	if (size < 0) {
		tr_err(&dt_tr, "trace_work(): dma_copy_to_host_nowait() failed");
		goto out;
	}

	/* update host pointer and check for wrap */
	d->posn.host_offset += size;
	if (d->posn.host_offset >= d->host_size)
		d->posn.host_offset -= d->host_size;

	/* update local pointer and check for wrap */
	buffer->r_ptr = (char *)buffer->r_ptr + size;
	if (buffer->r_ptr >= buffer->end_addr)
		buffer->r_ptr = (char *)buffer->r_ptr - DMA_TRACE_LOCAL_SIZE;

out:
	spin_lock_irq(&d->lock, flags);

	/* disregard any old messages and don't resend them if we overflow */
	if (size > 0) {
		if (d->posn.overflow)
			buffer->avail = DMA_TRACE_LOCAL_SIZE - size;
		else
			buffer->avail -= size;
	}

	/* DMA trace copying is done, allow reschedule */
	d->copy_in_progress = 0;

	spin_unlock_irq(&d->lock, flags);

	/* reschedule the trace copying work */
	return SOF_TASK_STATE_RESCHEDULE;
}

int dma_trace_init_early(struct sof *sof)
{
	sof->dmat = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sof->dmat));
	dma_sg_init(&sof->dmat->config.elem_array);
	spinlock_init(&sof->dmat->lock);

	ipc_build_trace_posn(&sof->dmat->posn);
	sof->dmat->msg = ipc_msg_init(sof->dmat->posn.rhdr.hdr.cmd,
				      sizeof(sof->dmat->posn));
	if (!sof->dmat->msg)
		return -ENOMEM;

	return 0;
}

int dma_trace_init_complete(struct dma_trace_data *d)
{
	int ret = 0;

	tr_info(&dt_tr, "dma_trace_init_complete()");

	/* init DMA copy context */
	ret = dma_copy_new(&d->dc);
	if (ret < 0) {
		tr_err(&dt_tr, "dma_trace_init_complete(): dma_copy_new() failed");
		goto out;
	}

	ret = dma_get_attribute(d->dc.dmac, DMA_ATTR_COPY_ALIGNMENT,
				&d->dma_copy_align);

	if (ret < 0) {
		tr_err(&dt_tr, "dma_trace_init_complete(): dma_get_attribute()");

		goto out;
	}

	schedule_task_init_ll(&d->dmat_work, SOF_UUID(dma_trace_task_uuid),
			      SOF_SCHEDULE_LL_TIMER,
			      SOF_TASK_PRI_MED, trace_work, d, 0, 0);

out:

	return ret;
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
	void *buf;
	unsigned int flags;

	/* allocate new buffer */
	buf = rballoc(0, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
		      DMA_TRACE_LOCAL_SIZE);
	if (!buf) {
		tr_err(&dt_tr, "dma_trace_buffer_init(): alloc failed");
		return -ENOMEM;
	}

	bzero(buf, DMA_TRACE_LOCAL_SIZE);
	dcache_writeback_region(buf, DMA_TRACE_LOCAL_SIZE);

	/* initialise the DMA buffer, whole sequence in section */
	spin_lock_irq(&d->lock, flags);

	buffer->addr  = buf;
	buffer->size = DMA_TRACE_LOCAL_SIZE;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = (char *)buffer->addr + buffer->size;
	buffer->avail = 0;

	spin_unlock_irq(&d->lock, flags);

	return 0;
}

static void dma_trace_buffer_free(struct dma_trace_data *d)
{
	struct dma_trace_buf *buffer = &d->dmatb;
	unsigned int flags;

	spin_lock_irq(&d->lock, flags);

	rfree(buffer->addr);
	memset(buffer, 0, sizeof(*buffer));

	spin_unlock_irq(&d->lock, flags);
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

	err = dma_sg_alloc(&config.elem_array, SOF_MEM_ZONE_SYS,
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
	if (d->old_host_offset != d->posn.host_offset) {
		ipc_msg_send(d->msg, &d->posn, false);
		d->old_host_offset = d->posn.host_offset;
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
	if (d->posn.host_offset + avail > d->host_size)
		hsize = d->host_size - d->posn.host_offset;

	/* local buffer wrap ? */
	if ((char *)buffer->r_ptr + avail > (char *)buffer->end_addr)
		lsize = (char *)buffer->end_addr - (char *)buffer->r_ptr;

	/* get smallest size */
	if (hsize < lsize)
		size = hsize;
	else
		size = lsize;

	return size;
}

#endif  /* CONFIG_DMA_GW */

int dma_trace_enable(struct dma_trace_data *d)
{
	int err;

	/* initialize dma trace buffer */
	err = dma_trace_buffer_init(d);
	if (err < 0)
		goto out;

	/*
	 * It should be the very first sent log for easily identification.
	 * Use tr_err to have this initial message also in error logs and assert
	 * traces works well.
	 */
	tr_err(&dt_tr, "FW ABI 0x%x DBG ABI 0x%x tag " SOF_GIT_TAG " src hash 0x%08x (ldc hash " META_QUOTE(SOF_SRC_HASH) ")",
	       SOF_ABI_VERSION, SOF_ABI_DBG_VERSION, SOF_SRC_HASH);

#if CONFIG_DMA_GW
	/*
	 * GW DMA need finish DMA config and start before
	 * host driver trigger start DMA
	 */
	err = dma_trace_start(d);
	if (err < 0)
		goto out;
#endif

	/* flush fw description message */
	trace_flush();

	/* validate DMA context */
	if (!d->dc.dmac || !d->dc.chan) {
		tr_err_atomic(&dt_tr, "dma_trace_enable(): not valid");
		err = -ENODEV;
		goto out;
	}

	d->enabled = 1;
	schedule_task(&d->dmat_work, DMA_TRACE_PERIOD, DMA_TRACE_PERIOD);

out:
	if (err < 0)
		dma_trace_buffer_free(d);

	return err;
}

/** Sends all pending DMA messages to mailbox (for emergencies) */
void dma_trace_flush(void *t)
{
	struct dma_trace_data *trace_data = dma_trace_data_get();
	struct dma_trace_buf *buffer = NULL;
	uint32_t avail;
	int32_t size;
	int32_t wrap_count;
	int ret;

	if (!trace_data || !trace_data->dmatb.addr) {
		return;
	}

	buffer = &trace_data->dmatb;
	avail = buffer->avail;

	/* number of bytes to flush */
	if (avail > DMA_FLUSH_TRACE_SIZE) {
		size = DMA_FLUSH_TRACE_SIZE;
	} else {
		/* check for buffer wrap */
		if (buffer->w_ptr > buffer->r_ptr)
			size = (char *)buffer->w_ptr - (char *)buffer->r_ptr;
		else
			size = (char *)buffer->end_addr -
				(char *)buffer->r_ptr +
				(char *)buffer->w_ptr -
				(char *)buffer->addr;
	}

	size = MIN(size, MAILBOX_TRACE_SIZE);

	/* invalidate trace data */
	dcache_invalidate_region((void *)t, size);

	/* check for buffer wrap */
	if ((char *)buffer->w_ptr - size < (char *)buffer->addr) {
		wrap_count = (char *)buffer->w_ptr - (char *)buffer->addr;
		ret = memcpy_s(t, size - wrap_count,
			       (char *)buffer->end_addr -
			       (size - wrap_count), size - wrap_count);
		assert(!ret);
		ret = memcpy_s((char *)t + (size - wrap_count), wrap_count,
			       buffer->addr, wrap_count);
		assert(!ret);
	} else {
		ret = memcpy_s(t, size, (char *)buffer->w_ptr - size, size);
		assert(!ret);
	}

	/* writeback trace data */
	dcache_writeback_region((void *)t, size);

}

void dma_trace_on(void)
{
	struct dma_trace_data *trace_data = dma_trace_data_get();

	if (trace_data->enabled) {
		return;
	}

	trace_data->enabled = 1;
	schedule_task(&trace_data->dmat_work, DMA_TRACE_PERIOD,
		      DMA_TRACE_PERIOD);

}

void dma_trace_off(void)
{
	struct dma_trace_data *trace_data = dma_trace_data_get();

	if (!trace_data->enabled) {
		return;
	}

	schedule_task_cancel(&trace_data->dmat_work);
	trace_data->enabled = 0;

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
		overflow_margin = (char *)buffer->r_ptr -
			(char *)buffer->w_ptr - 1;
	else
		overflow_margin = margin + (char *)buffer->r_ptr -
			(char *)buffer->addr - 1;

	if (overflow_margin < length)
		overflow = length - overflow_margin;

	return overflow;
}

/** Ring buffer implementation, drops on overflow. */
static void dtrace_add_event(const char *e, uint32_t length)
{
	struct dma_trace_data *trace_data = dma_trace_data_get();
	struct dma_trace_buf *buffer = &trace_data->dmatb;
	uint32_t margin;
	uint32_t overflow;
	int ret;

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
			tr_err(&dt_tr, "dtrace_add_event(): number of dropped logs = %u",
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
			ret = memcpy_s(buffer->w_ptr, length, e, length);
			assert(!ret);
			dcache_writeback_region(buffer->w_ptr, length);
			buffer->w_ptr = (char *)buffer->w_ptr + length;
		} else {
			/* data is bigger than remaining margin so we wrap */
			dcache_invalidate_region(buffer->w_ptr, margin);
			ret = memcpy_s(buffer->w_ptr, margin, e, margin);
			assert(!ret);
			dcache_writeback_region(buffer->w_ptr, margin);
			buffer->w_ptr = buffer->addr;

			dcache_invalidate_region(buffer->w_ptr,
						 length - margin);
			ret = memcpy_s(buffer->w_ptr, length - margin,
				       e + margin, length - margin);
			assert(!ret);
			dcache_writeback_region(buffer->w_ptr,
						length - margin);
			buffer->w_ptr = (char *)buffer->w_ptr + length - margin;
		}

		buffer->avail += length;
		trace_data->posn.messages++;
	} else {
		/* if there is not enough memory for new log, we drop it */
		trace_data->dropped_entries++;
	}

}

/** Main dma-trace entry point */
void dtrace_event(const char *e, uint32_t length)
{
	struct dma_trace_data *trace_data = dma_trace_data_get();
	struct dma_trace_buf *buffer = NULL;
	unsigned long flags;

	if (!trace_data || !trace_data->dmatb.addr ||
	    length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0) {
		return;
	}

	buffer = &trace_data->dmatb;

	spin_lock_irq(&trace_data->lock, flags);
	dtrace_add_event(e, length);

	/* if DMA trace copying is working or secondary core
	 * don't check if local buffer is half full
	 */
	if (trace_data->copy_in_progress ||
	    cpu_get_id() != PLATFORM_PRIMARY_CORE_ID) {
		spin_unlock_irq(&trace_data->lock, flags);
		return;
	}

	spin_unlock_irq(&trace_data->lock, flags);

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
	struct dma_trace_data *trace_data = dma_trace_data_get();

	if (!trace_data || !trace_data->dmatb.addr ||
	    length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0) {
		return;
	}

	dtrace_add_event(e, length);
}
