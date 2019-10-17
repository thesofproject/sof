/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_IPC_H__
#define __SOF_DRIVERS_IPC_H__

#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/trace/dma-trace.h>
#include <sof/trace/trace.h>
#include <ipc/header.h>
#include <user/trace.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;
struct dai_config;
struct dma;
struct dma_sg_elem_array;
struct pipeline;
struct sof;
struct sof_ipc_buffer;
struct sof_ipc_comp;
struct sof_ipc_comp_event;
struct sof_ipc_dai_config;
struct sof_ipc_host_buffer;
struct sof_ipc_pipe_comp_connect;
struct sof_ipc_pipe_new;
struct sof_ipc_stream_posn;

#define trace_ipc(format, ...) \
	trace_event(TRACE_CLASS_IPC, format, ##__VA_ARGS__)
#define tracev_ipc(format, ...) \
	tracev_event(TRACE_CLASS_IPC, format, ##__VA_ARGS__)
#define trace_ipc_error(format, ...) \
	trace_error(TRACE_CLASS_IPC, format, ##__VA_ARGS__)

#define MSG_QUEUE_SIZE		12

#define COMP_TYPE_COMPONENT	1
#define COMP_TYPE_BUFFER	2
#define COMP_TYPE_PIPELINE	3

/* validates internal non tail structures within IPC command structure */
#define IPC_IS_SIZE_INVALID(object)					\
	object.hdr.size == sizeof(object) ? 0 : 1

/* convenience error trace for mismatched internal structures */
#define IPC_SIZE_ERROR_TRACE(class, object)				\
	trace_error(class, "ipc: size %d expected %d",			\
		    object.hdr.size, sizeof(object))

/* IPC generic component device */
struct ipc_comp_dev {
	uint16_t type;	/* COMP_TYPE_ */
	uint16_t state;

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
	uint8_t tx_data[SOF_IPC_MSG_MAX_SIZE];	/* pointer to payload data */
	uint32_t rx_size;	/* payload size in bytes */
	uint8_t rx_data[SOF_IPC_MSG_MAX_SIZE];	/* pointer to payload data */
	struct list_item list;
	void (*cb)(void *cb_data);
	void *cb_data;
};

struct ipc_shared_context {
	struct ipc_msg *dsp_msg;	/* current message to host */
	uint32_t dsp_pending;
	struct list_item msg_list;
	struct list_item empty_list;
	struct ipc_msg message[MSG_QUEUE_SIZE];

	struct list_item comp_list;	/* list of component devices */
};

struct ipc {
	spinlock_t *lock;	/* locking mechanism */
	void *comp_data;

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);

	/* DMA for Trace*/
	struct dma_trace_data *dmat;

	/* PM */
	int pm_prepare_D3;	/* do we need to prepare for D3 */

	/* mmap for posn_offset */
	struct pipeline *posn_map[PLATFORM_MAX_STREAMS];

	/* context shared between cores */
	struct ipc_shared_context *shared_ctx;

	/* processing task */
	struct task ipc_task;

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	((ipc)->private = data)
#define ipc_get_drvdata(ipc) \
	((ipc)->private)


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

void ipc_free(struct ipc *ipc);

int ipc_process_msg_queue(void);
void ipc_schedule_process(struct ipc *ipc);

int ipc_stream_send_position(struct comp_dev *cdev,
		struct sof_ipc_stream_posn *posn);
int ipc_send_comp_notification(struct comp_dev *cdev,
			       struct sof_ipc_comp_event *event);
int ipc_stream_send_xrun(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn);

int ipc_queue_host_message(struct ipc *ipc, uint32_t header, void *tx_data,
			   size_t tx_bytes, uint32_t replace);

void ipc_platform_send_msg(struct ipc *ipc);

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
 * Configure all DAI components attached to DAI.
 */
int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config);

/* send DMA trace host buffer position to host */
int ipc_dma_trace_send_position(void);

/* get posn offset by pipeline. */
int ipc_get_posn_offset(struct ipc *ipc, struct pipeline *pipe);

struct sof_ipc_cmd_hdr *mailbox_validate(void);

/**
 * Generic IPC command handler. Expects that IPC command (the header plus
 * any optional payload) is deserialized from the IPC HW by the platform
 * specific method.
 *
 * @param hdr Points to the IPC command header.
 */
void ipc_cmd(struct sof_ipc_cmd_hdr *hdr);

#endif /* __SOF_DRIVERS_IPC_H__ */
