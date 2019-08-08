// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>
#include <cmocka.h>

#include <sof/lib/alloc.h>
#include <sof/lib/notifier.h>
#include <sof/audio/component.h>
#include <sof/drivers/timer.h>
#include <sof/schedule/schedule.h>
#include <mock_trace.h>
#include <sof/lib/clk.h>

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

int comp_set_state(struct comp_dev *dev, int cmd)
{
	return 0;
}

void notifier_register(struct notifier *notifier)
{
}

void notifier_unregister(struct notifier *notifier)
{
}

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       enum task_state (*run)(void *data),
		       void (*complete)(void *data), void *data, uint16_t core,
		       uint32_t xflags)
{
	return 0;
}

void __panic(uint32_t p, char *filename, uint32_t linenum)
{
	(void)p;
	(void)filename;
	(void)linenum;
}

uint64_t platform_timer_get(struct timer *timer)
{
	(void)timer;

	return 0;
}

uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	(void)clock;
	(void)ms;

	return 0;
}

struct schedulers **arch_schedulers_get(void)
{
	return NULL;
}
