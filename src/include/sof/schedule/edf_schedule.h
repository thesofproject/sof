/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_SOF_EDF_SCHEDULE_H__
#define __INCLUDE_SOF_EDF_SCHEDULE_H__

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/wait.h>
#include <sof/schedule/schedule.h>
#include <sof/alloc.h>

/* schedule tracing */
#define trace_edf_sch(format, ...) \
	trace_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

#define trace_edf_sch_error(format, ...) \
	trace_error(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

#define tracev_edf_sch(format, ...) \
	tracev_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

struct sof;

/* maximun task time slice in microseconds */
#define SCHEDULE_TASK_MAX_TIME_SLICE	5000

#define edf_sch_set_pdata(task, data) \
	(task->private = data)

#define edf_sch_get_pdata(task) task->private

struct edf_task_pdata {
	uint64_t deadline;
};

extern struct scheduler_ops schedule_edf_ops;

#endif /* __INCLUDE_SOF_EDF_SCHEDULE_H__ */
