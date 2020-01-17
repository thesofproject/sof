// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>

#include <mock_trace.h>

#include <sof/audio/component.h>
#include <sof/lib/alloc.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

TRACE_IMPL()

static struct sof sof;

void rfree(void *ptr)
{
	free(ptr);
}

void *_zalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;
	return calloc(bytes, 1);
}

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes)
{
}

void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes)
{
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	(void)p;
	(void)filename;
	(void)linenum;

	abort();
}

struct sof *sof_get(void)
{
	return &sof;
}
