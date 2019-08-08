// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>
#include <cmocka.h>

#include <sof/lib/alloc.h>
#include <sof/audio/component.h>

#include "comp_mock.h"

#include <mock_trace.h>

TRACE_IMPL()

void *_balloc(int zone, uint32_t caps, size_t bytes,
	      uint32_t alignment)
{
	(void)zone;
	(void)caps;

	return malloc(bytes);
}

void *_zalloc(int zone, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)caps;

	return calloc(bytes, 1);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *_brealloc(void *ptr, int zone, uint32_t caps, size_t bytes,
		uint32_t alignment)
{
	(void)zone;
	(void)caps;

	return realloc(ptr, bytes);
}

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

int comp_set_sink_buffer(struct comp_dev *dev, uint32_t period_bytes,
			 uint32_t periods)
{
	struct comp_buffer *sinkb;
	int ret = 0;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	if (!sinkb->sink->is_dma_connected) {
		ret = buffer_set_size(sinkb, period_bytes * periods);
		if (ret < 0)
			return ret;
	} else if (sinkb->size < period_bytes * periods) {
		ret = -EINVAL;
		return ret;
	}

	return ret;
}

int comp_set_state(struct comp_dev *dev, int cmd)
{
	return 0;
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	(void)p;
	(void)filename;
	(void)linenum;
}
