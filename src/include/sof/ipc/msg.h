/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_MSG_H__
#define __SOF_IPC_MSG_H__

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <sof/ipc/common.h>
#include <stdbool.h>
#include <stdint.h>

struct dai_config;
struct dma;
struct dma_sg_elem_array;

struct ipc_msg {
	uint32_t header;	/* specific to platform */
	uint32_t tx_size;	/* payload size in bytes */
	void *tx_data;		/* pointer to payload data */
	struct list_item list;
};

/**
 * \brief Initialise a new IPC message.
 * @param header Message header metadata
 * @param size Message size in bytes.
 * @return New IPC message.
 */
static inline struct ipc_msg *ipc_msg_init(uint32_t header, uint32_t size)
{
	struct ipc_msg *msg;

	msg = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*msg));
	if (!msg)
		return NULL;

	msg->tx_data = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, size);
	if (!msg->tx_data) {
		rfree(msg);
		return NULL;
	}

	msg->header = header;
	msg->tx_size = size;
	list_init(&msg->list);

	return msg;
}

/**
 * \brief Frees IPC message header and data.
 * @param msg The IPC message to be freed.
 */
static inline void ipc_msg_free(struct ipc_msg *msg)
{
	if (!msg)
		return;

	struct ipc *ipc = ipc_get();
	uint32_t flags;

	spin_lock_irq(&ipc->lock, flags);

	list_item_del(&msg->list);
	rfree(msg->tx_data);
	rfree(msg);

	spin_unlock_irq(&ipc->lock, flags);
}

/**
 * \brief Sends the next message in the IPC message queue.
 */
void ipc_send_queued_msg(void);

/**
 * \brief Queues an IPC message for transmission.
 * @param msg The IPC message to be freed.
 * @param data The message data.
 * @param high_priority True if a high priortity message.
 */
void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority);

/**
 * \brief Build stream position IPC message.
 * @param[in,out] posn Stream position message
 * @param[in] type Stream message type
 * @param[in] id Stream ID.
 */
void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn, uint32_t type,
			   uint32_t id);

/**
 * \brief Build component event IPC message.
 * @param[in,out] event Component event message
 * @param[in] type Component event type
 * @param[in] id Component ID.
 */
void ipc_build_comp_event(struct sof_ipc_comp_event *event, uint32_t type,
			  uint32_t id);

/**
 * \brief Build trace position IPC message.
 * @param[in,out] posn Trace position message
 */
void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn);

#endif
