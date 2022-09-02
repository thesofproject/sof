// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Yan Wang <yan.wang@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <sof/trace/dma-trace.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <kernel/abi.h>
#include <user/abi_dbg.h>
#include <sof_versions.h>

#ifdef __ZEPHYR__
#include <version.h>
#endif

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(dma_trace, CONFIG_SOF_LOG_LEVEL);

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
	k_spinlock_key_t key;
	uint32_t avail = buffer->avail;
	int32_t size;
	uint32_t overflow;

	/* The host DMA channel is not available */
	if (!d->dc.chan)
		return SOF_TASK_STATE_RESCHEDULE;

	if (!ipc_trigger_trace_xfer(avail))
		return SOF_TASK_STATE_RESCHEDULE;

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
	size = dma_copy_to_host(&d->dc, config, d->posn.host_offset,
				buffer->r_ptr, size);
	if (size < 0) {
		tr_err(&dt_tr, "trace_work(): dma_copy_to_host() failed");
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

	ipc_msg_send(d->msg, &d->posn, false);

out:
	key = k_spin_lock(&d->lock);

	/* disregard any old messages and don't resend them if we overflow */
	if (size > 0) {
		if (d->posn.overflow)
			buffer->avail = DMA_TRACE_LOCAL_SIZE - size;
		else
			buffer->avail -= size;
	}

	/* DMA trace copying is done, allow reschedule */
	d->copy_in_progress = 0;

	k_spin_unlock(&d->lock, key);

	/* reschedule the trace copying work */
	return SOF_TASK_STATE_RESCHEDULE;
}

/** Do this early so we can log at initialization time even before the
 * DMA runs. The rest happens later in dma_trace_init_complete() and
 * dma_trace_enable()
 */
int dma_trace_init_early(struct sof *sof)
{
	int ret;

	/* If this assert is wrong then traces have been corrupting
	 * random parts of memory. Some functions run before _and_ after
	 * DMA trace initialization and we don't want to ask them to
	 * never trace. So dma_trace_initialized() must be either
	 * clearly false/NULL or clearly true, we can't tolerate random
	 * uninitialized values in sof->dmat etc.
	 */
	assert(!dma_trace_initialized(sof->dmat));

	sof->dmat = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sof->dmat));

	dma_sg_init(&sof->dmat->config.elem_array);
	k_spinlock_init(&sof->dmat->lock);

	ipc_build_trace_posn(&sof->dmat->posn);
	sof->dmat->msg = ipc_msg_init(sof->dmat->posn.rhdr.hdr.cmd,
				      sof->dmat->posn.rhdr.hdr.size);
	if (!sof->dmat->msg) {
		ret = -ENOMEM;
		goto err;
	}

	return 0;

err:
	mtrace_printf(LOG_LEVEL_ERROR,
		      "dma_trace_init_early() failed: %d", ret);

	/* Cannot rfree(sof->dmat) from the system memory pool, see
	 * comments in lib/alloc.c
	 */
	sof->dmat = NULL;

	return ret;
}

/** Run after dma_trace_init_early() and before dma_trace_enable() */
int dma_trace_init_complete(struct dma_trace_data *d)
{
	int ret = 0;

	tr_info(&dt_tr, "dma_trace_init_complete()");

	if (!d) {
		mtrace_printf(LOG_LEVEL_ERROR,
			      "dma_trace_init_complete(): failed, no dma_trace_data");
		return -ENOMEM;
	}

	/* init DMA copy context */
	ret = dma_copy_new(&d->dc);
	if (ret < 0) {
		mtrace_printf(LOG_LEVEL_ERROR,
			      "dma_trace_init_complete(): dma_copy_new() failed: %d", ret);
		goto out;
	}

	ret = dma_get_attribute(d->dc.dmac, DMA_ATTR_COPY_ALIGNMENT,
				&d->dma_copy_align);

	if (ret < 0) {
		mtrace_printf(LOG_LEVEL_ERROR,
			      "dma_trace_init_complete(): dma_get_attribute() failed: %d", ret);

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

static void dma_trace_buffer_free(struct dma_trace_data *d)
{
	struct dma_trace_buf *buffer = &d->dmatb;
	k_spinlock_key_t key;

	key = k_spin_lock(&d->lock);

	rfree(buffer->addr);
	memset(buffer, 0, sizeof(*buffer));

	k_spin_unlock(&d->lock, key);
}

static int dma_trace_buffer_init(struct dma_trace_data *d)
{
#if CONFIG_DMA_GW
	struct dma_sg_config *config = &d->gw_config;
	uint32_t elem_size, elem_addr, elem_num;
	int ret;
#endif
	struct dma_trace_buf *buffer = &d->dmatb;
	void *buf;
	k_spinlock_key_t key;
	uint32_t addr_align;
	int err;

	/*
	 * Keep the existing dtrace buffer to avoid memory leak, unlikely to
	 * happen if host correctly using the dma_trace_disable().
	 *
	 * The buffer can not be freed up here as it is likely in use.
	 * The (re-)initialization will happen in dma_trace_start() when it is
	 * safe to do (the DMA is stopped)
	 */
	if (dma_trace_initialized(d))
		return 0;

	if (!d || !d->dc.dmac) {
		mtrace_printf(LOG_LEVEL_ERROR,
			      "dma_trace_buffer_init() failed, no DMAC! d=%p", d);
		return -ENODEV;
	}

	err = dma_get_attribute(d->dc.dmac, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0)
		return err;

	/* For DMA to work properly the buffer must be correctly aligned */
	buf = rballoc_align(0, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
			    DMA_TRACE_LOCAL_SIZE, addr_align);
	if (!buf) {
		mtrace_printf(LOG_LEVEL_ERROR, "dma_trace_buffer_init(): alloc failed");
		return -ENOMEM;
	}

	bzero(buf, DMA_TRACE_LOCAL_SIZE);
	dcache_writeback_region((__sparse_force void __sparse_cache *)buf, DMA_TRACE_LOCAL_SIZE);

	/* initialise the DMA buffer, whole sequence in section */
	key = k_spin_lock(&d->lock);

	buffer->addr  = buf;
	buffer->size = DMA_TRACE_LOCAL_SIZE;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = (char *)buffer->addr + buffer->size;
	buffer->avail = 0;

	k_spin_unlock(&d->lock, key);

#if CONFIG_DMA_GW
	/* size of every trace record */
	elem_size = sizeof(uint64_t) * 2;

	/* Initialize address of local elem */
	elem_addr = (uint32_t)buffer->addr;

	/* the number of elem list */
	elem_num = DMA_TRACE_LOCAL_SIZE / elem_size;

	config->direction = DMA_DIR_LMEM_TO_HMEM;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 0;

	ret = dma_sg_alloc(&config->elem_array, SOF_MEM_ZONE_SYS,
			   config->direction, elem_num, elem_size,
			   elem_addr, 0);
	if (ret < 0) {
		dma_trace_buffer_free(d);
		return ret;
	}
#endif

#ifdef __ZEPHYR__
#define ZEPHYR_VER_OPT " zephyr:" META_QUOTE(BUILD_VERSION)
#else
#define ZEPHYR_VER_OPT
#endif

	/* META_QUOTE(SOF_SRC_HASH) is part of the format string so it
	 * goes to the .ldc file and does not go to the firmware
	 * binary. It will be different from SOF_SRC_HASH in case of
	 * mismatch.
	 */
#define SOF_BANNER_COMMON  \
	"FW ABI 0x%x DBG ABI 0x%x tags SOF:" SOF_GIT_TAG  ZEPHYR_VER_OPT  \
	" src hash 0x%08x (ldc hash " META_QUOTE(SOF_SRC_HASH) ")"

	/* It should be the very first sent log for easy identification. */
	mtrace_printf(LOG_LEVEL_INFO,
		      "SHM: " SOF_BANNER_COMMON,
		      SOF_ABI_VERSION, SOF_ABI_DBG_VERSION, SOF_SRC_HASH);

	/* Use a different, DMA: prefix to ease identification of log files */
	tr_info(&dt_tr,
		"DMA: " SOF_BANNER_COMMON,
		SOF_ABI_VERSION, SOF_ABI_DBG_VERSION, SOF_SRC_HASH);

	return 0;
}

#if CONFIG_DMA_GW

static int dma_trace_start(struct dma_trace_data *d)
{
	int err = 0;

	/* DMA Controller initialization is platform-specific */
	if (!d || !d->dc.dmac) {
		mtrace_printf(LOG_LEVEL_ERROR,
			      "dma_trace_start failed: no DMAC!");
		return -ENODEV;
	}

	if (d->dc.chan) {
		/* We already have DMA channel for dtrace, stop it */
		mtrace_printf(LOG_LEVEL_WARNING,
			      "dma_trace_start(): DMA reconfiguration (active stream_tag: %u)",
			      d->active_stream_tag);

		schedule_task_cancel(&d->dmat_work);
		err = dma_stop_legacy(d->dc.chan);
		if (err < 0) {
			mtrace_printf(LOG_LEVEL_ERROR,
				      "dma_trace_start(): DMA channel failed to stop");
		} else if (d->active_stream_tag != d->stream_tag) {
			/* Re-request a channel if different tag is provided */
			mtrace_printf(LOG_LEVEL_WARNING,
				      "dma_trace_start(): stream_tag change from %u to %u",
				      d->active_stream_tag, d->stream_tag);

			dma_channel_put_legacy(d->dc.chan);
			d->dc.chan = NULL;
			err = dma_copy_set_stream_tag(&d->dc, d->stream_tag);
		}
	} else {
		err = dma_copy_set_stream_tag(&d->dc, d->stream_tag);
	}

	if (err < 0)
		return err;

	/* Reset host buffer information as host is re-configuring dtrace */
	d->posn.host_offset = 0;

	d->active_stream_tag = d->stream_tag;

	err = dma_set_config_legacy(d->dc.chan, &d->gw_config);
	if (err < 0) {
		mtrace_printf(LOG_LEVEL_ERROR, "dma_set_config() failed: %d", err);
		goto error;
	}

	err = dma_start_legacy(d->dc.chan);
	if (err == 0)
		return 0;

error:
	dma_channel_put_legacy(d->dc.chan);
	d->dc.chan = NULL;

	return err;
}

static int dma_trace_get_avail_data(struct dma_trace_data *d,
				    struct dma_trace_buf *buffer,
				    int avail)
{
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

/** Invoked remotely by SOF_IPC_TRACE_DMA_PARAMS* Depends on
 * dma_trace_init_complete()
 */
int dma_trace_enable(struct dma_trace_data *d)
{
	int err;

	/* Allocate and initialize the dma trace buffer if needed */
	err = dma_trace_buffer_init(d);
	if (err < 0)
		return err;

#if CONFIG_DMA_GW
	/*
	 * GW DMA need finish DMA config and start before
	 * host driver trigger start DMA
	 */
	err = dma_trace_start(d);
	if (err < 0)
		goto out;
#endif

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

void dma_trace_disable(struct dma_trace_data *d)
{
	/* cancel trace work */
	schedule_task_cancel(&d->dmat_work);

	if (d->dc.chan) {
		dma_stop_legacy(d->dc.chan);
		dma_channel_put_legacy(d->dc.chan);
		d->dc.chan = NULL;
	}

#if (CONFIG_HOST_PTABLE)
	/* Free up the host SG if it is set */
	if (d->host_size) {
		dma_sg_free(&d->config.elem_array);
		d->host_size = 0;
	}
#endif
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

	if (!dma_trace_initialized(trace_data))
		return;

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
	dcache_invalidate_region((__sparse_force void __sparse_cache *)t, size);

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
	dcache_writeback_region((__sparse_force void __sparse_cache *)t, size);

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
			dcache_invalidate_region((__sparse_force void __sparse_cache *)buffer->w_ptr,
						 length);
			ret = memcpy_s(buffer->w_ptr, length, e, length);
			assert(!ret);
			dcache_writeback_region((__sparse_force void __sparse_cache *)buffer->w_ptr,
						length);
			buffer->w_ptr = (char *)buffer->w_ptr + length;
		} else {
			/* data is bigger than remaining margin so we wrap */
			dcache_invalidate_region((__sparse_force void __sparse_cache *)buffer->w_ptr,
						 margin);
			ret = memcpy_s(buffer->w_ptr, margin, e, margin);
			assert(!ret);
			dcache_writeback_region((__sparse_force void __sparse_cache *)buffer->w_ptr,
						margin);
			buffer->w_ptr = buffer->addr;

			dcache_invalidate_region((__sparse_force void __sparse_cache *)buffer->w_ptr,
						 length - margin);
			ret = memcpy_s(buffer->w_ptr, length - margin,
				       e + margin, length - margin);
			assert(!ret);
			dcache_writeback_region((__sparse_force void __sparse_cache *)buffer->w_ptr,
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
	k_spinlock_key_t key;

	if (!dma_trace_initialized(trace_data) ||
	    length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0) {
		return;
	}

	buffer = &trace_data->dmatb;

	key = k_spin_lock(&trace_data->lock);
	dtrace_add_event(e, length);

	/* if DMA trace copying is working or secondary core
	 * don't check if local buffer is half full
	 */
	if (trace_data->copy_in_progress ||
	    cpu_get_id() != PLATFORM_PRIMARY_CORE_ID) {
		k_spin_unlock(&trace_data->lock, key);
		return;
	}

	k_spin_unlock(&trace_data->lock, key);

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

	if (!dma_trace_initialized(trace_data) ||
	    length > DMA_TRACE_LOCAL_SIZE / 8 || length == 0) {
		return;
	}

	dtrace_add_event(e, length);
}
