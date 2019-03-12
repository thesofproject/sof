/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
#include <sof/schedule.h>
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
	task->private = data

#define edf_sch_get_pdata(task) task->private

struct edf_task_pdata {
	uint64_t deadline;
};

extern struct scheduler_ops schedule_edf_ops;

#endif /* __INCLUDE_SOF_EDF_SCHEDULE_H__ */
