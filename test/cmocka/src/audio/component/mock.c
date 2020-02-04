// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <stdint.h>
#include <stdlib.h>

#include <sof/lib/alloc.h>
#include <sof/trace/trace.h>
#include <sof/audio/component.h>

#include <mock_trace.h>

TRACE_IMPL()

static struct sof sof;

#if !CONFIG_LIBRARY

void *_zalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;

	return calloc(bytes, 1);
}

void *_realloc(void *ptr, enum mem_zone zone, uint32_t flags, uint32_t caps,
	       size_t bytes)
{
	(void)ptr;
	(void)zone;
	(void)flags;
	(void)caps;

	return realloc(ptr, bytes);
}

int buffer_set_size(struct comp_buffer *buffer, uint32_t size)
{
	(void)buffer;
	(void)size;

	return 0;
}

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

struct sof *sof_get(void)
{
	return &sof;
}

#endif
