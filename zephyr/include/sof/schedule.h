/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

/* Generic schedule api header */
#ifndef __INCLUDE_SOF_SCHEDULER_H__
#define __INCLUDE_SOF_SCHEDULER_H__

#include <stdint.h>
#include <sof/list.h>
#include <sof/trace.h>

/* schedule tracing */
#define trace_schedule(format, ...) \
	trace_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

#define trace_schedule_error(format, ...) \
	trace_error(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

#define tracev_schedule(format, ...) \
	tracev_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

/* SOF_SCHEDULE_ type comes from topology */
enum {
	SOF_SCHEDULE_EDF = 0,	/* EDF scheduler */
	SOF_SCHEDULE_LL,	/* Low latency scheduler */
	SOF_SCHEDULE_COUNT
};

#define SOF_TASK_PRI_HIGH	0	/* priority level 0 - high */
#define SOF_TASK_PRI_MED	4	/* priority level 4 - medium */
#define SOF_TASK_PRI_LOW	9	/* priority level 9 - low */

#define SOF_TASK_PRI_COUNT	10	/* range of priorities (0-9) */

#define SOF_TASK_PRI_IPC	SOF_TASK_PRI_LOW
#define SOF_TASK_PRI_IDC	SOF_TASK_PRI_LOW

/* task states */
#define SOF_TASK_STATE_INIT		0
#define SOF_TASK_STATE_QUEUED		1
#define SOF_TASK_STATE_PENDING		2
#define SOF_TASK_STATE_RUNNING		3
#define SOF_TASK_STATE_PREEMPTED	4
#define SOF_TASK_STATE_COMPLETED	5
#define SOF_TASK_STATE_FREE		6
#define SOF_TASK_STATE_CANCEL		7

/* Scheduler flags */
/* Sync/Async only supported by ll scheduler atm */
#define SOF_SCHEDULE_FLAG_ASYNC (0 << 0) /* task scheduled asynchronously */
#define SOF_SCHEDULE_FLAG_SYNC	(1 << 0) /* task scheduled synchronously */
#define SOF_SCHEDULE_FLAG_IDLE  (2 << 0)

struct task;

struct scheduler_ops {
	void (*schedule_task)(struct task *w, uint64_t start,
			      uint64_t deadline, uint32_t flags);
	int (*schedule_task_init)(struct task *task, uint32_t xflags);
	void (*schedule_task_running)(struct task *task);
	void (*schedule_task_complete)(struct task *task);
	void (*reschedule_task)(struct task *task, uint64_t start);
	int (*schedule_task_cancel)(struct task *task);
	void (*schedule_task_free)(struct task *task);
	int (*scheduler_init)(void);
	void (*scheduler_free)(void);
	void (*scheduler_run)(void);
};

struct task {
	uint16_t type;
	uint64_t start;
	uint16_t priority;
	uint16_t state;
	uint16_t core;
	void *data;
	uint64_t (*func)(void *data);
	struct list_item list;
	struct list_item irq_list;	/* list for assigned irq level */
	const struct scheduler_ops *ops;
	void *private;
};

struct edf_schedule_data;
struct ll_schedule_data;

struct schedule_data {
	struct ll_schedule_data *ll_sch_data;
	struct edf_schedule_data *edf_sch_data;
};

struct schedule_data **arch_schedule_get_data(void);

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       uint64_t (*func)(void *data), void *data, uint16_t core,
		       uint32_t xflags);

void schedule_task_running(struct task *task);

void schedule_task_complete(struct task *task);

void schedule_task(struct task *task, uint64_t start, uint64_t deadline,
		   uint32_t flags);

void reschedule_task(struct task *task, uint64_t start);

void schedule_free(void);

void schedule(void);

int scheduler_init(void);

int schedule_task_cancel(struct task *task);

void schedule_task_free(struct task *task);

#endif /* __INCLUDE_SOF_SCHEDULER_H__ */

