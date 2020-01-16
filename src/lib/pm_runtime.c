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

#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <stdint.h>

void pm_runtime_init(struct sof *sof)
{
	sof->prd = rzalloc(SOF_MEM_ZONE_SYS, SOF_MEM_FLAG_SHARED,
			   SOF_MEM_CAPS_RAM, sizeof(*sof->prd));
	spinlock_init(&sof->prd->lock);

	platform_pm_runtime_init(sof->prd);

	platform_shared_commit(sof->prd, sizeof(*sof->prd));
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

void pm_runtime_enable(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_enable()");

	switch (context) {
	default:
		platform_pm_runtime_enable(context, index);
		break;
	}
}

void pm_runtime_disable(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_disable()");

	switch (context) {
	default:
		platform_pm_runtime_disable(context, index);
		break;
	}
}

bool pm_runtime_is_active(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_is_active()");

	switch (context) {
	default:
		return platform_pm_runtime_is_active(context, index);
	}
}
