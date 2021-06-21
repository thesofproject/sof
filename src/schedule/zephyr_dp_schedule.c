// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

/*
 * Data processing priority based scheduling. Intention is to be similar to
 * userspace processing in that work can be
 *
 * 1) Preempted - higher priority work can preempt DP work.
 * 2) Block - DP work can block on IO and sleep().
 *
 * All DP work has a priority and can be preempted by higher priority DP or LL
 * work. DP work can also sleep() and yield() when needed.
 * DP work should return when completed.
 *
 * The DP threads and stack are allocated at runtime when DP pipelines are
 * loaded by host.
 *
 * TODO: Support stack size from topology.
 */

#include <sof/init.h>
#include <sof/lib/alloc.h>
#include <ipc/topology.h>

#include <kernel.h>
#include <sys_clock.h>

/* TODO: set high/low via Kconfig */
#define DP_THREAD_HIGHEST_PRIORITY	4
#define DP_THREAD_LOWEST_PRIORITY	7

/* TODO: this could come from Kconfig, but topology would be better */
#define DP_THREAD_STACK_SIZE		4096

/* NOTE: based on 8 levels 0 - 3 LL, 4 - 7 DP TODO: set by kconfig */
#define DP_THREAD_PRIORITIES	\
	(DP_THREAD_LOWEST_PRIORITY - DP_THREAD_HIGHEST_PRIORITY + 1)

/* scheduler per DP thread private data */
struct dp_thread {
	struct k_thread thread;
	struct k_stack stack;
	stack_data_t *stack_data;
	k_tid_t id;
};

/* scheduler private data */
struct dp_data {
	struct list_item list;	/* list of DP tasks TODO: needed ? */
	struct dp_thread dp_thread[CONFIG_CORE_COUNT][DP_THREAD_PRIORITIES];
};

static const struct scheduler_ops dp_zephyr_ops;

static void dp_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	int reschedule;

	/*
	 * This loop will block on IO when pipeline copy() makes any calls to
	 * comp_get_copy_limits().
	 */
	for (;;) {
		/* run the work */
		reschedule = task->ops.run(task->data);

		/* is work finished ? */
		if (reschedule != SOF_TASK_STATE_RESCHEDULE)
			break;

		/* has task to stop ? */
		if (task->state != SOF_TASK_STATE_RUNNING)
			break;
	}

	/* mark as complete TODO: is this needed for Zephyr threads ?? */
	task->ops.complete(task->data);
}

/* Create a new DP thread */
static int dp_task_add(void *data, struct task *task, uint64_t start,
			     uint64_t period)
{
	struct dp_data *dp = data;
	struct dp_thread *dp_thread;
	char thread_name[] = "dp_thread0.0";
	(void) period; /* not used */
	(void) start; /* not used */

	/* validate task core */
	if (task->core >= CONFIG_CORE_COUNT) {
		tr_info(&ll_tr, "dp_task_add: invalid core %d", task->core);
		return -EINVAL;
	}

	/* validate task priority */
	if (task->priority < DP_THREAD_HIGHEST_PRIORITY ||
	    task->priority >= DP_THREAD_LOWEST_PRIORITY) {
		tr_err(&ll_tr, "dp_task_add: invalid priority %d, need %d:%d",
			task->priority, DP_THREAD_HIGHEST_PRIORITY,
			DP_THREAD_LOWEST_PRIORITY);
		return -EINVAL;
	}

	/* thread context for this core and priority */
	dp_thread = &dp->dp_thread[task->core][task->priority];

	/* allocate the stack space */
	dp_thread->stack_data = rballoc(0, SOF_MEM_CAPS_RAM, DP_THREAD_STACK_SIZE);
	if (!dp_thread->stack_data) {
		tr_err(&ll_tr, "dp_task_add: no mem for stack, need %d",
			DP_THREAD_STACK_SIZE);
		return -ENOMEM;
	}

	/* allocate stack */
	k_stack_init(&dp_thread->stack, dp_thread->stack_data, DP_THREAD_STACK_SIZE);

	/* set thread name with core and priority */
	thread_name[sizeof(thread_name) - 4] = '0' + task->priority;
	thread_name[sizeof(thread_name) - 2] = '0' + task->core;

	/* allocate thread for task at priority and core */
	dp_thread->id = k_thread_create(&dp_thread->thread, dp_thread->stack,
					DP_THREAD_STACK_SIZE,
					dp_thread_fn, task, NULL, NULL,
					task->priority - DP_THREAD_HIGHEST_PRIORITY,
					0, K_FOREVER);

	/* set core affinity */
	k_thread_cpu_mask_clear(dp_thread->id);
	k_thread_cpu_mask_enable(dp_thread->id, task->core);
	k_thread_name_set(dp_thread->id, thread_name);

	k_thread_start(dp_thread->id);

	return 0;
}

static int dp_task_cancel(void *data, struct task *task)
{
	struct dp_data *dp = data;
	struct dp_thread *dp_thread;

	/* thread context for this core and priority */
	dp_thread = &dp->dp_thread[task->core][task->priority];

	/* cancel thread now */
	task->state = SOF_TASK_STATE_CANCEL;
	k_wakeup(dp_thread->id);

	return 0;
}

static int dp_task_running(void *data, struct task *task)
{
	/* DP work threads will run after being added so no need to
	 * manually run them */
	return 0;
}

static int dp_task_complete(void *data, struct task *task)
{
	/* DP work threads will complete after returning */
	task->state = SOF_TASK_STATE_COMPLETED;
	return 0;
}

int dp_task_init(struct task *task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   void *data, uint16_t core, uint32_t flags)
{
	/* setup task - TODO needed for Zephyr ??? */
	task->uid = uid;
	task->type = SOF_SCHEDULE_PRIORITY_DP; /* Note: Force Priority scheduler */
	/* TODO: where do we get priority */
	task->priority = DP_THREAD_HIGHEST_PRIORITY;
	task->core = core;
	task->flags = flags;
	task->state = SOF_TASK_STATE_INIT;
	task->ops = ops;
	task->data = data;

	return 0;
}

static int dp_task_free(void *data, struct task *task)
{
	struct dp_data *dp = data;
	struct dp_thread *dp_thread;
	int is_complete;

	/* thread context for this core and priority */
	dp_thread = &dp->dp_thread[task->core][task->priority];

	/* has thread exited */
	is_complete = k_thread_join(dp_thread->thread, K_NO_WAIT);
	if (!is_complete) {
		tr_err(&ll_tr, "dp_task_free: thread not completed !!!");
		return -EINVAL;
	}

	/* TODO: do we really need the complet state/calls for Zephyr ??? */
	/* lest make sure we are actually completed before we free */
	if (task->state != SOF_TASK_STATE_COMPLETED) {
		tr_err(&ll_tr, "dp_task_free: task state not completed !!!");
	}

	/* free resources */
	rfree(dp_thread->stack_data);

	return 0;
}

static void dp_free(void *data)
{
	/* do any zephyr related free for DP threads */
	struct dp_data *dp = data;

	rfree(dp);
}

static const struct scheduler_ops dp_zephyr_ops = {
	.schedule_task		= dp_task_add,
	.schedule_task_running	= dp_task_running,
	.schedule_task_complete = dp_task_complete,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= dp_task_cancel,
	.schedule_task_free	= dp_task_free,
	.scheduler_free		= dp_free,
};

int dp_init(void)
{
	struct dp_data *dp;

	dp = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
					  sizeof(*dp));
	scheduler_init(SOF_SCHEDULE_PRIORITY_DP, &dp_zephyr_ops, dp);

	return 0;
}
