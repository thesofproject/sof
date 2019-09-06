// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

/*
 * Generic delayed work queue support.
 *
 * Work can be queued to run after a microsecond timeout on either the system
 * work queue or a private work queue. It's expected most users will use the
 * system work queue as private work queues depend on available architecture
 * timers.
 *
 * The work on the system work queue should be short duration and not delay
 * any other work on this queue. If you have longer duration work (like audio
 * processing) then use a private work queue.
 *
 * The generic work queues are intended to stay in time synchronisation with
 * any CPU clock changes. i.e. timeouts will remain constant regardless of CPU
 * frequency changes.
 */

#include <sof/atomic.h>
#include <sof/common.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct ll_schedule_data {
	struct list_item tasks;			/* list of ll tasks */
	uint64_t timeout;			/* timeout for next queue run */
	uint32_t window_size;			/* window size for pending ll */
	struct notifier notifier;		/* notify CPU freq changes */
	struct timesource_data *ts;		/* time source for work queue */
	uint32_t ticks_per_msec;		/* ticks per msec */
	atomic_t num_ll;			/* number of queued ll items */
};

struct ll_queue_shared_context {
	spinlock_t *lock;		/* lock to synchronize many cores */
	atomic_t total_num_work;	/* number of total queued ll items */
	atomic_t timer_clients;		/* number of timer clients */
	uint64_t last_tick;		/* time of last tick */

	/* registered timers */
	struct timer *timers[PLATFORM_CORE_COUNT];
	void *irq_arg[PLATFORM_CORE_COUNT];
};

static struct ll_queue_shared_context *ll_shared_ctx;

struct scheduler_ops schedule_ll_ops;

/* calculate next timeout */
static uint64_t queue_calc_next_timeout(struct ll_schedule_data *queue,
					uint64_t start)
{
	return queue->ticks_per_msec * queue->timeout / 1000 + start;
}

static uint64_t ll_get_timer(struct ll_schedule_data *queue)
{
	return queue->ts->timer_get(&queue->ts->timer);
}

static void ll_set_timer(struct ll_schedule_data *queue)
{
	int core = cpu_get_id();
	uint64_t ticks;

	spin_lock(ll_shared_ctx->lock);

	if (atomic_add(&queue->num_ll, 1) == 1)
		ll_shared_ctx->timers[core] = &queue->ts->timer;

	if (atomic_add(&ll_shared_ctx->total_num_work, 1) == 1) {
		ticks = queue_calc_next_timeout(queue, ll_get_timer(queue));
		ll_shared_ctx->last_tick = ticks;
		queue->ts->timer_set(&queue->ts->timer, ticks);
		atomic_add(&ll_shared_ctx->timer_clients, 1);
		timer_enable(&queue->ts->timer, ll_shared_ctx->irq_arg[core],
			     core);
	}

	spin_unlock(ll_shared_ctx->lock);
}

static void ll_clear_timer(struct ll_schedule_data *queue)
{
	spin_lock(ll_shared_ctx->lock);

	if (!atomic_sub(&ll_shared_ctx->total_num_work, 1))
		queue->ts->timer_clear(&queue->ts->timer);

	if (!atomic_sub(&queue->num_ll, 1))
		ll_shared_ctx->timers[cpu_get_id()] = NULL;

	spin_unlock(ll_shared_ctx->lock);
}

/* is there any work pending in the current time window ? */
static int is_ll_pending(struct ll_schedule_data *queue)
{
	struct list_item *wlist;
	struct task *ll_task;
	uint64_t win_end;
	uint64_t win_start;
	int pending_count = 0;

	/* get the current valid window of work */
	win_end = ll_get_timer(queue);
	win_start = win_end - queue->window_size;

	/* mark each valid work item in this time period as pending */
	list_for_item(wlist, &queue->tasks) {
		ll_task = container_of(wlist, struct task, list);

		/* correct the pending flag window for overflow */
		if (win_end > win_start) {
			/* mark pending work */
			if (ll_task->start >= win_start &&
				ll_task->start <= win_end) {
				ll_task->state =
					SOF_TASK_STATE_PENDING;
				pending_count++;
			} else {
				ll_task->state = SOF_TASK_STATE_INIT;
			}
		} else {
			/* mark pending work */
			if ((ll_task->start <= win_end ||
				(ll_task->start >= win_start &&
				ll_task->start < UINT64_MAX))) {
				ll_task->state =
					SOF_TASK_STATE_PENDING;
				pending_count++;
			} else {
				ll_task->state = SOF_TASK_STATE_INIT;
			}
		}
	}

	return pending_count;
}

static void ll_next_timeout(struct ll_schedule_data *queue,
			    struct task *work)
{
	/* reschedule work */
	struct ll_task_pdata *ll_task_pdata = ll_sch_get_pdata(work);
	uint64_t next_d;

	next_d = queue->ticks_per_msec * ll_task_pdata->period / 1000;

	if (work->flags & SOF_SCHEDULE_FLAG_SYNC) {
		work->start += next_d;
	} else {
		/* calc next run based on work request */
		work->start = next_d + ll_shared_ctx->last_tick;
	}
}

/* run all pending work */
static void run_ll(struct ll_schedule_data *queue, uint32_t *flags)
{
	struct list_item *wlist;
	struct list_item *tlist;
	struct task *ll_task;
	int cpu = cpu_get_id();

	/* check each work item in queue for pending */
	list_for_item_safe(wlist, tlist, &queue->tasks) {
		ll_task = container_of(wlist, struct task, list);

		/* run work if its pending and remove from the queue */
		if (ll_task->state == SOF_TASK_STATE_PENDING) {
			/* work can run in non atomic context */
			irq_local_enable(*flags);
			ll_task->state = ll_task->func(ll_task->data);
			irq_local_disable(*flags);

			/* do we need reschedule this work ? */
			if (ll_task->state == SOF_TASK_STATE_COMPLETED) {
				list_item_del(&ll_task->list);
				atomic_sub(&ll_shared_ctx->total_num_work, 1);

				/* don't enable irq, if no more work to do */
				if (!atomic_sub(&queue->num_ll, 1))
					ll_shared_ctx->timers[cpu] = NULL;
			} else {
				/* get next work timeout */
				ll_next_timeout(queue, ll_task);
			}
		}
	}
}

static uint64_t calc_delta_ticks(uint64_t current, uint64_t work)
{
	/* does work run in next cycle ? */
	if (work >= current)
		return work - current;

	return UINT64_MAX - current + work;
}

/* re calculate timers for queue after CPU frequency change */
static void queue_recalc_timers(struct ll_schedule_data *queue,
				struct clock_notify_data *clk_data)
{
	struct list_item *wlist;
	struct task *ll_task;
	uint64_t delta_ticks;
	uint64_t delta_msecs;
	uint64_t current;

	/* get current time */
	current = ll_get_timer(queue);

	/* recalculate timers for each work item */
	list_for_item(wlist, &queue->tasks) {
		ll_task = container_of(wlist, struct task, list);
		delta_ticks = calc_delta_ticks(current, ll_task->start);
		delta_msecs = delta_ticks /
			clk_data->old_ticks_per_msec;

		/* is work within next msec, then schedule it now */
		if (delta_msecs > 0)
			ll_task->start = current +
				queue->ticks_per_msec * delta_msecs;
		else
			ll_task->start = current +
				(queue->ticks_per_msec >> 3);
	}
}

/* enable all registered timers */
static void queue_enable_registered_timers(void)
{
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (ll_shared_ctx->timers[i]) {
			atomic_add(&ll_shared_ctx->timer_clients, 1);
			timer_enable(ll_shared_ctx->timers[i],
				     ll_shared_ctx->irq_arg[i], i);
		}
	}
}

/* reschedule queue */
static void queue_reschedule(struct ll_schedule_data *queue)
{
	uint64_t ticks;

	/* clear only if all timer clients are done */
	if (!atomic_sub(&ll_shared_ctx->timer_clients, 1)) {
		/* clear timer */
		queue->ts->timer_clear(&queue->ts->timer);

		/* re-arm only if there is work to do */
		if (atomic_read(&ll_shared_ctx->total_num_work)) {
			/* re-arm timer */
			ticks = queue_calc_next_timeout
				(queue,
				 ll_shared_ctx->last_tick);
			ll_shared_ctx->last_tick = ticks;
			queue->ts->timer_set(&queue->ts->timer, ticks);

			queue_enable_registered_timers();
		}
	}
}

/* run the work queue */
static void queue_run(void *data)
{
	struct ll_schedule_data *queue = (struct ll_schedule_data *)data;
	int core = cpu_get_id();
	uint32_t flags;

	timer_disable(&queue->ts->timer, ll_shared_ctx->irq_arg[core], core);

	irq_local_disable(flags);

	/* run work if there is any pending */
	if (is_ll_pending(queue))
		run_ll(queue, &flags);

	spin_lock(ll_shared_ctx->lock);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock(ll_shared_ctx->lock);

	irq_local_enable(flags);
}

/* notification of CPU frequency changes - atomic PRE and POST sequence */
static void ll_notify(int message, void *data, void *event_data)
{
	struct ll_schedule_data *queue = (struct ll_schedule_data *)data;
	struct clock_notify_data *clk_data =
		(struct clock_notify_data *)event_data;
	uint32_t flags;

	irq_local_disable(flags);

	/* we need to re-calculate timer when CPU frequency changes */
	if (message == CLOCK_NOTIFY_POST) {
		/* CPU frequency update complete */
		/* scale the window size to clock speed */
		queue->ticks_per_msec = clock_ms_to_ticks(queue->ts->clk, 1);
		queue->window_size =
			queue->ticks_per_msec * PLATFORM_WORKQ_WINDOW / 1000;
		queue_recalc_timers(queue, clk_data);
	} else if (message == CLOCK_NOTIFY_PRE) {
		/* CPU frequency update pending */
	}

	irq_local_enable(flags);
}

static void insert_task_to_queue(struct task *w, struct list_item *q_list)
{
	struct task *ll_task;
	struct list_item *wlist;

	/* works are adding to queue in order */
	list_for_item(wlist, q_list) {
		ll_task = container_of(wlist, struct task, list);
		if (w->priority <= ll_task->priority) {
			list_item_append(&w->list, &ll_task->list);
			return;
		}
	}

	/* if task has not been added, means that it has the lowest
	 * priority in queue and it should be added at the end of the list
	 */
	list_item_append(&w->list, q_list);
}

static void schedule_ll_task(void *data, struct task *task, uint64_t start,
			     uint64_t period)
{
	struct ll_schedule_data *sch = data;
	struct ll_task_pdata *ll_pdata;
	struct task *ll_task;
	struct list_item *wlist;
	uint32_t flags;

	irq_local_disable(flags);

	/* check to see if we are already scheduled ? */
	list_for_item(wlist, &sch->tasks) {
		ll_task = container_of(wlist, struct task, list);

		/* keep original start */
		if (ll_task == task)
			goto out;
	}

	task->start = sch->ticks_per_msec * start / 1000;

	/* convert start micro seconds to CPU clock ticks */
	if (task->flags & SOF_SCHEDULE_FLAG_SYNC)
		task->start += ll_get_timer(sch);
	else
		task->start += ll_shared_ctx->last_tick;

	ll_pdata = ll_sch_get_pdata(task);

	/* invalidate if slave core */
	if (cpu_is_slave(task->core))
		dcache_invalidate_region(ll_pdata, sizeof(*ll_pdata));

	ll_pdata->period = period;

	/* insert work into list */
	insert_task_to_queue(task, &sch->tasks);

	ll_set_timer(sch);

out:
	irq_local_enable(flags);
}

static void reschedule_ll_task(void *data, struct task *task, uint64_t start)
{
	struct ll_schedule_data *sch = data;
	struct task *ll_task;
	struct list_item *wlist;
	uint32_t flags;
	uint64_t time;

	time = sch->ticks_per_msec * start / 1000;

	/* convert start micro seconds to CPU clock ticks */
	if (task->flags & SOF_SCHEDULE_FLAG_SYNC)
		time += ll_get_timer(sch);
	else
		time += ll_shared_ctx->last_tick;

	irq_local_disable(flags);

	/* check to see if we are already scheduled */
	list_for_item(wlist, &sch->tasks) {
		ll_task = container_of(wlist, struct task, list);
		/* found it */
		if (ll_task == task)
			goto found;
	}

	insert_task_to_queue(task, &sch->tasks);

	ll_set_timer(sch);

found:
	/* re-calc timer and re-arm */
	task->start = time;

	irq_local_enable(flags);
}

static void schedule_ll_task_cancel(void *data, struct task *task)
{
	struct ll_schedule_data *sch = data;
	struct task *ll_task;
	struct list_item *wlist;
	uint32_t flags;

	irq_local_disable(flags);

	/* check to see if we are scheduled */
	list_for_item(wlist, &sch->tasks) {
		ll_task = container_of(wlist, struct task, list);

		/* found it */
		if (ll_task == task) {
			ll_clear_timer(sch);
			break;
		}
	}

	/* remove work from list */
	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	irq_local_enable(flags);
}

static void schedule_ll_task_free(void *data, struct task *task)
{
	struct ll_task_pdata *ll_pdata;
	uint32_t flags;

	irq_local_disable(flags);

	/* release the resources */
	task->state = SOF_TASK_STATE_FREE;
	ll_pdata = ll_sch_get_pdata(task);
	rfree(ll_pdata);
	ll_sch_set_pdata(task, NULL);

	irq_local_enable(flags);
}

static struct ll_schedule_data *work_new_queue(struct timesource_data *ts)
{
	struct ll_schedule_data *queue;

	/* init work queue */
	queue = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*queue));
	list_init(&queue->tasks);

	atomic_init(&queue->num_ll, 0);
	queue->ts = ts;
	queue->ticks_per_msec = clock_ms_to_ticks(queue->ts->clk, 1);
	queue->window_size = queue->ticks_per_msec *
		PLATFORM_WORKQ_WINDOW / 1000;

	/* TODO: configurable through IPC */
	queue->timeout = PLATFORM_WORKQ_DEFAULT_TIMEOUT;

	/* notification of clk changes */
	queue->notifier.cb = ll_notify;
	queue->notifier.cb_data = queue;
	queue->notifier.id = ts->notifier;
	notifier_register(&queue->notifier);

	return queue;
}

int scheduler_init_ll(void)
{
	struct ll_schedule_data *sch;
	struct timesource_data *ts;
	int cpu;

	cpu = cpu_get_id();
	ts = &platform_generic_queue[cpu];

	sch = work_new_queue(ts);
	scheduler_init(SOF_SCHEDULE_LL, &schedule_ll_ops, sch);

	if (cpu == PLATFORM_MASTER_CORE_ID) {
		ll_shared_ctx = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED,
					SOF_MEM_CAPS_RAM,
					sizeof(*ll_shared_ctx));
		spinlock_init(&ll_shared_ctx->lock);
		atomic_init(&ll_shared_ctx->total_num_work, 0);
		atomic_init(&ll_shared_ctx->timer_clients, 0);
	}

	/* register system timer */
	timer_register(&platform_generic_queue[cpu].timer, queue_run, sch);

	/* save interrupt argument */
	ll_shared_ctx->irq_arg[cpu] = sch;

	return 0;
}

/* initialise our work */
static int schedule_ll_task_init(void *data, struct task *task)
{
	struct ll_task_pdata *ll_pdata;

	if (ll_sch_get_pdata(task))
		return -EEXIST;

	ll_pdata = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
			   sizeof(*ll_pdata));

	if (!ll_pdata) {
		trace_ll_error("schedule_ll_task_init() error: alloc failed");
		return -ENOMEM;
	}

	/* flush for slave core */
	if (cpu_is_slave(task->core))
		dcache_writeback_invalidate_region(ll_pdata,
						   sizeof(*ll_pdata));

	ll_sch_set_pdata(task, ll_pdata);

	return 0;
}

static void scheduler_free_ll(void *data)
{
	struct ll_schedule_data *sch = data;
	uint32_t flags;

	irq_local_disable(flags);

	timer_unregister(&sch->ts->timer,
			 ll_shared_ctx->irq_arg[cpu_get_id()]);

	notifier_unregister(&sch->notifier);

	list_item_del(&sch->tasks);

	irq_local_enable(flags);
}

struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_init	= schedule_ll_task_init,
	.schedule_task_running	= NULL,
	.schedule_task_complete	= NULL,
	.reschedule_task	= reschedule_ll_task,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.schedule_task_free	= schedule_ll_task_free,
	.scheduler_free		= scheduler_free_ll,
	.scheduler_run		= NULL
};
