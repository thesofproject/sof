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

#ifndef __INCLUDE_SOF_SCHEDULE_H__
#define __INCLUDE_SOF_SCHEDULE_H__

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/work.h>
#include <sof/wait.h>

struct sof;

/* task states */
#define TASK_STATE_INIT		0	
#define TASK_STATE_QUEUED	1
#define TASK_STATE_RUNNING	2
#define TASK_STATE_PREEMPTED	3
#define TASK_STATE_COMPLETED	4
#define TASK_STATE_FREE		5
#define TASK_STATE_CANCEL	6

/* task priorities - values same as Linux processes, gives scope for future.*/
#define TASK_PRI_LOW	19
#define TASK_PRI_MED	0
#define TASK_PRI_HIGH	-20

#define TASK_PRI_IPC	6

/* maximun task time slice in microseconds */
#define SCHEDULE_TASK_MAX_TIME_SLICE	5000

/* task descriptor */
struct task {
	uint16_t core;			/* core id to run on */
	int16_t priority;		/* scheduling priority TASK_PRI_ */
	uint64_t start;			/* scheduling earliest start time */
	uint64_t deadline;		/* scheduling deadline */
	uint32_t state;			/* TASK_STATE_ */
	struct list_item list;		/* list in scheduler */
	struct list_item irq_list;	/* list for assigned irq level */

	/* task function and private data */
	void *data;
	void (*func)(void *arg);

	/* runtime duration in scheduling clock base */
	uint64_t max_rtime;		/* max time taken to run */
	completion_t complete;
};

void schedule(void);

void schedule_task(struct task *task, uint64_t start, uint64_t deadline);

void schedule_task_idle(struct task *task, uint64_t deadline);

int schedule_task_cancel(struct task *task);

void schedule_task_complete(struct task *task);

static inline void schedule_task_init(struct task *task, void (*func)(void *),
	void *data)
{
	task->core = 0;
	task->state = TASK_STATE_INIT;
	task->func = func;
	task->data = data;
}

static inline void schedule_task_free(struct task *task)
{
	task->state = TASK_STATE_FREE;
	task->func = NULL;
	task->data = NULL;
}

static inline void schedule_task_config(struct task *task, uint16_t priority,
	uint16_t core)
{
	task->priority = priority;
	task->core = core;
}

int scheduler_init(struct sof *sof);

#endif
