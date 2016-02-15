/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_COMPONENT__
#define __INCLUDE_AUDIO_COMPONENT__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/dma.h>
#include <reef/stream.h>

/* component 32bit UUID - type is COMP_TYPE_ or bespoke type */
#define COMP_UUID(vendor, type)	\
	(((vendor & 0xffff) << 16) | (type & 0xffff))
#define COMP_TYPE(uuid) (uuid & 0xffff)

/* component vendors */
#define COMP_VENDOR_GENERIC	0		/* community */
#define COMP_VENDOR_INTEL	0x8086

/* audio component states */
#define COMP_STATE_UNAVAIL	0	/* not ready for COMP ops */
#define COMP_STATE_IDLE		1	/* ready for ops, but not prepared */
#define COMP_STATE_PREPARED	2	/* prepared for stream use */
#define COMP_STATE_ACTIVE	3	/* actively in use by stream */

/* standard audio component types */
#define COMP_TYPE_HOST		0	/* host endpoint */
#define COMP_TYPE_VOLUME	1	/* Volume component */
#define COMP_TYPE_MIXER		2	/* Mixer component */
#define COMP_TYPE_MUX		3	/* MUX component */
#define COMP_TYPE_SWITCH	4	/* Switch component */
#define COMP_TYPE_DAI_SSP	5	/* SSP DAI endpoint */
#define COMP_TYPE_DAI_HDA	6	/* SSP DAI endpoint */

/* standard component commands */
#define COMP_CMD_VOLUME		1
#define COMP_CMD_MUTE		2
#define COMP_CMD_UNMUTE		3
#define COMP_CMD_ROUTE		4

/* component operations */
#define COMP_OPS_PARAMS		0
#define COMP_OPS_CMD		1
#define COMP_OPS_PREPARE	2
#define COMP_OPS_COPY		3
#define COMP_OPS_BUFFER		4
#define COMP_OPS_RESET		5

/* standard component command structures */
struct comp_volume {
	uint16_t volume[STREAM_MAX_CHANNELS];
};

struct comp_dev;
struct comp_buffer;

/*
 * Pipeline component descriptor.
 */
struct comp_desc {
	uint32_t uuid;
	uint16_t id;
	uint8_t is_ep_playback;	/* is endpoint playback or capture */
	uint8_t reserved;
};

/*
 * Componnent period descriptors
 */
struct period_desc {
	uint32_t size;	/* period size in bytes */
	uint16_t number;
	uint8_t no_irq;	/* dont send periodic IRQ to host/DSP */
	uint8_t reserved;
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
	struct comp_dev *(*new)(struct comp_desc *desc);
	void (*free)(struct comp_dev *dev);

	/* set component audio stream paramters */
	int (*params)(struct comp_dev *dev, struct stream_params *params);

	/* used to pass standard and bespoke commands (with data) to component */
	int (*cmd)(struct comp_dev *dev, struct stream_params *params, 
		int cmd, void *data);

	/* prepare component after params are set */
	int (*prepare)(struct comp_dev *dev, struct stream_params *params);

	/* reset component */
	int (*reset)(struct comp_dev *dev, struct stream_params *params);

	/* copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *dev, struct stream_params *params);

	/* host buffer config */
	int (*host_buffer)(struct comp_dev *dev, struct stream_params *params,
		struct dma_sg_elem *elem);
};

/* component buffer data capabilities */
struct comp_buffer_caps {
	uint32_t formats;
	uint32_t min_rate;
	uint32_t max_rate;
	uint16_t min_channels;
	uint16_t max_channels;
};

/* component capabilities */
struct comp_caps {
	struct comp_buffer_caps sink;
	struct comp_buffer_caps source;
};

/* audio component base driver "class" - used by all other component types */
struct comp_driver {
	uint32_t uuid;		/* UUID for component */
	uint32_t module_id;

	struct comp_ops ops;	/* component operations */
	struct comp_caps caps;	/* component capabilities */

	struct list_head list;	/* list of component drivers */
};	

/* audio component base device "class" - used by other component types */
struct comp_dev {

	/* runtime */
	uint8_t id;		/* id of component */
	uint8_t state;		/* COMP_STATE_ */
	uint8_t is_endpoint;	/* is this component an endpoint ? */
	uint8_t is_playback;	/* is the endpoint for playback or capture */
	spinlock_t lock;	/* lock for this component */
	void *private;		/* private data */
	struct comp_driver *drv;

	/* lists */
	struct list_head bsource_list;	/* list of source buffers */
	struct list_head bsink_list;	/* list of sink buffers */
	struct list_head pipeline_list;	/* list in pipeline component devs */
	struct list_head endpoint_list;	/* list in pipeline endpoint devs */
};

/* audio component buffer - connects 2 audio components together in pipeline */
struct comp_buffer {
	struct buffer_desc desc;
	struct stream_params params;

	/* runtime data */
	uint32_t avail;		/* available bytes between R and W ptrs */
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */
	uint8_t connected;	/* connected in path */

	/* connected components */
	struct comp_dev *source;	/* source component */
	struct comp_dev *sink;		/* sink component */

	/* lists */
	struct list_head pipeline_list;	/* list in pipeline buffers */
	struct list_head source_list;	/* list in comp buffers */
	struct list_head sink_list;	/* list in comp buffers */
};

#define comp_set_drvdata(c, data) \
	c->private = data
#define comp_get_drvdata(c) \
	c->private;

void sys_comp_init(void);

/* component registration */
int comp_register(struct comp_driver *drv);
void comp_unregister(struct comp_driver *drv);

/* component creation and destruction */
struct comp_dev *comp_new(struct comp_desc *desc);
static inline void comp_free(struct comp_dev *dev)
{
	dev->drv->ops.free(dev);
}

/* component parameter init */
static inline int comp_params(struct comp_dev *dev,
	struct stream_params *params)
{
	return dev->drv->ops.params(dev, params);
}

/* component host buffer config */
static inline int comp_host_buffer(struct comp_dev *dev,
	struct stream_params *params, struct dma_sg_elem *elem)
{
	return dev->drv->ops.host_buffer(dev, params, elem);
}

/* send component command */
static inline int comp_cmd(struct comp_dev *dev, struct stream_params *params,
	int cmd, void *data)
{
	return dev->drv->ops.cmd(dev, params, cmd, data);
}

/* prepare component */
static inline int comp_prepare(struct comp_dev *dev,
	struct stream_params *params)
{
	return dev->drv->ops.prepare(dev, params);
}

/* copy component buffers */
static inline int comp_copy(struct comp_dev *dev, struct stream_params *params)
{
	return dev->drv->ops.copy(dev, params);
}

/* component reset and free runtime resources */
static inline int comp_reset(struct comp_dev *dev,
	struct stream_params *params)
{
	return dev->drv->ops.reset(dev, params);
}

/* default base component initialisations */
void sys_comp_dai_init(void);
void sys_comp_host_init(void);
void sys_comp_mixer_init(void);
void sys_comp_mux_init(void);
void sys_comp_switch_init(void);
void sys_comp_volume_init(void);

static inline void comp_update_avail(struct comp_buffer *buffer)
{
	if (buffer->r_ptr <= buffer->w_ptr)
		buffer->avail = buffer->r_ptr - buffer->w_ptr;
	else
		buffer->avail = buffer->end_addr - buffer->w_ptr +
			buffer->r_ptr - buffer->addr;
}

#endif
