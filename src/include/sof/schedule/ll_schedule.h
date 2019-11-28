/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/*
 * Delayed or scheduled work. Work runs in the same context as it's timer
 * interrupt source. It should execute quickly and must not sleep or wait.
 */

#ifndef __SOF_SCHEDULE_LL_SCHEDULE_H__
#define __SOF_SCHEDULE_LL_SCHEDULE_H__

#include <sof/schedule/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

struct ll_schedule_domain;

/* ll tracing */
#define trace_ll(format, ...) \
	trace_event(TRACE_CLASS_SCHEDULE_LL, format, ##__VA_ARGS__)

#define trace_ll_error(format, ...) \
	trace_error(TRACE_CLASS_SCHEDULE_LL, format, ##__VA_ARGS__)

#define tracev_ll(format, ...) \
	tracev_event(TRACE_CLASS_SCHEDULE_LL, format, ##__VA_ARGS__)

#define ll_sch_set_pdata(task, data) \
	do { (task)->private = (data); } while (0)

#define ll_sch_get_pdata(task) ((task)->private)

struct ll_task_pdata {
	uint64_t period;
};

int scheduler_init_ll(struct ll_schedule_domain *domain);

int schedule_task_init_ll(struct task *task, uint16_t type, uint16_t priority,
			  enum task_state (*run)(void *data), void *data,
			  uint16_t core, uint32_t flags);

#endif /* __SOF_SCHEDULE_LL_SCHEDULE_H__ */
