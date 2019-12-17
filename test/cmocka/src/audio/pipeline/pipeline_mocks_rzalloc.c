// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule/edf_schedule.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>
#include <cmocka.h>
#include <sof/lib/alloc.h>

void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)flags;
	(void)caps;
	return calloc(bytes, 1);
}
