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
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <sof/ll_schedule.h>
#include <sof/schedule.h>
#include <sof/timer.h>
#include <sof/list.h>
#include <sof/clk.h>
#include <sof/alloc.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/notifier.h>
#include <sof/debug.h>
#include <sof/cpu.h>
#include <sof/atomic.h>
#include <platform/clk.h>
#include <platform/platform.h>
#include <limits.h>

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

struct ll_schedule_data {
	struct list_item work[LL_PRIORITIES];	/* list of ll per priority */
	uint64_t timeout;			/* timeout for next queue run */
	uint32_t window_size;			/* window size for pending ll */
	spinlock_t lock;
	struct notifier notifier;		/* notify CPU freq changes */
	struct timesource_data *ts;		/* time source for work queue */
	uint32_t ticks_per_msec;		/* ticks per msec */
	atomic_t num_ll;			/* number of queued ll items */
};

struct ll_queue_shared_context {
	atomic_t total_num_work;	/* number of total queued ll items */
	atomic_t timer_clients;		/* number of timer clients */
	uint64_t last_tick;		/* time of last tick */

	/* registered timers */
	struct timer *timers[PLATFORM_CORE_COUNT];
};

static struct ll_queue_shared_context *ll_shared_ctx;

static void reschedule_ll_task(struct task *w, uint64_t start);
static void schedule_ll_task(struct task *w, uint64_t start, uint64_t deadline,
			     uint32_t flags);
static int schedule_ll_task_cancel(struct task *w);
static void schedule_ll_task_free(struct task *w);
static int ll_scheduler_init(void);
static int schedule_ll_task_init(struct task *w, uint32_t xflags);
static void ll_scheduler_free(void);

/* calculate next timeout */
static inline uint64_t queue_calc_next_timeout(struct ll_schedule_data *queue,
					       uint64_t start)
{
	return queue->ticks_per_msec * queue->timeout / 1000 + start;
}

static inline uint64_t ll_get_timer(struct ll_schedule_data *queue)
{
	return queue->ts->timer_get(&queue->ts->timer);
}

static inline void ll_set_timer(struct ll_schedule_data *queue)
{
	uint64_t ticks;

	if (atomic_add(&queue->num_ll, 1) == 1)
		ll_shared_ctx->timers[cpu_get_id()] = &queue->ts->timer;

	if (atomic_add(&ll_shared_ctx->total_num_work, 1) == 1) {
		ticks = queue_calc_next_timeout(queue, ll_get_timer(queue));
		ll_shared_ctx->last_tick = ticks;
		queue->ts->timer_set(&queue->ts->timer, ticks);
		atomic_add(&ll_shared_ctx->timer_clients, 1);
		timer_enable(&queue->ts->timer);
	}
}

static inline void ll_clear_timer(struct ll_schedule_data *queue)
{
	if (!atomic_sub(&ll_shared_ctx->total_num_work, 1))
		queue->ts->timer_clear(&queue->ts->timer);

	if (!atomic_sub(&queue->num_ll, 1)) {
		timer_disable(&queue->ts->timer);
		atomic_sub(&ll_shared_ctx->timer_clients, 1);
		ll_shared_ctx->timers[cpu_get_id()] = NULL;
	}
}

/* is there any work pending in the current time window ? */
static int is_ll_pending(struct ll_schedule_data *queue)
{
	struct list_item *wlist;
	struct task *ll_task;
	uint64_t win_end;
	uint64_t win_start;
	int pending_count = 0;
	int i;

	/* get the current valid window of work */
	win_end = ll_get_timer(queue);
	win_start = win_end - queue->window_size;

	/* check every priority level */
	for (i = (LL_PRIORITIES - 1); i >= 0; i--) {
		/* mark each valid work item in this time period as pending */
		list_for_item(wlist, &queue->work[i]) {
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
					ll_task->state = 0;
				}
			} else {
				/* mark pending work */
				if ((ll_task->start <= win_end ||
				    (ll_task->start >= win_start &&
				     ll_task->start < ULONG_LONG_MAX))) {
					ll_task->state =
						SOF_TASK_STATE_PENDING;
					pending_count++;
				} else {
					ll_task->state = 0;
				}
			}
		}
	}

	return pending_count;
}

static inline void ll_next_timeout(struct ll_schedule_data *queue,
				   struct task *work,
				   uint64_t reschedule_usecs)
{
	/* reschedule work */
	uint64_t next_d = 0;
	struct ll_task_pdata *ll_task_pdata;

	ll_task_pdata = ll_sch_get_pdata(work);

	next_d = queue->ticks_per_msec * reschedule_usecs / 1000;

	if (ll_task_pdata->flags & SOF_SCHEDULE_FLAG_SYNC) {
		work->start += next_d;
	} else {
		/* calc next run based on work request */
		work->start = next_d + ll_shared_ctx->last_tick;
	}
}

/* run all pending work */
static void run_ll(struct ll_schedule_data *queue, uint32_t *flags,
		   uint32_t priority)
{
	struct list_item *wlist;
	struct list_item *tlist;
	struct task *ll_task;
	uint64_t reschedule_usecs;
	int cpu = cpu_get_id();

	/* check each work item in queue for pending */
	list_for_item_safe(wlist, tlist, &queue->work[priority]) {
		ll_task = container_of(wlist, struct task, list);

		/* run work if its pending and remove from the queue */
		if (ll_task->state == SOF_TASK_STATE_PENDING) {
			/* work can run in non atomic context */
			spin_unlock_irq(&queue->lock, *flags);
			reschedule_usecs = ll_task->func(ll_task->data);
			spin_lock_irq(&queue->lock, *flags);

			/* do we need reschedule this work ? */
			if (reschedule_usecs == 0) {
				list_item_del(&ll_task->list);
				atomic_sub(&ll_shared_ctx->total_num_work, 1);

				/* don't enable irq, if no more work to do */
				if (!atomic_sub(&queue->num_ll, 1))
					ll_shared_ctx->timers[cpu] = NULL;
			} else {
				/* get next work timeout */
				ll_next_timeout(queue, ll_task,
						reschedule_usecs);
			}
		}
	}
}

static inline uint64_t calc_delta_ticks(uint64_t current, uint64_t work)
{
	uint64_t max = ULONG_LONG_MAX;

	/* does work run in next cycle ? */
	if (work < current) {
		max -= current;
		max += work;
		return max;
	} else {
		return work - current;
	}
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
	int i;

	/* get current time */
	current = ll_get_timer(queue);

	/* recalculate for each priority level */
	for (i = (LL_PRIORITIES - 1); i >= 0; i--) {
		/* recalculate timers for each work item */
		list_for_item(wlist, &queue->work[i]) {
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
}

/* enable all registered timers */
static void queue_enable_registered_timers(void)
{
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (ll_shared_ctx->timers[i]) {
			atomic_add(&ll_shared_ctx->timer_clients, 1);
			timer_enable(ll_shared_ctx->timers[i]);
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
	uint32_t flags;
	int i;

	timer_disable(&queue->ts->timer);

	spin_lock_irq(&queue->lock, flags);

	/* run work if there is any pending */
	if (is_ll_pending(queue)) {
		/* run for each priority level */
		for (i = (LL_PRIORITIES - 1); i >= 0; i--)
			run_ll(queue, &flags, i);
	}

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock_irq(&queue->lock, flags);
}

/* notification of CPU frequency changes - atomic PRE and POST sequence */
static void ll_notify(int message, void *data, void *event_data)
{
	struct ll_schedule_data *queue = (struct ll_schedule_data *)data;
	struct clock_notify_data *clk_data =
		(struct clock_notify_data *)event_data;
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

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

	spin_unlock_irq(&queue->lock, flags);
}

static void ll_schedule(struct ll_schedule_data *queue, struct task *w,
			uint64_t start)
{
	struct ll_task_pdata *ll_pdata;
	struct task *ll_task;
	struct list_item *wlist;
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

	/* check to see if we are already scheduled ? */
	list_for_item(wlist, &queue->work[w->priority]) {
		ll_task = container_of(wlist, struct task, list);

		/* keep original start */
		if (ll_task == w)
			goto out;
	}

	w->start = queue->ticks_per_msec * start / 1000;
	ll_pdata = ll_sch_get_pdata(w);

	/* convert start micro seconds to CPU clock ticks */
	if (ll_pdata->flags & SOF_SCHEDULE_FLAG_SYNC)
		w->start += ll_get_timer(queue);
	else
		w->start += ll_shared_ctx->last_tick;

	/* insert work into list */
	list_item_prepend(&w->list, &queue->work[w->priority]);

	ll_set_timer(queue);

out:
	spin_unlock_irq(&queue->lock, flags);
}

static void schedule_ll_task(struct task *w, uint64_t start, uint64_t deadline,
			     uint32_t flags)
{
	(void)deadline;
	(void)flags;
	ll_schedule((*arch_schedule_get_data())->ll_sch_data, w, start);
}

static void reschedule(struct ll_schedule_data *queue, struct task *w,
		       uint64_t time)
{
	struct task *ll_task;
	struct list_item *wlist;
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

	/* check to see if we are already scheduled */
	list_for_item(wlist, &queue->work[w->priority]) {
		ll_task = container_of(wlist, struct task, list);

		/* found it */
		if (ll_task == w)
			goto found;
	}

	/* not found insert work into list */
	list_item_prepend(&w->list, &queue->work[w->priority]);

	ll_set_timer(queue);

found:
	/* re-calc timer and re-arm */
	w->start = time;

	spin_unlock_irq(&queue->lock, flags);
}

static void reschedule_ll_task(struct task *w, uint64_t start)
{
	struct ll_schedule_data *queue =
		(*arch_schedule_get_data())->ll_sch_data;
	uint64_t time;
	struct ll_task_pdata *ll_pdata;

	ll_pdata = ll_sch_get_pdata(w);

	time = queue->ticks_per_msec * start / 1000;

	/* convert start micro seconds to CPU clock ticks */
	if (ll_pdata->flags & SOF_SCHEDULE_FLAG_SYNC)
		time += ll_get_timer(queue);
	else
		time += ll_shared_ctx->last_tick;

	reschedule(queue, w, time);
}

static int schedule_ll_task_cancel(struct task *w)
{
	struct ll_schedule_data *queue =
		(*arch_schedule_get_data())->ll_sch_data;
	struct task *ll_task;
	struct list_item *wlist;
	uint32_t flags;
	int ret = 0;

	spin_lock_irq(&queue->lock, flags);

	/* check to see if we are scheduled */
	list_for_item(wlist, &queue->work[w->priority]) {
		ll_task = container_of(wlist, struct task, list);

		/* found it */
		if (ll_task == w) {
			ll_clear_timer(queue);
			break;
		}
	}

	/* remove work from list */
	w->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&w->list);

	spin_unlock_irq(&queue->lock, flags);

	return ret;
}

static void schedule_ll_task_free(struct task *w)
{
	struct ll_schedule_data *queue =
		(*arch_schedule_get_data())->ll_sch_data;
	uint32_t flags;
	struct ll_task_pdata *ll_pdata;

	spin_lock_irq(&queue->lock, flags);

	/* release the resources */
	w->state = SOF_TASK_STATE_FREE;
	ll_pdata = ll_sch_get_pdata(w);
	rfree(ll_pdata);
	ll_sch_set_pdata(w, NULL);

	spin_unlock_irq(&queue->lock, flags);
}

static struct ll_schedule_data *work_new_queue(struct timesource_data *ts)
{
	struct ll_schedule_data *queue;
	int i;

	/* init work queue */
	queue = rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*queue));

	for (i = 0; i < LL_PRIORITIES; ++i)
		list_init(&queue->work[i]);

	spinlock_init(&queue->lock);
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

	/* register system timer */
	timer_register(&queue->ts->timer, queue_run, queue);

	return queue;
}

static int ll_scheduler_init(void)
{
	int ret = 0;
	int cpu;
	struct schedule_data *sch_data = *arch_schedule_get_data();
	struct timesource_data *ts;

	cpu = cpu_get_id();
	ts = &platform_generic_queue[cpu];

	sch_data->ll_sch_data = work_new_queue(ts);

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID) {
		ll_shared_ctx = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED,
					SOF_MEM_CAPS_RAM,
					sizeof(*ll_shared_ctx));
		atomic_init(&ll_shared_ctx->total_num_work, 0);
		atomic_init(&ll_shared_ctx->timer_clients, 0);
	}

	return ret;
}

/* initialise our work */
static int schedule_ll_task_init(struct task *w, uint32_t xflags)
{
	struct ll_task_pdata *ll_pdata;

	if (ll_sch_get_pdata(w))
		return -EEXIST;

	ll_pdata = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
			   SOF_MEM_CAPS_RAM, sizeof(*ll_pdata));

	if (!ll_pdata) {
		trace_error(0, "schedule_ll_task_init() error: alloc failed");
		return -ENOMEM;
	}

	ll_sch_set_pdata(w, ll_pdata);

	ll_pdata->flags = xflags;

	return 0;
}

static void ll_scheduler_free(void)
{
	struct ll_schedule_data *queue =
		(*arch_schedule_get_data())->ll_sch_data;
	uint32_t flags;
	int i;

	spin_lock_irq(&queue->lock, flags);

	timer_unregister(&queue->ts->timer);

	notifier_unregister(&queue->notifier);

	for (i = 0; i < LL_PRIORITIES; ++i)
		list_item_del(&queue->work[i]);

	spin_unlock_irq(&queue->lock, flags);
}

struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_init	= schedule_ll_task_init,
	.schedule_task_running	= NULL,
	.schedule_task_complete	= NULL,
	.reschedule_task	= reschedule_ll_task,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.schedule_task_free	= schedule_ll_task_free,
	.scheduler_init		= ll_scheduler_init,
	.scheduler_free		= ll_scheduler_free,
	.scheduler_run		= NULL
};
