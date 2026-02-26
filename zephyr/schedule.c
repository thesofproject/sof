// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */
#include <sof/schedule/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <rtos/alloc.h>
#include <rtos/symbol.h>
#include <rtos/userspace_helper.h>
#include <sof/lib/cpu.h>
#include <ipc/topology.h>

static APP_TASK_BSS struct schedulers *_schedulers[CONFIG_CORE_COUNT];

/**
 * Retrieves registered schedulers.
 * @return List of registered schedulers.
 */
struct schedulers **arch_schedulers_get(void)
{
	if (k_is_user_context()) {
		printk("FIXME: using core0 scheduler\n");
		return _schedulers;
	}

	return _schedulers + cpu_get_id();
}
EXPORT_SYMBOL(arch_schedulers_get);
