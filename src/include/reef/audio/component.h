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

#define AUDIO_COMP_STATE_UNAVAIL	0	/* not ready for stream ops */
#define AUDIO_COMP_STATE_INACTIVE	1	/* ready for ops, but not used */
#define AUDIO_COMP_STATE_ACTIVE		2	/* read and in use for stream */

struct comp_object;
struct comp_buffer;

struct comp_ops {
	/* ops */
	int (*new)(struct comp_object *c);
	int (*free)(struct comp_object *c);
	int (*params)(struct comp_object *c, struct stream_params *params);
	int (*cmd)(struct comp_object *c, int cmd, void *data);
	int (*copy)(struct comp_object *sink, struct comp_object *source);
};

struct comp_object {
	/* runtime */
	uint16_t id;		/* UUID for component */
	uint8_t index;		/* index of component */
	uint8_t state;		/* AUDIO_COMP_ */
	spinlock_t lock;
	void *private;		/* private data */

	/* ops */
	struct comp_ops ops;

	/* lists */
	struct list_head bsource_list;	/* list of source buffers */
	struct list_head bsink_list;	/* list of sink buffers */
	struct list_head pipeline_list;	/* list of pipeline components */
};

struct comp_buffer {
	/* runtime data */
	uint32_t size;		/* size of buffer in bytes */
	uint8_t *w_ptr;		/* buffer write pointer */
	uint8_t *r_ptr;		/* buffer read position */
	uint8_t *addr;		/* buffer base address */
	uint32_t avail;		/* available bytes between R and W ptrs */
	uint8_t connected;	/* connected in path */

	/* connected components */
	struct comp_object *source;	/* source component */
	struct comp_object *sink;	/* sink component */

	/* lists */
	struct list_head pipeline_list;	/* list of pipeline buffers */
};	

#endif
