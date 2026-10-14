/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Yan Wang <yan.wang@linux.intel.com>
 */

/**
 * \file include/sof/trace/dma-trace.h
 * \brief Direct Memory Access (DMA) trace buffer management and host streaming.
 * \ingroup trace_logging_api
 */

#ifndef __SOF_TRACE_DMA_TRACE_H__
#define __SOF_TRACE_DMA_TRACE_H__

#include <sof/lib/dma.h>
#include <rtos/task.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <ipc/trace.h>
#include <stdint.h>

struct ipc_msg;
struct sof;

/**
 * \brief Internal ring buffer descriptor for DSP trace packets.
 * \ingroup trace_logging_api
 */
struct dma_trace_buf {
	void *w_ptr;		/**< Buffer write pointer where new trace packets are appended */
	void *r_ptr;		/**< Buffer read pointer tracking data transferred by DMA */
	void *addr;		/**< Base address of the circular trace buffer */
	void *end_addr;		/**< Upper boundary address of the trace buffer */
	uint32_t size;		/**< Total size of the buffer in bytes */
	uint32_t avail;		/**< Number of readable bytes currently available */
};

/**
 * \brief Main DMA trace context and state machine descriptor.
 * \ingroup trace_logging_api
 */
struct dma_trace_data {
	struct dma_sg_config config;		/**< Scatter-gather configuration for host DMA */
	struct dma_trace_buf dmatb;		/**< Internal circular buffer tracking read/write pointers */
	struct dma_copy dc;			/**< DMA copy engine instance */
	struct sof_ipc_dma_trace_posn posn;	/**< Position report payload sent to host */
	struct ipc_msg *msg;			/**< IPC message used to report buffer position */
	uint32_t host_size;			/**< Total size of host-allocated trace buffer */
	struct task dmat_work;			/**< Periodic scheduling task driving DMA transfers */
	uint32_t enabled;			/**< 1 if DMA trace streaming is active, 0 otherwise */
	uint32_t copy_in_progress;		/**< 1 if a DMA copy operation is currently underway */
	uint32_t stream_tag;			/**< Assigned host DMA stream tag */
	uint32_t active_stream_tag;		/**< Current active stream tag */
	uint32_t dma_copy_align;		/**< Minimal byte alignment required for DMA transfer chunks */
	uint32_t dropped_entries;		/**< Cumulative counter of dropped entries due to buffer overflow */
	struct k_spinlock lock;			/**< Spinlock synchronizing buffer writes and DMA triggers */
	uint64_t time_delta;			/**< Offset between host wallclock and DSP hardware timer */
};

/**
 * \brief Early initialization of the DMA trace subsystem during bootstrap.
 * \ingroup trace_logging_api
 * @param[in,out] sof Global SOF system context.
 * @return 0 on success, or negative error code on failure.
 */
int dma_trace_init_early(struct sof *sof);

/**
 * \brief Complete initialization of DMA trace once host channel is negotiated.
 * \ingroup trace_logging_api
 * @param[in,out] d DMA trace data context.
 * @return 0 on success, or negative error code on failure.
 */
int dma_trace_init_complete(struct dma_trace_data *d);

/**
 * \brief Configure the host-side destination scatter-gather buffer for DMA trace.
 * \ingroup trace_logging_api
 * @param[in,out] d DMA trace data context.
 * @param[in] elem_array Array of scatter-gather buffer page elements.
 * @param[in] host_size Total size in bytes of the host buffer.
 * @return 0 on success, or negative error code on failure.
 */
int dma_trace_host_buffer(struct dma_trace_data *d,
			  struct dma_sg_elem_array *elem_array,
			  uint32_t host_size);

/**
 * \brief Enable DMA trace streaming to the host.
 * \ingroup trace_logging_api
 * @param[in,out] d DMA trace data context.
 * @return 0 on success, or negative error code on failure.
 */
int dma_trace_enable(struct dma_trace_data *d);

/**
 * \brief Disable DMA trace streaming and cancel pending periodic tasks.
 * \ingroup trace_logging_api
 * @param[in,out] d DMA trace data context.
 */
void dma_trace_disable(struct dma_trace_data *d);

/**
 * \brief Flush residual trace data from local DSP buffer to destination memory or mailbox.
 * \ingroup trace_logging_api
 * @param[out] destination Pointer to destination memory buffer, or NULL for default mailbox.
 */
void dma_trace_flush(void *destination);

/**
 * \brief Globally unpause and resume trace event logging.
 * \ingroup trace_logging_api
 */
void dma_trace_on(void);

/**
 * \brief Globally pause trace event logging.
 * \ingroup trace_logging_api
 */
void dma_trace_off(void);

/**
 * \brief Append a binary trace event to the circular buffer.
 * \ingroup trace_logging_api
 * @param[in] e Pointer to binary event packet.
 * @param[in] size Packet size in bytes.
 */
void dtrace_event(const char *e, uint32_t size);

/**
 * \brief Append a binary trace event to the buffer from atomic or interrupt context.
 * \ingroup trace_logging_api
 * @param[in] e Pointer to binary event packet.
 * @param[in] length Size of packet in bytes.
 */
void dtrace_event_atomic(const char *e, uint32_t length);

/**
 * \brief Check whether DMA trace subsystem has been initialized.
 * \ingroup trace_logging_api
 * @param[in] d DMA trace context.
 * @return True if initialized with valid buffer memory, false otherwise.
 */
static inline bool dma_trace_initialized(const struct dma_trace_data *d)
{
	return d && d->dmatb.addr;
}

/**
 * \brief Retrieve global DMA trace data context from system runtime.
 * \ingroup trace_logging_api
 * @return Pointer to struct dma_trace_data.
 */
static inline struct dma_trace_data *dma_trace_data_get(void)
{
	return sof_get()->dmat;
}

/**
 * \brief Calculate contiguous bytes remaining until the end of the circular buffer.
 * \ingroup trace_logging_api
 * @param[in] buffer Pointer to circular trace buffer descriptor.
 * @return Contiguous byte margin before write wrap-around.
 */
static inline uint32_t dtrace_calc_buf_margin(struct dma_trace_buf *buffer)
{
	return (char *)buffer->end_addr - (char *)buffer->w_ptr;
}

#endif /* __SOF_TRACE_DMA_TRACE_H__ */
