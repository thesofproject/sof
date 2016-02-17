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
