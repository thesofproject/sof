// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */

#include <rtos/alloc.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>
#include <ipc4/base_fw.h>

LOG_MODULE_REGISTER(schedule, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(schedule);

DECLARE_TR_CTX(sch_tr, SOF_UUID(schedule_uuid), LOG_LEVEL_INFO);

static inline bool scheduler_is_user(int type)
{
	/*
	 * currently only LL managed in user-space, but longterm
	 * goal is to move all audio application level scheduling
	 * to user-space and only keep Zephyr scheduler logic in
	 * kernel
	 */
	return type == SOF_SCHEDULE_LL_TIMER || type == SOF_SCHEDULE_DP;
}

int schedule_task_init(struct task *task,
		       const struct sof_uuid_entry *uid, uint16_t type,
		       uint16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags)
{
	struct schedulers *schedulers;
	struct schedule_data *sch = NULL;
	struct list_item *slist;

	if (type >= SOF_SCHEDULE_COUNT) {
		tr_err(&sch_tr, "invalid task type");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_SOF_USERSPACE_LL) && scheduler_is_user(type))
		schedulers = *arch_user_schedulers_get_for_core(core);
	else
		schedulers = *arch_schedulers_get();

	if (!schedulers)
		return -ENODEV;

	task->sch = NULL;
	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (type == sch->type) {
			task->sch = sch;
			break;
		}
	}

	if (!task->sch) {
		tr_err(&sch_tr, "unable to bind scheduler to task %p (type %d)", task, type);
		return -ENODEV;
	}

	task->uid = uid;
	task->type = type;
	task->priority = priority;
	task->core = core;
	task->flags = flags;
	task->state = SOF_TASK_STATE_INIT;
	task->ops.run = run;
	task->data = data;

	return 0;
}

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

/* Locks for the list here should be held by the caller,
 * as different schedulers use different locks
 */
void scheduler_get_task_info(struct scheduler_props *scheduler_props,
			     uint32_t *data_off_size,
			     struct list_item *tasks)
{
	/* TODO
	 * - container_of(tlist, struct task, list)->uid->id could possibly be used as task id,
	 *   but currently accessing it crashes, so setting the value to 0 for now
	 */
	struct task_props *task_props;
	struct list_item *tlist;

	scheduler_props->core_id = cpu_get_id();
	scheduler_props->task_count = 0;
	*data_off_size += sizeof(*scheduler_props);

	task_props = (struct task_props *)((uint8_t *)scheduler_props + sizeof(*scheduler_props));
	list_for_item(tlist, tasks) {
		/* Fill SchedulerProps */
		scheduler_props->task_count++;

		/* Fill TaskProps */
		task_props->task_id = 0;

		/* Left unimplemented */
		task_props->module_instance_count = 0;

		/* TODO: after module instances are implemented, remember to increase
		 * data_off_size and the offset to next task_props by the amount of
		 * module instances included
		 */
		*data_off_size += sizeof(*task_props);
		task_props++;
	}
}
