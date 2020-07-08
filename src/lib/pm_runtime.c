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
#include <sof/lib/uuid.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <stdint.h>

/* d7f6712d-131c-45a7-82ed-6aa9dc2291ea */
DECLARE_SOF_UUID("pm-runtime", pm_runtime_uuid, 0xd7f6712d, 0x131c, 0x45a7,
		 0x82, 0xed, 0x6a, 0xa9, 0xdc, 0x22, 0x91, 0xea);

DECLARE_TR_CTX(pm_tr, SOF_UUID(pm_runtime_uuid), LOG_LEVEL_INFO);

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
	tr_dbg(&pm_tr, "pm_runtime_get() context %d index %d", context, index);

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_get_sync() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, 0);
		break;
	}
}

void pm_runtime_put(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_put() context %d index %d", context, index);

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_put_sync() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, 0);
		break;
	}
}

void pm_runtime_enable(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_enable() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_enable(context, index);
		break;
	}
}

void pm_runtime_disable(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_disable() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_disable(context, index);
		break;
	}
}

bool pm_runtime_is_active(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_is_active() context %d index %d", context,
	       index);

	switch (context) {
	default:
		return platform_pm_runtime_is_active(context, index);
	}
}
