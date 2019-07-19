/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

/* Generic schedule api header */
#ifndef __SOF_SCHEDULE_SCHEDULE_H__
#define __SOF_SCHEDULE_SCHEDULE_H__

#include <sof/list.h>
#include <sof/trace/trace.h>
#include <stdint.h>

struct edf_schedule_data;
struct ll_schedule_data;
struct task;

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

/* Scheduler flags */
/* Sync/Async only supported by ll scheduler atm */
#define SOF_SCHEDULE_FLAG_ASYNC (0 << 0) /* task scheduled asynchronously */
#define SOF_SCHEDULE_FLAG_SYNC	(1 << 0) /* task scheduled synchronously */
#define SOF_SCHEDULE_FLAG_IDLE  (2 << 0)

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

struct schedule_data {
	struct ll_schedule_data *ll_sch_data;
	struct edf_schedule_data *edf_sch_data;
};

extern struct scheduler_ops schedule_edf_ops;
extern struct scheduler_ops schedule_ll_ops;

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

#endif /* __SOF_SCHEDULE_SCHEDULE_H__ */
