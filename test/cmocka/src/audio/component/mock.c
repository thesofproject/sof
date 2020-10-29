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

static struct sof sof;

struct tr_ctx buffer_tr;

#if !CONFIG_LIBRARY

void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;

	return calloc(bytes, 1);
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

uint32_t crc32(uint32_t base, const void *data, uint32_t bytes)
{
	return 0;
}

struct sof *sof_get(void)
{
	return &sof;
}

int buffer_set_params(struct comp_buffer *buffer, struct sof_ipc_stream_params *params,
		      bool force_update)
{
	return 0;
}

#endif
