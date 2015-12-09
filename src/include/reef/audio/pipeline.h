/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_PIPELINE__
#define __INCLUDE_AUDIO_PIPELINE__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/audio/endpoint.h>

struct pipeline {
	uint16_t id;		/* id */
	uint16_t state;		/* PIPELINE_STATE_ */ 
	spinlock_t lock;

	/* lists */
	struct list_head endpoint_list;		/* list of endpoints */
	struct list_head comp_list;		/* list of components */
	struct list_head buffer_list;		/* list of buffers */
};

/* create new pipeline - returns pipeline id */
int pipeline_new(void);
void pipeline_free(int pipeline_id);

/* insert component in pipeline - returns new comp id */
int pipeline_comp_insert(int pipeline_id, int comp_type, int comp_source_id,
	int comp_sink_id);

#endif
