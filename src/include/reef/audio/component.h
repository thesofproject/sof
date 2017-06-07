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

#ifndef __INCLUDE_AUDIO_COMPONENT_H__
#define __INCLUDE_AUDIO_COMPONENT_H__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/reef.h>
#include <reef/alloc.h>
#include <reef/dma.h>
#include <reef/stream.h>
#include <reef/audio/buffer.h>
#include <uapi/ipc.h>

/* audio component states
 * the states may transform as below:
 *        new()         params()          start()
 * none	-----> init ------> setup -----> running
 * none	<----- init <------ setup <----- running
 *        free()          reset()             stop()
 */
#define COMP_STATE_INIT		0	/* component being initialised */
#define COMP_STATE_SETUP       1       /* component inactive, but ready */
#define COMP_STATE_SUSPEND     2       /* component suspended */
#define COMP_STATE_DRAINING	3	/* component draining */
#define COMP_STATE_PREPARE	4	/* component prepared */
#define COMP_STATE_PAUSED	5	/* component paused */
#define COMP_STATE_RUNNING	6	/* component active */

/* standard component commands */

#define COMP_CMD_STOP		0	/* stop component stream */
#define COMP_CMD_START		1	/* start component stream */
#define COMP_CMD_PAUSE		2	/* immediately pause the component stream */
#define COMP_CMD_RELEASE	3	/* release paused component stream */
#define COMP_CMD_DRAIN		4	/* drain component buffers */
#define COMP_CMD_SUSPEND	5	/* suspend component */
#define COMP_CMD_RESUME		6	/* resume component */

#define COMP_CMD_VOLUME		100
#define COMP_CMD_MUTE		101
#define COMP_CMD_UNMUTE		102
#define COMP_CMD_ROUTE		103
#define COMP_CMD_SRC		104
#define COMP_CMD_LOOPBACK	105

#define COMP_CMD_TONE           106     /* Tone generator amplitude and frequency */
#define COMP_CMD_EQ_FIR_CONFIG  107     /* Configuration data for FIR EQ */
#define COMP_CMD_EQ_FIR_SWITCH  108     /* Update request for FIR EQ */
#define COMP_CMD_EQ_IIR_CONFIG  109     /* Configuration data for IIR EQ */
#define COMP_CMD_EQ_IIR_SWITCH  110     /* Response update request for IIR EQ */

/* MMAP IPC status */
#define COMP_CMD_IPC_MMAP_RPOS	200	/* host read position */
#define COMP_CMD_IPC_MMAP_PPOS	201	/* DAI presentation position */

#define COMP_CMD_IPC_MMAP_VOL(chan)	(216 + chan)	/* Volume */

/* component operations */
#define COMP_OPS_PARAMS		0
#define COMP_OPS_CMD		1
#define COMP_OPS_PREPARE	2
#define COMP_OPS_COPY		3
#define COMP_OPS_BUFFER		4
#define COMP_OPS_RESET		5

#define trace_comp(__e)	trace_event(TRACE_CLASS_COMP, __e)
#define trace_comp_error(__e)	trace_error(TRACE_CLASS_COMP, __e)
#define tracev_comp(__e)	tracev_event(TRACE_CLASS_COMP, __e)

struct comp_dev;
struct comp_buffer;
struct dai_config;
struct pipeline;

/*
 * Audio component operations - all mandatory.
 *
 * All component operations must return 0 for success, negative values for
 * errors and 1 to stop the pipeline walk operation.
 */
struct comp_ops {
	/* component creation and destruction */
	struct comp_dev *(*new)(struct sof_ipc_comp *comp);
	void (*free)(struct comp_dev *dev);

	/* set component audio stream paramters */
	int (*params)(struct comp_dev *dev, struct stream_params *params);

	/* preload buffers */
	int (*preload)(struct comp_dev *dev);

	/* set component audio stream paramters */
	int (*dai_config)(struct comp_dev *dev, struct dai_config *dai_config);

	/* used to pass standard and bespoke commands (with data) to component */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data);

	/* prepare component after params are set */
	int (*prepare)(struct comp_dev *dev);

	/* reset component */
	int (*reset)(struct comp_dev *dev);

	/* copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *dev);

	/* host buffer config */
	int (*host_buffer)(struct comp_dev *dev, struct dma_sg_elem *elem,
			uint32_t host_size);
};


/* audio component base driver "class" - used by all other component types */
struct comp_driver {
	uint32_t type;		/* SOF_COMP_ for driver */
	uint32_t module_id;

	struct comp_ops ops;	/* component operations */

	struct list_item list;	/* list of component drivers */
};	

/* audio component base device "class" - used by other component types */
struct comp_dev {

	/* runtime */
	uint16_t state;		/* COMP_STATE_ */
	uint16_t is_endpoint;	/* component is end point in pipeline */
	spinlock_t lock;	/* lock for this component */
	struct pipeline *pipeline;	/* pipeline we belong to */
	uint32_t period_frames;	/* frames to process per period - 0 is variable */
	uint32_t period_bytes;	/* bytes to process per period - 0 is variable */

	/* driver */
	struct comp_driver *drv;

	/* lists */
	struct list_item list;			/* list in components */
	struct list_item bsource_list;	/* list of source buffers */
	struct list_item bsink_list;	/* list of sink buffers */

	/* private data - core does not touch this */
	void *private;		/* private data */

	/* IPC config object header - MUST be at end as it's variable size/type */
	struct sof_ipc_comp comp;
};

#define COMP_SIZE(x) \
	(sizeof(struct comp_dev) - sizeof(struct sof_ipc_comp) + sizeof(x))

#define comp_set_drvdata(c, data) \
	c->private = data
#define comp_get_drvdata(c) \
	c->private;

void sys_comp_init(void);

/* component registration */
int comp_register(struct comp_driver *drv);
void comp_unregister(struct comp_driver *drv);

/* component creation and destruction - mandatory */
struct comp_dev *comp_new(struct sof_ipc_comp *comp);
static inline void comp_free(struct comp_dev *dev)
{
	dev->drv->ops.free(dev);
}

/* component parameter init - mandatory */
static inline int comp_params(struct comp_dev *dev,
	struct stream_params *params)
{
	return dev->drv->ops.params(dev, params);
}

/* component host buffer config
 * mandatory for host components, optional for the others.
 */
static inline int comp_host_buffer(struct comp_dev *dev,
	struct dma_sg_elem *elem, uint32_t host_size)
{
	if (dev->drv->ops.host_buffer)
		return dev->drv->ops.host_buffer(dev, elem, host_size);
	return 0;
}

/* send component command - mandatory */
static inline int comp_cmd(struct comp_dev *dev, int cmd, void *data)
{
	return dev->drv->ops.cmd(dev, cmd, data);
}

/* prepare component - mandatory */
static inline int comp_prepare(struct comp_dev *dev)
{
	return dev->drv->ops.prepare(dev);
}

/* component preload buffers -mandatory  */
static inline int comp_preload(struct comp_dev *dev)
{
	return dev->drv->ops.preload(dev);
}

/* copy component buffers - mandatory */
static inline int comp_copy(struct comp_dev *dev)
{
	return dev->drv->ops.copy(dev);
}

/* component reset and free runtime resources -mandatory  */
static inline int comp_reset(struct comp_dev *dev)
{
	return dev->drv->ops.reset(dev);
}

/* DAI configuration - only mandatory for DAI components */
static inline int comp_dai_config(struct comp_dev *dev,
	struct dai_config *dai_config)
{
	if (dev->drv->ops.dai_config)
		return dev->drv->ops.dai_config(dev, dai_config);
	return 0;
}

/* default base component initialisations */
void sys_comp_dai_init(void);
void sys_comp_host_init(void);
void sys_comp_mixer_init(void);
void sys_comp_mux_init(void);
void sys_comp_switch_init(void);
void sys_comp_volume_init(void);
void sys_comp_src_init(void);
void sys_comp_tone_init(void);
void sys_comp_eq_iir_init(void);


void sys_comp_eq_fir_init(void);

static inline void comp_set_endpoint(struct comp_dev *dev)
{
	dev->is_endpoint = 1;
}

/* reset component downstream buffers  */
static inline int comp_buffer_reset(struct comp_dev *dev)
{
	struct list_item *clist;

	/* reset downstream buffers */
	list_for_item(clist, &dev->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer, source_list);

		/* dont reset buffer if the component is not connected */
		if (!buffer->connected)
			continue;

		/* reset buffer next to the component*/
		bzero(buffer->addr, buffer->ipc_buffer.size);
		buffer->w_ptr = buffer->r_ptr = buffer->addr;
		buffer->end_addr = buffer->addr + buffer->ipc_buffer.size;
		buffer->free = buffer->ipc_buffer.size;
		buffer->avail = 0;
	}

	return 0;
}

static inline void comp_set_sink_params(struct comp_dev *dev,
	struct stream_params *params)
{
	struct list_item *clist;
	struct comp_buffer *sink;

	list_for_item(clist, &dev->bsink_list) {

		sink = container_of(clist, struct comp_buffer, source_list);
		sink->params = *params;
	}
}

static inline void comp_set_source_params(struct comp_dev *dev,
	struct stream_params *params)
{
	struct list_item *clist;
	struct comp_buffer *source;

	list_for_item(clist, &dev->bsource_list) {

		source = container_of(clist, struct comp_buffer, sink_list);
		source->params = *params;
	}
}

/* get a components preload period count from source buffer */
static inline uint32_t comp_get_preload_count(struct comp_dev *dev)
{
	struct comp_buffer *source;

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	return source->ipc_buffer.preload_count;
}

#endif
