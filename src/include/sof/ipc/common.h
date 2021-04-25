/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_COMMON_H__
#define __SOF_IPC_COMMON_H__

#include <sof/lib/alloc.h>
#include <sof/list.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <user/trace.h>
#include <stdbool.h>
#include <stdint.h>

struct dma_sg_elem_array;
struct ipc_msg;

/* generic IPC header regardless of ABI MAJOR type that is always 4 byte aligned */
typedef uint32_t ipc_cmd_hdr;

/*
 * Common IPC logic uses standard types for abstract IPC features. This means all ABI MAJOR
 * abstraction is done in the IPC layer only and not in the surrounding infrastructure.
 */
#if CONFIG_IPC_MAJOR_3
#include <ipc/header.h>
#define ipc_from_hdr(x) ((struct sof_ipc_cmd_hdr *)x)
#elif CONFIG_IPC_MAJOR_4
#include <ipc4/header.h>
#define ipc_from_hdr(x) ((union ipc4_message_header *)x)
#else
#error "No or invalid IPC MAJOR version selected."
#endif

#define ipc_to_hdr(x) ((ipc_cmd_hdr *)x)

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
 * \brief Read a compact IPC message or return NULL for normal message.
 * @return Pointer to the compact message data.
 */
ipc_cmd_hdr *ipc_compact_read_msg(void);

/**
 * \brief Write a compact IPC message.
 * @param[in] hdr Compact message header data.
 * @return Number of words written.
 */
int ipc_compact_write_msg(ipc_cmd_hdr *hdr);

/**
 * \brief Validate mailbox contents for valid IPC header.
 * @return pointer to header if valid or NULL.
 */
ipc_cmd_hdr *mailbox_validate(void);

/**
 * Generic IPC command handler. Expects that IPC command (the header plus
 * any optional payload) is deserialized from the IPC HW by the platform
 * specific method.
 *
 * @param hdr Points to the IPC command header.
 */
void ipc_cmd(ipc_cmd_hdr *hdr);

/**
 * \brief IPC message to be processed on other core.
 * @param[in] core Core id for IPC to be processed on.
 * @return 1 if successful (reply sent by other core), error code otherwise.
 */
int ipc_process_on_core(uint32_t core);

#endif /* __SOF_DRIVERS_IPC_H__ */
