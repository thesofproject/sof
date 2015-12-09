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
#include <reef/stream.h>

/* component 32bit UUID - type is COMP_TYPE_ or bespoke type */
#define COMP_UUID(vendor, type)	\
	(((vendor & 0xffff) << 16) | (type & 0xffff))
#define COMP_VENDOR_GENERIC	0
#define COMP_VENDOR_INTEL	0x8086

/* audio component states */
#define COMP_STATE_UNAVAIL	0	/* not ready for COMP ops */
#define COMP_STATE_INACTIVE	1	/* ready for ops, but not used */
#define COMP_STATE_ACTIVE	2	/* read and in use for COMP */

/* standard audio component types */
#define COMP_TYPE_EP_HOST	0	/* host endpoint */
#define COMP_TYPE_EP_DAI	1	/* DAI endpoint */
#define COMP_TYPE_VOLUME	2	/* Volume component */
#define COMP_TYPE_MIXER		3	/* Mixer component */
#define COMP_TYPE_MUX		4	/* MUX component */
#define COMP_TYPE_SWITCH	5	/* Switch component */

/* standard component commands */
#define COMP_CMD_STOP		0
#define COMP_CMD_START		1
#define COMP_CMD_PAUSE		2
#define COMP_CMD_RELEASE	3
#define COMP_CMD_DRAIN		4
#define COMP_CMD_VOLUME		32
#define COMP_CMD_MUTE		33
#define COMP_CMD_ROUTE		64

struct comp_dev;
struct comp_buffer;

/* audio component operations - all mandatory */
struct comp_ops {
	/* component creation and destruction */
	int (*new)(struct comp_dev *dev);
	void (*free)(struct comp_dev *dev);

	/* set component audio stream paramters */
	int (*params)(struct comp_dev *dev, struct stream_params *params);

	/* used to pass standard and bespoke commands (with data) to component */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data);

	/* copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *sink, struct comp_dev *source);
};

/* audio component base driver "class" - used by all other component types */
struct comp_driver {
	uint32_t uuid;		/* UUID for component */

	/* ops */
	struct comp_ops ops;	/* component operations */
};

/* audio component base device "class" - used by other component types */
struct comp_dev {

	/* runtime */
	uint8_t index;		/* index of component */
	uint8_t state;		/* COMP_STATE_ */
	spinlock_t lock;	/* lock for this component */
	void *private;		/* private data */
	struct comp_driver *drv;

	/* lists */
	struct list_head bsource_list;	/* list of source buffers */
	struct list_head bsink_list;	/* list of sink buffers */
	struct list_head pipeline_list;	/* list of pipeline components */
};

/* audio component buffer - connects 2 audio components togetehr in pipeline */
struct comp_buffer {
	/* runtime data */
	uint32_t size;		/* size of buffer in bytes */
	uint8_t *w_ptr;		/* buffer write pointer */
	uint8_t *r_ptr;		/* buffer read position */
	uint8_t *addr;		/* buffer base address */
	uint32_t avail;		/* available bytes between R and W ptrs */
	uint8_t connected;	/* connected in path */

	/* connected components */
	struct comp_dev *source;	/* source component */
	struct comp_dev *sink;	/* sink component */

	/* lists */
	struct list_head pipeline_list;	/* list of pipeline buffers */
};	

#endif
