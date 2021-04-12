/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_COMMON_H__
#define __SOF_IPC_COMMON_H__

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
#include <ipc/control.h>
#include <ipc/header.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <stdbool.h>
#include <stdint.h>

struct dai_config;
struct dma;
struct dma_sg_elem_array;
struct sof_ipc_buffer;
struct sof_ipc_comp;
struct sof_ipc_comp_event;
struct sof_ipc_dai_config;
struct sof_ipc_host_buffer;
struct sof_ipc_pipe_comp_connect;
struct sof_ipc_pipe_new;
struct sof_ipc_stream_posn;
struct ipc_msg;

/* validates internal non tail structures within IPC command structure */
#define IPC_IS_SIZE_INVALID(object)					\
	(object).hdr.size == sizeof(object) ? 0 : 1

/* ipc trace context, used by multiple units */
extern struct tr_ctx ipc_tr;

/* convenience error trace for mismatched internal structures */
#define IPC_SIZE_ERROR_TRACE(ctx, object)		\
	tr_err(ctx, "ipc: size %d expected %d",		\
	       (object).hdr.size, sizeof(object))

/* Returns pipeline source component */
#define ipc_get_ppl_src_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_UPSTREAM)

/* Returns pipeline sink component */
#define ipc_get_ppl_sink_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_DOWNSTREAM)

struct ipc {
	spinlock_t lock;	/* locking mechanism */
	void *comp_data;

	/* PM */
	int pm_prepare_D3;	/* do we need to prepare for D3 */

	struct list_item msg_list;	/* queue of messages to be sent */
	bool is_notification_pending;	/* notification is being sent to host */

	struct list_item comp_list;	/* list of component devices */

	/* processing task */
	struct task ipc_task;

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	((ipc)->private = data)
#define ipc_get_drvdata(ipc) \
	((ipc)->private)

extern struct task_ops ipc_task_ops;

/**
 * \brief Get the IPC global context.
 * @return The global IPC context.
 */
static inline struct ipc *ipc_get(void)
{
	return sof_get()->ipc;
}

/**
 * \brief Build stream position IPC message.
 * @param[in,out] posn Stream position message
 * @param[in] type Stream message type
 * @param[in] id Stream ID.
 */
static inline void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn,
					 uint32_t type, uint32_t id)
{
	posn->rhdr.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | type | id;
	posn->rhdr.hdr.size = sizeof(*posn);
	posn->comp_id = id;
}

/**
 * \brief Build component event IPC message.
 * @param[in,out] event Component event message
 * @param[in] type Component event type
 * @param[in] id Component ID.
 */
static inline void ipc_build_comp_event(struct sof_ipc_comp_event *event,
					uint32_t type, uint32_t id)
{
	event->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_NOTIFICATION |
		id;
	event->rhdr.hdr.size = sizeof(*event);
	event->src_comp_type = type;
	event->src_comp_id = id;
}

/**
 * \brief Build trace position IPC message.
 * @param[in,out] posn Trace position message
 */
static inline void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn)
{
	posn->rhdr.hdr.cmd =  SOF_IPC_GLB_TRACE_MSG |
		SOF_IPC_TRACE_DMA_POSITION;
	posn->rhdr.hdr.size = sizeof(*posn);
}

/**
 * \brief Initialise global IPC context.
 * @param[in,out] sof Global SOF context.
 * @return 0 on success.
 */
int ipc_init(struct sof *sof);

/**
 * \brief Free global IPC context.
 * @param[in] ipc Global IPC context.
 */
void ipc_free(struct ipc *ipc);

/**
 * \brief Data provided by the platform which use ipc...page_descriptors().
 *
 * Note: this should be made private for ipc-host-ptable.c and ipc
 * drivers for platforms that use ptables.
 */
struct ipc_data_host_buffer {
	/* DMA */
	struct dma *dmac;
	uint8_t *page_table;
};

/**
 * \brief Processes page tables for the host buffer.
 * @param[in] ipc Ipc
 * @param[in] ring Ring description sent via Ipc
 * @param[in] direction Direction (playback/capture)
 * @param[out] elem_array Array of SG elements
 * @param[out] ring_size Size of the ring
 * @return Status, 0 if successful, error code otherwise.
 */
int ipc_process_host_buffer(struct ipc *ipc,
			    struct sof_ipc_host_buffer *ring,
			    uint32_t direction,
			    struct dma_sg_elem_array *elem_array,
			    uint32_t *ring_size);

/**
 * \brief Send DMA trace host buffer position to host.
 * @return 0 on success.
 */
int ipc_dma_trace_send_position(void);

/**
 * \brief Validate mailbox contents for valid IPC header.
 * @return pointer to header if valid or NULL.
 */
struct sof_ipc_cmd_hdr *mailbox_validate(void);

/**
 * Generic IPC command handler. Expects that IPC command (the header plus
 * any optional payload) is deserialized from the IPC HW by the platform
 * specific method.
 *
 * @param hdr Points to the IPC command header.
 */
void ipc_cmd(struct sof_ipc_cmd_hdr *hdr);

/**
 * \brief IPC message to be processed on other core.
 * @param[in] core Core id for IPC to be processed on.
 * @return 1 if successful (reply sent by other core), error code otherwise.
 */
int ipc_process_on_core(uint32_t core);

#endif /* __SOF_DRIVERS_IPC_H__ */
