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

 /**
  * \file include/sof/audio/component.h
  * \brief Component API definition
  * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
  * \author Keyon Jie <yang.jie@linux.intel.com>
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
#include <sof/cache.h>
#include <uapi/ipc/control.h>
#include <uapi/ipc/stream.h>
#include <uapi/ipc/topology.h>
#include <uapi/ipc/dai.h>

/** \addtogroup component_api Component API
 *  Component API specification.
 *  @{
 */

/** \name Audio Component States
 *  @{
 */

/**
 * States may transform as below:-
 * \verbatim
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
 * \endverbatim
 */

#define COMP_STATE_INIT		0	/**< Component being initialised */
#define COMP_STATE_READY	1       /**< Component inactive, but ready */
#define COMP_STATE_SUSPEND	2       /**< Component suspended */
#define COMP_STATE_PREPARE	3	/**< Component prepared */
#define COMP_STATE_PAUSED	4	/**< Component paused */
#define COMP_STATE_ACTIVE	5	/**< Component active */
/** @}*/

/** \name Standard Component Stream Commands
 *  TODO: use IPC versions after 1.1
 *  @{
 */
#define COMP_TRIGGER_STOP	0	/**< Stop component stream */
#define COMP_TRIGGER_START	1	/**< Start component stream */
#define COMP_TRIGGER_PAUSE	2	/**< Pause the component stream */
#define COMP_TRIGGER_RELEASE	3	/**< Release paused component stream */
#define COMP_TRIGGER_SUSPEND	4	/**< Suspend component */
#define COMP_TRIGGER_RESUME	5	/**< Resume component */
#define COMP_TRIGGER_RESET	6	/**< Reset component */
#define COMP_TRIGGER_PREPARE	7	/**< Prepare component */
#define COMP_TRIGGER_XRUN	8	/**< XRUN component */
/** @}*/

/** \name Standard Component Control Commands
 *  "Value" commands are standard ones, known to the driver while
 *  "Data" commands are opaque blobs transferred by the driver.
 *
 *  TODO: see also: ref to ipc data structures for commands.
 *  @{
 */
#define COMP_CMD_SET_VALUE	100	/**< Set value to component */
#define COMP_CMD_GET_VALUE	101     /**< Get value from component */
#define COMP_CMD_SET_DATA	102     /**< Set data to component */
#define COMP_CMD_GET_DATA	103     /**< Get data from component */
#define COMP_CMD_SET_LARGE_DATA	104	/**< Set large data to component */
#define COMP_CMD_GET_LARGE_DATA	105	/**< Get large data from component */
/** @}*/

/** \name MMAP IPC status
 *  @{
 */
#define COMP_CMD_IPC_MMAP_RPOS	200	/**< Host read position */
#define COMP_CMD_IPC_MMAP_PPOS	201	/**< DAI presentation position */

#define COMP_CMD_IPC_MMAP_VOL(chan)	(216 + chan)	/**< Volume */
/** @}*/

/** \name Trace macros
 *  @{
 */
#define trace_comp(__e, ...)	trace_event(TRACE_CLASS_COMP, __e, ##__VA_ARGS__)
#define trace_comp_error(__e, ...)	trace_error(TRACE_CLASS_COMP, __e, ##__VA_ARGS__)
#define tracev_comp(__e, ...)	tracev_event(TRACE_CLASS_COMP, __e, ##__VA_ARGS__)
/** @}*/

struct comp_dev;
struct comp_buffer;
struct dai_config;
struct pipeline;

/**
 * Audio component operations - all mandatory.
 *
 * All component operations must return 0 for success, negative values for
 * errors and 1 to stop the pipeline walk operation.
 */
struct comp_ops {
	/** component creation */
	struct comp_dev *(*new)(struct sof_ipc_comp *comp);

	/** component destruction */
	void (*free)(struct comp_dev *dev);

	/** set component audio stream parameters */
	int (*params)(struct comp_dev *dev);

	/** set component audio stream parameters */
	int (*dai_config)(struct comp_dev *dev,
		struct sof_ipc_dai_config *dai_config);

	/** used to pass standard and bespoke commands (with optional data) */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size);

	/** atomic - used to start/stop/pause stream operations */
	int (*trigger)(struct comp_dev *dev, int cmd);

	/** prepare component after params are set */
	int (*prepare)(struct comp_dev *dev);

	/** reset component */
	int (*reset)(struct comp_dev *dev);

	/** copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *dev);

	/** host buffer config */
	int (*host_buffer)(struct comp_dev *dev,
			   struct dma_sg_elem_array *elem_array,
			   uint32_t host_size);

	/** position */
	int (*position)(struct comp_dev *dev,
		struct sof_ipc_stream_posn *posn);

	/** cache operation on component data */
	void (*cache)(struct comp_dev *dev, int cmd);
};


/**
 * Audio component base driver "class"
 * - used by all other component types.
 */
struct comp_driver {
	uint32_t type;		/**< SOF_COMP_ for driver */
	uint32_t module_id;	/**< module id */

	struct comp_ops ops;	/**< component operations */

	struct list_item list;	/**< list of component drivers */
};	

/**
 * Audio component base device "class"
 * - used by other component types.
 */
struct comp_dev {

	/* runtime */
	uint16_t state;		   /**< COMP_STATE_ */
	uint16_t is_dma_connected; /**< component is connected to DMA */
	spinlock_t lock;	   /**< lock for this component */
	uint64_t position;	   /**< component rendering position */
	uint32_t frames;	   /**< number of frames we copy to sink */
	uint32_t frame_bytes;	   /**< frames size copied to sink in bytes */
	struct pipeline *pipeline; /**< pipeline we belong to */

	/** common runtime configuration for downstream/upstream */
	struct sof_ipc_stream_params params;

	/** driver */
	struct comp_driver *drv;

	/* lists */
	struct list_item bsource_list;	/**< list of source buffers */
	struct list_item bsink_list;	/**< list of sink buffers */

	/* private data - core does not touch this */
	void *private;		/**< private data */

	/**
	 * IPC config object header - MUST be at end as it's
	 * variable size/type
	 */
	struct sof_ipc_comp comp;
};

/** \name Helpers.
 *  @{
 */

/** \brief Computes size of the component device including ipc config. */
#define COMP_SIZE(x) \
	(sizeof(struct comp_dev) - sizeof(struct sof_ipc_comp) + sizeof(x))

/** \brief Retrieves component device IPC configuration. */
#define COMP_GET_IPC(dev, type) \
	(struct type *)(&dev->comp)

/** \brief Retrieves component device runtime configuration. */
#define COMP_GET_PARAMS(dev) \
	(struct type *)(&dev->params)

/** \brief Retrieves component device config data. */
#define COMP_GET_CONFIG(dev) \
	(struct sof_ipc_comp_config *)((void*)&dev->comp + sizeof(struct sof_ipc_comp))

/** \brief Sets the driver private data. */
#define comp_set_drvdata(c, data) \
	c->private = data

/** \brief Retrieves the driver private data. */
#define comp_get_drvdata(c) \
	c->private;

/** \brief Retrieves the component device buffer list. */
#define comp_buffer_list(comp, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &comp->bsink_list : \
	 &comp->bsource_list)

/** @}*/

/** \name Component registration
 *  @{
 */

/**
 * Registers the component driver on the list of available components.
 * @param drv Component driver to be registered.
 * @return 0 if succeeded, error code otherwise.
 */
int comp_register(struct comp_driver *drv);

/**
 * Unregisters the component driver from the list of available components.
 * @param drv Component driver to be unregistered.
 */
void comp_unregister(struct comp_driver *drv);

/** @}*/

/** \name Component creation and destruction - mandatory
 *  @{
 */

/**
 * Creates a new component device.
 * @param comp Parameters of the new component device.
 * @return Pointer to the new component device.
 */
struct comp_dev *comp_new(struct sof_ipc_comp *comp);

/**
 * Called to delete the specified component device.
 * All data structures previously allocated on the run-time heap
 * must be freed by the implementation of <code>free()</code>.
 * @param dev Component device to be deleted.
 */
static inline void comp_free(struct comp_dev *dev)
{
	dev->drv->ops.free(dev);
}

/** @}*/

/** \name Component API.
 *  @{
 */

/**
 * Component state set.
 * @param dev Component device.
 * @param cmd Command, one of <i>COMP_TRIGGER_...</i>.
 * @return 0 if succeeded, error code otherwise.
 */
int comp_set_state(struct comp_dev *dev, int cmd);

/**
 * Component set period bytes
 * @param dev Component device.
 * @param frames Number of frames.
 * @param format Frame format.
 * @param period_bytes Size of a single period in bytes.
 */
void comp_set_period_bytes(struct comp_dev *dev, uint32_t frames,
			   enum sof_ipc_frame *format, uint32_t *period_bytes);

/**
 * Component parameter init - mandatory.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_params(struct comp_dev *dev)
{
	return dev->drv->ops.params(dev);
}

/**
 * Component host buffer config.
 * Mandatory for host components, optional for the others.
 * @param dev Component device.
 * @param elem_array SG element array.
 * @param host_size Host ring buffer size.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_host_buffer(struct comp_dev *dev,
		struct dma_sg_elem_array *elem_array, uint32_t host_size)
{
	if (dev->drv->ops.host_buffer)
		return dev->drv->ops.host_buffer(dev, elem_array, host_size);
	return 0;
}

/**
 * Send component command (mandatory).
 * @param dev Component device.
 * @param cmd Command.
 * @param data Command data.
 * @param max_data_size Max data size.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_cmd(struct comp_dev *dev, int cmd, void *data,
			   int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	if ((cmd == COMP_CMD_SET_DATA)
		&& ((cdata->data->magic != SOF_ABI_MAGIC)
		|| (cdata->data->abi != SOF_ABI_VERSION))) {
		trace_comp_error("comp_cmd() error: invalid version, "
				 "data->magic = %u, data->abi = %u",
				 cdata->data->magic, cdata->data->abi);
		return -EINVAL;
	}

	return dev->drv->ops.cmd(dev, cmd, data, max_data_size);
}

/**
 * Trigger component - mandatory and atomic.
 * @param dev Component device.
 * @param cmd Command.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_trigger(struct comp_dev *dev, int cmd)
{
	return dev->drv->ops.trigger(dev, cmd);
}

/**
 * Prepare component - mandatory.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_prepare(struct comp_dev *dev)
{
	return dev->drv->ops.prepare(dev);
}

/**
 * Copy component buffers - mandatory.
 * @param dev Component device.
 * @return Number of frames copied.
 */
static inline int comp_copy(struct comp_dev *dev)
{
	return dev->drv->ops.copy(dev);
}

/**
 * Component reset and free runtime resources -mandatory.
 * @param dev Component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_reset(struct comp_dev *dev)
{
	return dev->drv->ops.reset(dev);
}

/**
 * DAI configuration - only mandatory for DAI components.
 * @param dev Component device.
 * @param config DAI configuration.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_dai_config(struct comp_dev *dev,
	struct sof_ipc_dai_config *config)
{
	if (dev->drv->ops.dai_config)
		return dev->drv->ops.dai_config(dev, config);
	return 0;
}

/**
 * Retrieves component rendering position.
 * @param dev Component device.
 * @param posn Position reported by the component device.
 * @return 0 if succeeded, error code otherwise.
 */
static inline int comp_position(struct comp_dev *dev,
	struct sof_ipc_stream_posn *posn)
{
	if (dev->drv->ops.position)
		return dev->drv->ops.position(dev, posn);
	return 0;
}

/**
 * Component L1 cache command (invalidate, writeback, ...).
 * @param dev Component device
 * @param cmd Command.
 */
static inline void comp_cache(struct comp_dev *dev, int cmd)
{
	if (dev->drv->ops.cache)
		dev->drv->ops.cache(dev, cmd);
}

/** @}*/

/** \name Components API for infrastructure.
 *  @{
 */

/**
 * Allocates and initializes audio component list.
 * To be called once at boot time.
 */
void sys_comp_init(void);

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

/** @}*/

/** \name Helpers.
 *  @{
 */

/**
 * Checks if two component devices belong to the same parent pipeline.
 * @param current Component device.
 * @param previous Another component device.
 * @return 1 if children of the same pipeline, 0 otherwise.
 */
static inline int comp_is_single_pipeline(struct comp_dev *current,
					  struct comp_dev *previous)
{
	return current->comp.pipeline_id == previous->comp.pipeline_id;
}

/**
 * Checks if component device is active.
 * @param current Component device.
 * @return 1 if active, 0 otherwise.
 */
static inline int comp_is_active(struct comp_dev *current)
{
	return current->state == COMP_STATE_ACTIVE;
}

/**
 * Calculates period size in bytes based on component device's parameters.
 * @param dev Component device.
 * @return Period size in bytes.
 */
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

/**
 * Calculates sample size in bytes based on component device's parameters.
 * @param dev Component device.
 * @return Size of sample in bytes.
 */
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

/** @}*/

/** \name XRUN handling.
 *  @{
 */

/**
 * Called by the component device when underrun is detected.
 * @param dev Component device.
 * @param source Source buffer.
 * @param copy_bytes Requested size of data to be available.
 * @param min_bytes
 */
static inline void comp_underrun(struct comp_dev *dev, struct comp_buffer *source,
	uint32_t copy_bytes, uint32_t min_bytes)
{
	trace_comp("comp_underrun(), ((dev->comp.id << 16) | source->avail) ="
		   " %u, ((min_bytes << 16) | copy_bytes) = %u",
		  (dev->comp.id << 16) | source->avail,
		  (min_bytes << 16) | copy_bytes);

	pipeline_xrun(dev->pipeline, dev, (int32_t)source->avail - copy_bytes);
}

/**
 * Called by component device when overrun is detected.
 * @param dev Component device.
 * @param sink Sink buffer.
 * @param copy_bytes Requested size of free space to be available.
 * @param min_bytes
 */
static inline void comp_overrun(struct comp_dev *dev, struct comp_buffer *sink,
	uint32_t copy_bytes, uint32_t min_bytes)
{
	trace_comp("comp_overrun(), ((dev->comp.id << 16) | sink->free) = %u, "
		   "((min_bytes << 16) | copy_bytes) = %u",
		  (dev->comp.id << 16) | sink->free,
		  (min_bytes << 16) | copy_bytes);

	pipeline_xrun(dev->pipeline, dev, (int32_t)copy_bytes - sink->free);
}

/** @}*/

/** @}*/

#endif
