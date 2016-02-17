/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_IPC_H__
#define __INCLUDE_IPC_H__

#include <stdint.h>
#include <reef/trace.h>
#include <reef/dai.h>
#include <reef/audio/component.h>

#define trace_ipc(__e)	trace_event(TRACE_CLASS_IPC, __e)

#define MSG_QUEUE_SIZE		8

/* IPC generic component device */
struct ipc_comp_dev {
	/* pipeline we belong to */
	uint32_t pipeline_id;
	struct pipeline *p;

	/* component data */
	struct comp_dev *cd;
	struct comp_desc desc;

	struct list_head list;		/* list in ipc data */
	void *private;
};

/* IPC FE PCM device - maps to host PCM */
struct ipc_pcm_dev {

	struct ipc_comp_dev dev;

	/* runtime config */	
	struct stream_params params;
};

/* IPC BE DAI device */ 
struct ipc_dai_dev {

	struct ipc_comp_dev dev;

	/* runtime config */
	struct dai_config dai_config;
};

struct ipc_msg {
	uint32_t type;		/* specific to platform */
	uint32_t size;		/* payload size in bytes */
	uint32_t *data;		/* pointer to payload data */
};

struct ipc {
	/* messaging */
	uint32_t host_msg;		/* current message from host */
	uint32_t dsp_msg;		/* current message to host */
	uint16_t host_pending;
	uint16_t dsp_pending;

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);

	/* mixers, dais and pcms */
	struct list_head comp_list;	/* list of component devices */

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	(ipc)->private = data
#define ipc_get_drvdata(ipc) \
	(ipc)->private;
#define ipc_get_pcm_comp(pipeline_id, id) \
	(struct ipc_pcm_dev *)ipc_get_comp(pipeline_id, id)
#define ipc_get_dai_comp(pipeline_id, id) \
	(struct ipc_dai_dev *)ipc_get_comp(pipeline_id, id)

int ipc_init(void);
int platform_ipc_init(struct ipc *ipc);
void ipc_free(struct ipc *ipc);

struct ipc_comp_dev *ipc_get_comp(int pipeline_id, uint32_t id);

int ipc_process_msg_queue(void);

int ipc_send_msg(struct ipc_msg *msg);
int ipc_send_short_msg(uint32_t msg);

/* dynamic pipeline API */
int ipc_comp_new(int pipeline_id, struct comp_desc *desc);
int ipc_comp_free(int pipeline_id, struct comp_desc *desc);

int ipc_buffer_new(int pipeline_id, struct buffer_desc *buffer_desc);
int ipc_buffer_free(int pipeline_id, struct buffer_desc *buffer_desc);

int ipc_comp_connect(int pipeline_id, struct comp_desc *source_desc,
	struct comp_desc *sink_desc, struct buffer_desc *buffer_desc);

#endif
