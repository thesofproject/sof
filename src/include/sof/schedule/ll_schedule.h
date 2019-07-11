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

#include <stdint.h>
#include <sof/list.h>
#include <sof/drivers/timer.h>
#include <sof/alloc.h>
#include <sof/schedule/schedule.h>
#include <sof/trace.h>

struct work_queue;

/* ll tracing */
#define trace_ll(format, ...) \
	trace_event(TRACE_CLASS_SCHEDULE_LL, format, ##__VA_ARGS__)

#define trace_ll_error(format, ...) \
	trace_error(TRACE_CLASS_SCHEDULE_LL, format, ##__VA_ARGS__)

#define tracev_ll(format, ...) \
	tracev_event(TRACE_CLASS_SCHEDULE_LL, format, ##__VA_ARGS__)

#define ll_sch_set_pdata(task, data) \
	(task->private = data)

#define ll_sch_get_pdata(task) task->private

struct ll_task_pdata {
	uint32_t flags;
};

extern struct timesource_data platform_generic_queue[];

extern struct scheduler_ops schedule_ll_ops;

#endif /* __SOF_SCHEDULE_LL_SCHEDULE_H__ */
