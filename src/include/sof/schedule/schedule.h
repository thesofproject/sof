/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

/* Generic schedule api header */
#ifndef __SOF_SCHEDULE_SCHEDULE_H__
#define __SOF_SCHEDULE_SCHEDULE_H__

#include <sof/bit.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/schedule/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

/* schedule tracing */
#define trace_schedule(format, ...) \
	trace_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

#define trace_schedule_error(format, ...) \
	trace_error(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

#define tracev_schedule(format, ...) \
	tracev_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

/* SOF_SCHEDULE_ type comes from topology */
enum {
	/* EDF scheduler - schedules based on task's deadline */
	SOF_SCHEDULE_EDF = 0,

	/* Low latency timer scheduler - schedules immediately on selected
	 * timer's tick
	 */
	SOF_SCHEDULE_LL_TIMER,

	/* Low latency DMA scheduler - schedules immediately on scheduling
	 * component's DMA interrupt
	 */
	SOF_SCHEDULE_LL_DMA,
	SOF_SCHEDULE_COUNT
};

struct scheduler_ops {
	void (*schedule_task)(void *data, struct task *task, uint64_t start,
			      uint64_t period);
	void (*schedule_task_running)(void *data, struct task *task);
	void (*schedule_task_complete)(void *data, struct task *task);
	void (*reschedule_task)(void *data, struct task *task, uint64_t start);
	void (*schedule_task_cancel)(void *data, struct task *task);
	void (*schedule_task_free)(void *data, struct task *task);
	void (*scheduler_free)(void *data);
	void (*scheduler_run)(void *data);
};

struct schedule_data {
	struct list_item list;
	int type;
	const struct scheduler_ops *ops;
	void *data;
};

struct schedulers {
	struct list_item list;
};

struct schedulers **arch_schedulers_get(void);

static inline void *scheduler_get_data(uint16_t type)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (type == sch->type)
			return sch->data;
	}

	return NULL;
}

static inline void schedule_task_running(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task_running)
			sch->ops->schedule_task_running(sch->data, task);
	}
}

static inline void schedule_task_complete(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task_complete)
			sch->ops->schedule_task_complete(sch->data, task);
	}
}

static inline void schedule_task(struct task *task, uint64_t start,
				 uint64_t period)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task)
			sch->ops->schedule_task(sch->data, task, start, period);
	}
}

static inline void reschedule_task(struct task *task, uint64_t start)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->reschedule_task)
			sch->ops->reschedule_task(sch->data, task, start);
	}
}

static inline void schedule_task_cancel(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task_cancel)
			sch->ops->schedule_task_cancel(sch->data, task);
	}
}

static inline void schedule_task_free(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task_free)
			sch->ops->schedule_task_free(sch->data, task);
	}
}

static inline void schedule_free(void)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (sch->ops->scheduler_free)
			sch->ops->scheduler_free(sch->data);
	}
}

static inline void schedule(void)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (sch->ops->scheduler_run)
			sch->ops->scheduler_run(sch->data);
	}
}

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       enum task_state (*run)(void *data), void *data,
		       uint16_t core, uint32_t flags);


void scheduler_init(int type, const struct scheduler_ops *ops, void *data);

#endif /* __SOF_SCHEDULE_SCHEDULE_H__ */
