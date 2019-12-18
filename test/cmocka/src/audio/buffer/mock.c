// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <stdint.h>
#include <stdlib.h>

#include <config.h>
#include <sof/lib/alloc.h>
#include <sof/trace/trace.h>

#include <mock_trace.h>

TRACE_IMPL()

#if !CONFIG_LIBRARY

void *_zalloc(int zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;

	return malloc(bytes);
}

void *_balloc(uint32_t flags, uint32_t caps, size_t bytes,
	      uint32_t alignment)
{
	(void)flags;
	(void)caps;

	return malloc(bytes);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *_brealloc(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		uint32_t alignment)
{
	(void)flags;
	(void)caps;

	return realloc(ptr, bytes);
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	(void)p;
	(void)filename;
	(void)linenum;
}

#endif
