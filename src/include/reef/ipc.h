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
#include <reef/trace.h>
#include <reef/dai.h>
#include <reef/lock.h>
#include <platform/platform.h>
#include <uapi/ipc.h>
#include <reef/audio/pipeline.h>
#include <reef/audio/component.h>

struct reef;

#define trace_ipc(__e)	trace_event(TRACE_CLASS_IPC, __e)
#define tracev_ipc(__e)	tracev_event(TRACE_CLASS_IPC, __e)
#define trace_ipc_error(__e)	trace_error(TRACE_CLASS_IPC, __e)

#define MSG_QUEUE_SIZE		12


/* IPC generic component device */
struct ipc_comp_dev {
	uint16_t flags;
	uint16_t state;

	/* component data */
	struct comp_dev *cd;

	/* lists */
	struct list_item list;		/* list in components */
};

/* IPC buffer device */
struct ipc_buffer_dev {
	uint16_t flags;
	uint16_t state;

	struct comp_buffer *cb;

	/* lists */
	struct list_item list;		/* list in buffers */
};

/* IPC pipeline device */
struct ipc_pipeline_dev {
	uint16_t flags;
	uint16_t state;

	struct pipeline *pipeline;

	/* lists */
	struct list_item list;		/* list in pipelines */
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

struct ipc {
	/* messaging */
	uint32_t host_msg;		/* current message from host */
	struct ipc_msg *dsp_msg;		/* current message to host */
	uint32_t host_pending;
	uint32_t dsp_pending;
	struct list_item msg_list;
	struct list_item empty_list;
	spinlock_t lock;
	struct ipc_msg message[MSG_QUEUE_SIZE];
	void *comp_data;

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);

	/* pipelines, components and buffers */
	struct list_item pipeline_list;	/* list of pipelines */
	struct list_item comp_list;		/* list of component devices */
	struct list_item buffer_list;	/* list of buffer devices */

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	(ipc)->private = data
#define ipc_get_drvdata(ipc) \
	(ipc)->private;


int ipc_init(struct reef *reef);
int platform_ipc_init(struct ipc *ipc);
void ipc_free(struct ipc *ipc);

int ipc_process_msg_queue(void);

int ipc_stream_send_notification(struct comp_dev *cdev,
		struct sof_ipc_stream_posn *posn);
int ipc_queue_host_message(struct ipc *ipc, uint32_t header,
	void *tx_data, size_t tx_bytes, void *rx_data,
	size_t rx_bytes, void (*cb)(void*, void*), void *cb_data);
int ipc_send_short_msg(uint32_t msg);

void ipc_platform_do_cmd(struct ipc *ipc);
void ipc_platform_send_msg(struct ipc *ipc);

/*
 * IPC Component creation and destruction.
 */
int ipc_comp_new(struct ipc *ipc, struct sof_ipc_comp *new);
void ipc_comp_free(struct ipc *ipc, uint32_t comp_id);

/*
 * IPC Buffer creation and destruction.
 */
int ipc_buffer_new(struct ipc *ipc, struct sof_ipc_buffer *buffer);
void ipc_buffer_free(struct ipc *ipc, uint32_t buffer_id);

/*
 * IPC Pipeline creation and destruction.
 */
int ipc_pipeline_new(struct ipc *ipc, struct sof_ipc_pipe_new *pipeline);
void ipc_pipeline_free(struct ipc *ipc, uint32_t pipeline_id);

/*
 * Pipeline component and buffer connections.
 */
int ipc_comp_connect(struct ipc *ipc,
	struct sof_ipc_pipe_comp_connect *connect);
int ipc_pipe_connect(struct ipc *ipc,
	struct sof_ipc_pipe_pipe_connect *connect);

struct ipc_comp_dev *ipc_get_comp(struct ipc *ipc, uint32_t id);
struct ipc_buffer_dev *ipc_get_buffer(struct ipc *ipc, uint32_t id);
struct ipc_pipeline_dev *ipc_get_pipeline(struct ipc *ipc, uint32_t id);

#endif
