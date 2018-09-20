/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_IPC_H__
#define __INCLUDE_IPC_H__

#include <stdint.h>
#include <sof/trace.h>
#include <sof/dai.h>
#include <sof/lock.h>
#include <platform/platform.h>
#include <uapi/ipc.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component.h>
#include <sof/dma-trace.h>

struct sof;
struct dai_config;

#define trace_ipc(__e)	trace_event(TRACE_CLASS_IPC, __e)
#define tracev_ipc(__e)	tracev_event(TRACE_CLASS_IPC, __e)
#define trace_ipc_error(__e)	trace_error(TRACE_CLASS_IPC, __e)

#define MSG_QUEUE_SIZE		12

#define COMP_TYPE_COMPONENT	1
#define COMP_TYPE_BUFFER	2
#define COMP_TYPE_PIPELINE	3

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
	uint8_t tx_data[SOF_IPC_MSG_MAX_SIZE];		/* pointer to payload data */
	uint32_t rx_size;	/* payload size in bytes */
	uint8_t rx_data[SOF_IPC_MSG_MAX_SIZE];		/* pointer to payload data */
	struct list_item list;
	void (*cb)(void *cb_data, void *mailbox_data);
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
	/* messaging */
	uint32_t host_msg;		/* current message from host */
	uint32_t host_pending;
	spinlock_t lock;
	void *comp_data;

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);

	/* DMA for Trace*/
	struct dma_trace_data *dmat;

	/* mmap for posn_offset */
	struct pipeline *posn_map[PLATFORM_MAX_STREAMS];

	/* context shared between cores */
	struct ipc_shared_context *shared_ctx;

	/* processing task */
	struct task ipc_task;

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	(ipc)->private = data
#define ipc_get_drvdata(ipc) \
	(ipc)->private;


int ipc_init(struct sof *sof);
int platform_ipc_init(struct ipc *ipc);
void ipc_free(struct ipc *ipc);

int ipc_process_msg_queue(void);
void ipc_process_task(void *data);
void ipc_schedule_process(struct ipc *ipc);

int ipc_stream_send_position(struct comp_dev *cdev,
		struct sof_ipc_stream_posn *posn);
int ipc_stream_send_xrun(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn);

int ipc_queue_host_message(struct ipc *ipc, uint32_t header,
	void *tx_data, size_t tx_bytes, void *rx_data,
	size_t rx_bytes, void (*cb)(void*, void*), void *cb_data, uint32_t replace);
int ipc_send_short_msg(uint32_t msg);

void ipc_platform_do_cmd(struct ipc *ipc);
void ipc_platform_send_msg(struct ipc *ipc);

/* create a SG page table eme list from a compressed page table */
int ipc_parse_page_descriptors(uint8_t *page_table,
			       struct sof_ipc_host_buffer *ring,
			       struct dma_sg_elem_array *elem_array,
			       uint32_t direction);
int ipc_get_page_descriptors(struct dma *dmac, uint8_t *page_table,
			     struct sof_ipc_host_buffer *ring);

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
struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id);

/*
 * Configure all DAI components attached to DAI.
 */
int ipc_comp_dai_config(struct ipc *ipc, struct sof_ipc_dai_config *config);

/* send DMA trace host buffer position to host */
int ipc_dma_trace_send_position(void);

/* get posn offset by pipeline. */
int ipc_get_posn_offset(struct ipc *ipc, struct pipeline *pipe);
#endif
