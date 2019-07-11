// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file lib/pm_runtime.c
 * \brief Runtime power management implementation
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/pm_runtime.h>
#include <sof/alloc.h>

/** \brief Runtime power management data pointer. */
static struct pm_runtime_data *prd;

void pm_runtime_init(void)
{
	trace_pm("pm_runtime_init()");

	prd = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*prd));
	spinlock_init(&prd->lock);

	platform_pm_runtime_init(prd);
}

void pm_runtime_get(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_get()");

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_get_sync()");

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, 0);
		break;
	}
}

void pm_runtime_put(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_put()");

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_put_sync()");

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, 0);
		break;
	}
}
