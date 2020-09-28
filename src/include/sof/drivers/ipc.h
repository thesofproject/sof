/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_IPC_H__
#define __SOF_DRIVERS_IPC_H__

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

#define COMP_TYPE_COMPONENT	1
#define COMP_TYPE_BUFFER	2
#define COMP_TYPE_PIPELINE	3

/** \brief Scheduling period for IPC task in microseconds. */
#define IPC_PERIOD_USEC	100

/* validates internal non tail structures within IPC command structure */
#define IPC_IS_SIZE_INVALID(object)					\
	(object).hdr.size == sizeof(object) ? 0 : 1

/* ipc trace context, used by multiple units */
extern struct tr_ctx ipc_tr;

/* convenience error trace for mismatched internal structures */
#define IPC_SIZE_ERROR_TRACE(ctx, object)		\
	tr_err(ctx, "ipc: size %d expected %d",		\
	       (object).hdr.size, sizeof(object))

/* IPC generic component device */
struct ipc_comp_dev {
	uint16_t type;	/* COMP_TYPE_ */
	uint16_t core;
	uint32_t id;

	/* component type data */
	union {
		struct comp_dev *cd;
		struct comp_buffer *cb;
		struct pipeline *pipeline;
	};

	/* lists */
	struct list_item list;		/* list in components */
};

struct ipc_msg {
	uint32_t header;	/* specific to platform */
	uint32_t tx_size;	/* payload size in bytes */
	void *tx_data;		/* pointer to payload data */
	struct list_item list;
};

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

static inline struct ipc *ipc_get(void)
{
	return sof_get()->ipc;
}

static inline uint64_t ipc_task_deadline(void *data)
{
	/* TODO: Currently it's a workaround to execute IPC tasks ASAP.
	 * In the future IPCs should have a cycle budget and deadline
	 * should be calculated based on that value. This means every
	 * IPC should have its own maximum number of cycles that is required
	 * to finish processing. This will allow us to calculate task deadline.
	 */
	return SOF_TASK_DEADLINE_NOW;
}

static inline int32_t ipc_comp_pipe_id(const struct ipc_comp_dev *icd)
{
	switch (icd->type) {
	case COMP_TYPE_COMPONENT:
		return dev_comp_pipe_id(icd->cd);
	case COMP_TYPE_BUFFER:
		return icd->cb->pipeline_id;
	case COMP_TYPE_PIPELINE:
		return icd->pipeline->ipc_pipe.pipeline_id;
	default:
		tr_err(&ipc_tr, "Unknown ipc component type %u", icd->type);
		return -EINVAL;
	};
}

static inline void ipc_build_stream_posn(struct sof_ipc_stream_posn *posn,
					 uint32_t type, uint32_t id)
{
	posn->rhdr.hdr.cmd = SOF_IPC_GLB_STREAM_MSG | type | id;
	posn->rhdr.hdr.size = sizeof(*posn);
	posn->comp_id = id;
}

static inline void ipc_build_comp_event(struct sof_ipc_comp_event *event,
					uint32_t type, uint32_t id)
{
	event->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_NOTIFICATION |
		id;
	event->rhdr.hdr.size = sizeof(*event);
	event->src_comp_type = type;
	event->src_comp_id = id;
}

static inline void ipc_build_trace_posn(struct sof_ipc_dma_trace_posn *posn)
{
	posn->rhdr.hdr.cmd =  SOF_IPC_GLB_TRACE_MSG |
		SOF_IPC_TRACE_DMA_POSITION;
	posn->rhdr.hdr.size = sizeof(*posn);
}

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

	platform_shared_commit(msg, sizeof(*msg));

	return msg;
}

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

	platform_shared_commit(ipc, sizeof(*ipc));

	spin_unlock_irq(&ipc->lock, flags);
}

int ipc_init(struct sof *sof);

/**
 * \brief Provides platform specific IPC initialization.
 * @param ipc Global IPC context
 * @return 0 if succeeded, error code otherwise.
 *
 * This function must be implemented by the platform. It is called from the
 * main IPC code, at the end of ipc_init().
 *
 * If the platform requires any private data to be associated with the IPC
 * context, it may allocate it here and attach to the global context using
 * ipc_set_drvdata(). Other platform specific IPC functions, like
 * ipc_platform_do_cmd(), may obtain it later from the context using
 * ipc_get_drvdata().
 */
int platform_ipc_init(struct ipc *ipc);

enum task_state ipc_platform_do_cmd(void *data);

void ipc_platform_complete_cmd(void *data);

void ipc_free(struct ipc *ipc);

void ipc_schedule_process(struct ipc *ipc);

int ipc_platform_send_msg(struct ipc_msg *msg);

void ipc_send_queued_msg(void);

void ipc_msg_send(struct ipc_msg *msg, void *data, bool high_priority);

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
 * \brief Retrieves the ipc_data_host_buffer allocated by the platform ipc.
 * @return Pointer to the data.
 *
 * This function must be implemented by platforms which use
 * ipc...page_descriptors() while processing host page tables.
 */
struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc);

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

/*
 * IPC Component creation and destruction.
 */
int ipc_comp_new(struct ipc *ipc, struct sof_ipc_comp *new);
int ipc_comp_free(struct ipc *ipc, uint32_t comp_id);

/*
 * IPC Buffer creation and destruction.
 */
int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *buffer);
int ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id);

/*
 * IPC Pipeline creation and destruction.
 */
int ipc_pipeline_new(struct ipc *ipc, struct sof_ipc_pipe_new *pipeline);
int ipc_pipeline_free(struct ipc *ipc, uint32_t comp_id);
int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id);

/*
 * Pipeline component and buffer connections.
 */
int ipc_comp_connect(struct ipc *ipc,
	struct sof_ipc_pipe_comp_connect *connect);

/*
 * Get component by ID.
 */
struct ipc_comp_dev *ipc_get_comp_by_id(struct ipc *ipc, uint32_t id);

/*
 * Get component by pipeline ID.
 */
struct ipc_comp_dev *ipc_get_comp_by_ppl_id(struct ipc *ipc, uint16_t type,
					    uint32_t ppl_id);

/*
 * Configure all DAI components attached to DAI.
 */
int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config);

/* send DMA trace host buffer position to host */
int ipc_dma_trace_send_position(void);

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

/**
 * \brief Initialise IPC hardware for polling mode.
 * @return 0 if successful error code otherwise.
 */
int ipc_platform_poll_init(void);

/**
 * \brief Tell host DSP has completed command.
 */
void ipc_platform_poll_set_cmd_done(void);

/**
 * \brief Check whether there is a new IPC command from host.
 * @return 1 if new command is pending from host.
 */
int ipc_platform_poll_is_cmd_pending(void);

/**
 * \brief Check whether host is ready for new IPC from DSP.
 * @return 1 if host is ready for a new command from DSP.
 */
int ipc_platform_poll_is_host_ready(void);

/**
 * \brief Transmit new message to host.
 * @return 0 if successful error code otherwise.
 */
int ipc_platform_poll_tx_host_msg(struct ipc_msg *msg);

#endif /* __SOF_DRIVERS_IPC_H__ */
