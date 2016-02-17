/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/ipc.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

static struct ipc *_ipc;

struct ipc_comp_dev *ipc_get_comp(int pipeline_id, uint32_t id)
{
	struct ipc_comp_dev *cd;
	struct list_head *plist;

	list_for_each(plist, &_ipc->comp_list) {

		cd = container_of(plist, struct ipc_comp_dev, list);

		if (cd->desc.id == id && cd->pipeline_id == pipeline_id)
			return cd;
	}

	return NULL;
}

int ipc_comp_new(int pipeline_id, struct comp_desc *desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);

	if (p == NULL)
		return -EINVAL;

	return pipeline_comp_new(p, desc);
}

int ipc_comp_free(int pipeline_id, struct comp_desc *desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);

	if (p == NULL)
		return -EINVAL;

	return pipeline_comp_free(p, desc);
}

int ipc_buffer_new(int pipeline_id, struct buffer_desc *desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);

	if (p == NULL)
		return -EINVAL;

	return pipeline_buffer_new(p, desc);
}

int ipc_buffer_free(int pipeline_id, struct buffer_desc *desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);

	if (p == NULL)
		return -EINVAL;

	return pipeline_buffer_free(p, desc);
}

int ipc_comp_connect(int pipeline_id, struct comp_desc *source_desc,
	struct comp_desc *sink_desc, struct buffer_desc *buffer_desc)
{
	struct pipeline *p = pipeline_from_id(pipeline_id);

	if (p == NULL)
		return -EINVAL;

	return pipeline_comp_connect(p, source_desc, sink_desc, buffer_desc);
}

int ipc_init(void)
{
	/* init ipc data */
	_ipc = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*_ipc));
	list_init(&_ipc->comp_list);
	_ipc->host_pending = 0;

	return platform_ipc_init(_ipc);
}
