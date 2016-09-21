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
#include <reef/audio/component.h>
#include <reef/lock.h>

#define trace_ipc(__e)	trace_event(TRACE_CLASS_IPC, __e)
#define tracev_ipc(__e)	tracev_event(TRACE_CLASS_IPC, __e)
#define trace_ipc_error(__e)	trace_error(TRACE_CLASS_IPC, __e)

#define MSG_QUEUE_SIZE		4

/* Intel IPC stream states. TODO we have to manually track this atm as is
does not align to ALSA. TODO align IPC with ALSA ops */
#define IPC_HOST_RESET		0	/* host stream has been reset */
#define IPC_HOST_ALLOC		1	/* host stream has been alloced */
#define IPC_HOST_RUNNING	2	/* host stream is running */
#define IPC_HOST_PAUSED		3	/* host stream has been paused */

#define MSG_MAX_SIZE		128

/* IPC generic component device */
struct ipc_comp_dev {
	/* pipeline we belong to */
	uint32_t pipeline_id;
	struct pipeline *p;

	/* component data */
	union {
		struct comp_dev *cd;
		struct comp_buffer *cb;
	};

	struct list_item list;		/* list in ipc data */
};

/* IPC FE PCM device - maps to host PCM */
struct ipc_pcm_dev {

	struct ipc_comp_dev dev;

	/* runtime config */
	struct stream_params params;
	uint32_t state;		/* IPC_HOST_*/
};

/* IPC BE DAI device */
struct ipc_dai_dev {

	struct ipc_comp_dev dev;

	/* runtime config */
	struct dai_config dai_config;
};

/* IPC buffer device */
struct ipc_buffer_dev {

	struct ipc_comp_dev dev;
};

struct ipc_msg {
	uint32_t header;	/* specific to platform */
	uint32_t tx_size;	/* payload size in bytes */
	uint8_t tx_data[MSG_MAX_SIZE];		/* pointer to payload data */
	uint32_t rx_size;	/* payload size in bytes */
	uint8_t rx_data[MSG_MAX_SIZE];		/* pointer to payload data */
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

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);

	/* mixers, dais and pcms */
	uint32_t next_comp_id;
	uint32_t next_buffer_id;
	struct list_item comp_list;	/* list of component devices */
	struct list_item buffer_list;	/* list of buffer devices */

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	(ipc)->private = data
#define ipc_get_drvdata(ipc) \
	(ipc)->private;
#define ipc_get_pcm_comp(id) \
	(struct ipc_pcm_dev *)ipc_get_comp(id)
#define ipc_get_dai_comp(id) \
	(struct ipc_dai_dev *)ipc_get_comp(id)

int ipc_init(void);
int platform_ipc_init(struct ipc *ipc);
void ipc_free(struct ipc *ipc);

struct ipc_comp_dev *ipc_get_comp(uint32_t id);

int ipc_process_msg_queue(void);

int ipc_stream_send_notification(struct comp_dev *cdev,
	struct comp_position *cp);
int ipc_queue_host_message(struct ipc *ipc, uint32_t header,
	void *tx_data, size_t tx_bytes, void *rx_data,
	size_t rx_bytes, void (*cb)(void*, void*), void *cb_data);
int ipc_send_short_msg(uint32_t msg);

void ipc_platform_do_cmd(struct ipc *ipc);
void ipc_platform_send_msg(struct ipc *ipc);

/* dynamic pipeline API */
int ipc_comp_new(int pipeline_id, uint32_t type, uint32_t index,
	uint8_t direction);
void ipc_comp_free(uint32_t comp_id);

int ipc_buffer_new(int pipeline_id, struct buffer_desc *buffer_desc);
void ipc_buffer_free(uint32_t buffer_id);

int ipc_comp_connect(uint32_t source_id, uint32_t sink_id,
	uint32_t buffer_id);

#endif
