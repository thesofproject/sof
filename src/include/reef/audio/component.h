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
#include <reef/dma.h>
#include <reef/stream.h>

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

/* standard audio component types */
#define COMP_TYPE_HOST		0	/* host endpoint */
#define COMP_TYPE_VOLUME	1	/* Volume component */
#define COMP_TYPE_MIXER		2	/* Mixer component */
#define COMP_TYPE_MUX		3	/* MUX component */
#define COMP_TYPE_SWITCH	4	/* Switch component */
#define COMP_TYPE_DAI_SSP	5	/* SSP DAI endpoint */
#define COMP_TYPE_DAI_HDA	6	/* SSP DAI endpoint */

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
#define COMP_CMD_AVAIL_UPDATE          104

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

#define CHANNELS_ALL           0xffffffff

/* standard component command structures */
struct comp_volume {
	uint32_t update_bits;   /* bit 1s indicate the coresponding
                                   channels(indices) need to be updated */
	uint32_t volume;
};

struct comp_dev;
struct comp_buffer;
struct dai_config;
struct pipeline;

/*
 * Component position information.
 */
 struct comp_position {
 	uint32_t position;
 	uint32_t cycle_count;
 };

/*
 * Componnent period descriptors
 */
struct period_desc {
	uint32_t size;	/* period size in bytes */
	uint32_t number;
	uint32_t no_irq;	/* dont send periodic IRQ to host/DSP */
	uint32_t reserved;
};

/*
 * Pipeline buffer descriptor.
 */
struct buffer_desc {
	uint32_t size;		/* buffer size in bytes */
	struct period_desc sink_period;
	struct period_desc source_period;
};

/* audio component operations - all mandatory */
struct comp_ops {
	/* component creation and destruction */
	struct comp_dev *(*new)(uint32_t type, uint32_t index,
		uint32_t direction);
	void (*free)(struct comp_dev *dev);

	/* set component audio stream paramters */
	int (*params)(struct comp_dev *dev, struct stream_params *params);

	/* set component audio stream paramters */
	int (*dai_config)(struct comp_dev *dev, struct dai_config *dai_config);

	/* set dai component loopback mode */
	int (*dai_set_loopback)(struct comp_dev *dev, uint32_t lbm);

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
	uint32_t type;		/* COMP_TYPE_ for driver */
	uint32_t module_id;

	struct comp_ops ops;	/* component operations */

	struct list_item list;	/* list of component drivers */
};	

/* audio component base device "class" - used by other component types */
struct comp_dev {

	/* runtime */
	uint32_t id;		/* runtime ID of component */
	uint32_t state;		/* COMP_STATE_ */
	uint32_t is_dai;		/* component is graph DAI endpoint */
	uint32_t is_host;	/* component is graph host endpoint */
	uint32_t is_mixer;	/* component is mixer */
	uint32_t direction;	/* STREAM_DIRECTION_ */
	spinlock_t lock;	/* lock for this component */
	void *private;		/* private data */
	struct comp_driver *drv;
	struct pipeline *pipeline;	/* pipeline we belong to */

	/* lists */
	struct list_item bsource_list;	/* list of source buffers */
	struct list_item bsink_list;	/* list of sink buffers */
	struct list_item pipeline_list;	/* list in pipeline component devs */
	struct list_item endpoint_list;	/* list in pipeline endpoint devs */
	struct list_item schedule_list;	/* list in pipeline copy scheduler */
};

/* audio component buffer - connects 2 audio components together in pipeline */
struct comp_buffer {
	struct buffer_desc desc;
	struct stream_params params;

	/* runtime data */
	uint32_t id;		/* runtime ID of buffer */
	uint32_t connected;	/* connected in path */
	uint32_t avail;		/* available bytes for reading */
	uint32_t free;		/* free bytes for writing */
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */

	/* connected components */
	struct comp_dev *source;	/* source component */
	struct comp_dev *sink;		/* sink component */

	/* lists */
	struct list_item pipeline_list;	/* list in pipeline buffers */
	struct list_item source_list;	/* list in comp buffers */
	struct list_item sink_list;	/* list in comp buffers */
};

#define comp_set_drvdata(c, data) \
	c->private = data
#define comp_get_drvdata(c) \
	c->private;

void sys_comp_init(void);

/* component registration */
int comp_register(struct comp_driver *drv);
void comp_unregister(struct comp_driver *drv);

/* component creation and destruction - mandatory */
struct comp_dev *comp_new(struct pipeline *p, uint32_t type, uint32_t index,
	uint32_t id, uint32_t direction);
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

/* DAI configuration - only mandatory for DAI components */
static inline int comp_dai_loopback(struct comp_dev *dev,
	uint32_t lbm)
{
	if (dev->drv->ops.dai_set_loopback)
		return dev->drv->ops.dai_set_loopback(dev, lbm);
	return 0;
}

/* default base component initialisations */
void sys_comp_dai_init(void);
void sys_comp_host_init(void);
void sys_comp_mixer_init(void);
void sys_comp_mux_init(void);
void sys_comp_switch_init(void);
void sys_comp_volume_init(void);

static inline void comp_update_buffer_produce(struct comp_buffer *buffer)
{
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = buffer->end_addr - buffer->addr; /* full */
	else
		buffer->avail = buffer->end_addr - buffer->r_ptr +
			buffer->w_ptr - buffer->addr;
	buffer->free = buffer->desc.size - buffer->avail;
}

static inline void comp_update_buffer_consume(struct comp_buffer *buffer)
{
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = 0; /* empty */
	else
		buffer->avail = buffer->end_addr - buffer->r_ptr +
			buffer->w_ptr - buffer->addr;
	buffer->free = buffer->desc.size - buffer->avail;
}

static inline void comp_set_host_ep(struct comp_dev *dev)
{
	dev->is_host = 1;
	dev->is_dai = 0;
}

static inline void comp_set_dai_ep(struct comp_dev *dev)
{
	dev->is_host = 0;
	dev->is_dai = 1;
}

static inline void comp_clear_ep(struct comp_dev *dev)
{
	dev->is_host = 0;
	dev->is_dai = 0;
}

static inline void comp_set_mixer(struct comp_dev *dev)
{
	dev->is_mixer = 1;
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

#endif
