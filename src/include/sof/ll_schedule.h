/*
 * Copyright (c) 2016, Intel Corporation
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
 *
 * Delayed or scheduled work. Work runs in the same context as it's timer
 * interrupt source. It should execute quickly and must not sleep or wait.
 */

#ifndef __INCLUDE_SOF_LOW_LATENCY_SCHEDULE_H__
#define __INCLUDE_SOF_LOW_LATENCY_SCHEDULE_H__

#include <stdint.h>
#include <sof/list.h>
#include <sof/timer.h>
#include <sof/alloc.h>
#include <sof/schedule.h>
#include <sof/trace.h>

struct work_queue;

#define ll_sch_set_pdata(task, data) \
	task->private = data

#define ll_sch_get_pdata(task) task->private

struct ll_task_pdata {
	uint32_t flags;
};

extern struct timesource_data platform_generic_queue[];

extern struct scheduler_ops schedule_ll_ops;

#endif /* __INCLUDE_SOF_LOW_LATENCY_SCHEDULE_H__ */
