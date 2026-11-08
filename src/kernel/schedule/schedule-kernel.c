// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */

#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/cold.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>
#include <ipc4/base_fw.h>

LOG_MODULE_DECLARE(schedule, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx sch_tr;

/* Kernel part of the generic scheduler: scheduler registration. The rest is in
 * src/schedule/schedule.c
 */

__cold static void scheduler_register(struct schedule_data *scheduler)
{
	struct schedulers **sch = arch_schedulers_get();
	struct k_heap *heap = NULL;

	assert_can_be_cold();

	if (IS_ENABLED(CONFIG_SOF_USERSPACE_LL) && scheduler_is_user(scheduler->type)) {
		sch = arch_user_schedulers_get();
		heap = sof_sys_user_heap_get();
	}

	if (!*sch) {
		/* init schedulers list */
		*sch = sof_heap_alloc(heap, 0, sizeof(**sch), 0);
		if (!*sch) {
			tr_err(&sch_tr, "allocation failed");
			return;
		}
		memset(*sch, 0, sizeof(**sch));
		list_init(&(*sch)->list);
	}

	list_item_append(&scheduler->list, &(*sch)->list);
}

__cold void scheduler_init(int type, const struct scheduler_ops *ops, void *data)
{
	struct schedule_data *sch;
	struct k_heap *heap = NULL;

	assert_can_be_cold();

	if (IS_ENABLED(CONFIG_SOF_USERSPACE_LL) && scheduler_is_user(type))
		heap = sof_sys_user_heap_get();

	if (!ops || !ops->schedule_task || !ops->schedule_task_cancel ||
	    !ops->schedule_task_free)
		return;

	sch = sof_heap_alloc(heap, SOF_MEM_FLAG_KERNEL, sizeof(*sch), 0);
	if (!sch) {
		tr_err(&sch_tr, "allocation failed");
		sof_panic(SOF_IPC_PANIC_IPC);
	}
	memset(sch, 0, sizeof(*sch));
	list_init(&sch->list);
	sch->type = type;
	sch->ops = ops;
	sch->data = data;

	scheduler_register(sch);
}
