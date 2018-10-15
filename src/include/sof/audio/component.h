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
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <sof/alloc.h>
#include <sof/dma.h>
#include <sof/stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/pipeline.h>
#include <uapi/ipc.h>

/*
 * Audio Component States
 *
 * States may transform as below:-
 *
 *
 *                            -------------
 *                   pause    |           |    stop/xrun
 *              +-------------| ACTIVITY  |---------------+
 *              |             |           |               |      prepare
 *              |             -------------               |   +-----------+
 *              |                ^     ^                  |   |           |
 *              |                |     |                  |   |           |
 *              v                |     |                  v   |           |
 *       -------------           |     |             -------------        |
 *       |           |   release |     |   start     |           |        |
 *       |   PAUSED  |-----------+     +-------------|  PREPARE  |<-------+
 *       |           |                               |           |
 *       -------------                               -------------
 *              |                                      ^     ^
 *              |               stop/xrun              |     |
 *              +--------------------------------------+     |
 *                                                           | prepare
 *                            -------------                  |
 *                            |           |                  |
 *                ----------->|   READY   |------------------+
 *                    reset   |           |
 *                            -------------
 *
 *
 */

#define COMP_STATE_INIT		0	/* component being initialised */
#define COMP_STATE_READY	1       /* component inactive, but ready */
#define COMP_STATE_SUSPEND	2       /* component suspended */
#define COMP_STATE_PREPARE	3	/* component prepared */
#define COMP_STATE_PAUSED	4	/* component paused */
#define COMP_STATE_ACTIVE	5	/* component active */

/*
 * standard component stream commands
 * TODO: use IPC versions after 1.1
 */

#define COMP_TRIGGER_STOP	0	/* stop component stream */
#define COMP_TRIGGER_START	1	/* start component stream */
#define COMP_TRIGGER_PAUSE	2	/* pause the component stream */
#define COMP_TRIGGER_RELEASE	3	/* release paused component stream */
#define COMP_TRIGGER_SUSPEND	4	/* suspend component */
#define COMP_TRIGGER_RESUME	5	/* resume component */
#define COMP_TRIGGER_RESET	6	/* reset component */
#define COMP_TRIGGER_PREPARE	7	/* prepare component */
#define COMP_TRIGGER_XRUN	8	/* XRUN component */

/*
 * standard component control commands
 */

#define COMP_CMD_SET_VALUE	100
#define COMP_CMD_GET_VALUE	101
#define COMP_CMD_SET_DATA	102
#define COMP_CMD_GET_DATA	103


/* MMAP IPC status */
#define COMP_CMD_IPC_MMAP_RPOS	200	/* host read position */
#define COMP_CMD_IPC_MMAP_PPOS	201	/* DAI presentation position */

#define COMP_CMD_IPC_MMAP_VOL(chan)	(216 + chan)	/* Volume */

/* component cache operations */

/* writeback and invalidate component data */
#define COMP_CACHE_WRITEBACK_INV	0
/* invalidate component data */
#define COMP_CACHE_INVALIDATE		1

/* component operations */
#define COMP_OPS_PARAMS		0
#define COMP_OPS_TRIGGER	1
#define COMP_OPS_PREPARE	2
#define COMP_OPS_COPY		3
#define COMP_OPS_BUFFER		4
#define COMP_OPS_RESET		5
#define COMP_OPS_CACHE		6

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

	/* set component audio stream parameters */
	int (*params)(struct comp_dev *dev);

	/* set component audio stream parameters */
	int (*dai_config)(struct comp_dev *dev,
		struct sof_ipc_dai_config *dai_config);

	/* used to pass standard and bespoke commands (with optional data) */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data);

	/* atomic - used to start/stop/pause stream operations */
	int (*trigger)(struct comp_dev *dev, int cmd);

	/* prepare component after params are set */
	int (*prepare)(struct comp_dev *dev);

	/* reset component */
	int (*reset)(struct comp_dev *dev);

	/* copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *dev);

	/* host buffer config */
	int (*host_buffer)(struct comp_dev *dev,
			   struct dma_sg_elem_array *elem_array,
			   uint32_t host_size);

	/* position */
	int (*position)(struct comp_dev *dev,
		struct sof_ipc_stream_posn *posn);

	/* cache operation on component data */
	void (*cache)(struct comp_dev *dev, int cmd);
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
	uint16_t state;			/* COMP_STATE_ */
	uint16_t is_endpoint;		/* component is end point in pipeline */
	uint16_t is_dma_connected;	/* component is connected to DMA */
	spinlock_t lock;		/* lock for this component */
	uint64_t position;		/* component rendering position */
	uint32_t frames;		/* number of frames we copy to sink */
	uint32_t frame_bytes;		/* frames size copied to sink in bytes */
	struct pipeline *pipeline;	/* pipeline we belong to */

	/* common runtime configuration for downstream/upstream */
	struct sof_ipc_stream_params params;

	/* driver */
	struct comp_driver *drv;

	/* lists */
	struct list_item bsource_list;	/* list of source buffers */
	struct list_item bsink_list;	/* list of sink buffers */

	/* private data - core does not touch this */
	void *private;		/* private data */

	/* IPC config object header - MUST be at end as it's variable size/type */
	struct sof_ipc_comp comp;
};

#define COMP_SIZE(x) \
	(sizeof(struct comp_dev) - sizeof(struct sof_ipc_comp) + sizeof(x))
#define COMP_GET_IPC(dev, type) \
	(struct type *)(&dev->comp)
#define COMP_GET_PARAMS(dev) \
	(struct type *)(&dev->params)
#define COMP_GET_CONFIG(dev) \
	(struct sof_ipc_comp_config *)((void*)&dev->comp + sizeof(struct sof_ipc_comp))

#define comp_set_drvdata(c, data) \
	c->private = data
#define comp_get_drvdata(c) \
	c->private;

typedef void (*cache_command)(void *, size_t);

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

/* component state set */
int comp_set_state(struct comp_dev *dev, int cmd);

/* component parameter init - mandatory */
static inline int comp_params(struct comp_dev *dev)
{
	return dev->drv->ops.params(dev);
}

/* component host buffer config
 * mandatory for host components, optional for the others.
 */
static inline int comp_host_buffer(struct comp_dev *dev,
		struct dma_sg_elem_array *elem_array, uint32_t host_size)
{
	if (dev->drv->ops.host_buffer)
		return dev->drv->ops.host_buffer(dev, elem_array, host_size);
	return 0;
}

/* send component command - mandatory */
static inline int comp_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct sof_ipc_ctrl_data *cdata = data;

	if ((cmd == COMP_CMD_SET_DATA)
		&& ((cdata->data->magic != SOF_ABI_MAGIC)
		|| (cdata->data->abi != SOF_ABI_VERSION))) {
		trace_comp_error("abi");
		trace_error_value(cdata->data->magic);
		trace_error_value(cdata->data->abi);
		return -EINVAL;
	}

	return dev->drv->ops.cmd(dev, cmd, data);
}

/* trigger component - mandatory and atomic */
static inline int comp_trigger(struct comp_dev *dev, int cmd)
{
	return dev->drv->ops.trigger(dev, cmd);
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
	struct sof_ipc_dai_config *config)
{
	if (dev->drv->ops.dai_config)
		return dev->drv->ops.dai_config(dev, config);
	return 0;
}

/* component rendering position */
static inline int comp_position(struct comp_dev *dev,
	struct sof_ipc_stream_posn *posn)
{
	if (dev->drv->ops.position)
		return dev->drv->ops.position(dev, posn);
	return 0;
}

/* component cache command */
static inline void comp_cache(struct comp_dev *dev, int cmd)
{
	if (dev->drv->ops.cache)
		dev->drv->ops.cache(dev, cmd);
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

/*
 * Convenience functions to install upstream/downstream common params. Only
 * applicable to single upstream source. Components with > 1 source  or sink
 * must do this manually.
 *
 * This allows params to propagate from the host PCM component downstream on
 * playback and upstream on capture.
 */
static inline void comp_install_params(struct comp_dev *dev,
	struct comp_dev *previous)
{
	dev->params = previous->params;
}

static inline uint32_t comp_frame_bytes(struct comp_dev *dev)
{
	/* calculate period size based on params */
	switch (dev->params.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return 2 * dev->params.channels;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		return 4 * dev->params.channels;
	default:
		return 0;
	}
}

static inline uint32_t comp_sample_bytes(struct comp_dev *dev)
{
	/* calculate period size based on params */
	switch (dev->params.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return 2;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		return 4;
	default:
		return 0;
	}
}

/* XRUN handling */
static inline void comp_underrun(struct comp_dev *dev, struct comp_buffer *source,
	uint32_t copy_bytes, uint32_t min_bytes)
{
	trace_comp("Xun");
	trace_value((dev->comp.id << 16) | source->avail);
	trace_value((min_bytes << 16) | copy_bytes);

	pipeline_xrun(dev->pipeline, dev, (int32_t)source->avail - copy_bytes);
}

static inline void comp_overrun(struct comp_dev *dev, struct comp_buffer *sink,
	uint32_t copy_bytes, uint32_t min_bytes)
{
	trace_comp("Xov");
	trace_value((dev->comp.id << 16) | sink->free);
	trace_value((min_bytes << 16) | copy_bytes);

	pipeline_xrun(dev->pipeline, dev, (int32_t)copy_bytes - sink->free);
}

static inline cache_command comp_get_cache_command(int cmd)
{
	switch (cmd) {
	case COMP_CACHE_WRITEBACK_INV:
		return &dcache_writeback_invalidate_region;
	case COMP_CACHE_INVALIDATE:
		return &dcache_invalidate_region;
	default:
		trace_comp_error("cu0");
		trace_error_value(cmd);
		return NULL;
	}
}

#endif
