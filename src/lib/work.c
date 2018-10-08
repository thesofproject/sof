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

#include <sof/work.h>
#include <sof/timer.h>
#include <sof/list.h>
#include <sof/clock.h>
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

struct work_queue {
	struct list_item work;		/* list of work */
	uint64_t timeout;		/* timeout for next queue run */
	uint32_t window_size;		/* window size for pending work */
	spinlock_t lock;
	struct notifier notifier;	/* notify CPU freq changes */
	struct work_queue_timesource *ts;	/* time source for work queue */
	uint32_t ticks_per_msec;	/* ticks per msec */
	atomic_t num_work;	/* number of queued work items */
};

struct work_queue_shared_context {
	atomic_t total_num_work;	/* number of total queued work items */
	atomic_t timer_clients;		/* number of timer clients */
	uint64_t last_tick;		/* time of last tick */

	/* registered timers */
	struct timer *timers[PLATFORM_CORE_COUNT];
};

static struct work_queue_shared_context *work_shared_ctx;

/* calculate next timeout */
static inline uint64_t queue_calc_next_timeout(struct work_queue *queue,
					       uint64_t start)
{
	return queue->ticks_per_msec * queue->timeout / 1000 + start;
}

static inline uint64_t work_get_timer(struct work_queue *queue)
{
	return queue->ts->timer_get(&queue->ts->timer);
}

static inline void work_set_timer(struct work_queue *queue)
{
	uint64_t ticks;

	if (atomic_add(&queue->num_work, 1) == 1)
		work_shared_ctx->timers[cpu_get_id()] = &queue->ts->timer;

	if (atomic_add(&work_shared_ctx->total_num_work, 1) == 1) {
		ticks = queue_calc_next_timeout(queue, work_get_timer(queue));
		work_shared_ctx->last_tick = ticks;
		queue->ts->timer_set(&queue->ts->timer, ticks);
		atomic_add(&work_shared_ctx->timer_clients, 1);
		timer_enable(&queue->ts->timer);
	}
}

static inline void work_clear_timer(struct work_queue *queue)
{
	if (!atomic_sub(&work_shared_ctx->total_num_work, 1))
		queue->ts->timer_clear(&queue->ts->timer);

	if (!atomic_sub(&queue->num_work, 1)) {
		timer_disable(&queue->ts->timer);
		atomic_sub(&work_shared_ctx->timer_clients, 1);
		work_shared_ctx->timers[cpu_get_id()] = NULL;
	}
}

/* is there any work pending in the current time window ? */
static int is_work_pending(struct work_queue *queue)
{
	struct list_item *wlist;
	struct work *work;
	uint64_t win_end;
	uint64_t win_start;
	int pending_count = 0;

	/* get the current valid window of work */
	win_end = work_get_timer(queue);
	win_start = win_end - queue->window_size;

	/* correct the pending flag window for overflow */
	if (win_end > win_start) {

		/* mark each valid work item in this time period as pending */
		list_for_item(wlist, &queue->work) {

			work = container_of(wlist, struct work, list);

			/* if work has timed out then mark it as pending to run */
			if (work->timeout >= win_start && work->timeout <= win_end) {
				work->pending = 1;
				pending_count++;
			} else {
				work->pending = 0;
			}
		}
	} else {

		/* mark each valid work item in this time period as pending */
		list_for_item(wlist, &queue->work) {

			work = container_of(wlist, struct work, list);

			/* if work has timed out then mark it as pending to run */
			if (work->timeout <= win_end ||
				(work->timeout >= win_start &&
				work->timeout < ULONG_LONG_MAX)) {
				work->pending = 1;
				pending_count++;
			} else {
				work->pending = 0;
			}
		}
	}

	return pending_count;
}

static inline void work_next_timeout(struct work_queue *queue,
	struct work *work, uint64_t reschedule_usecs)
{
	/* reschedule work */
	uint64_t next_d = 0;

	next_d = queue->ticks_per_msec * reschedule_usecs / 1000;

	if (work->flags & WORK_SYNC) {
		work->timeout += next_d;
	} else {
		/* calc next run based on work request */
		work->timeout = next_d + work_shared_ctx->last_tick;
	}
}

/* run all pending work */
static void run_work(struct work_queue *queue, uint32_t *flags)
{
	struct list_item *wlist;
	struct list_item *tlist;
	struct work *work;
	uint64_t reschedule_usecs;
	uint64_t udelay;
	int cpu = cpu_get_id();

	/* check each work item in queue for pending */
	list_for_item_safe(wlist, tlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		/* run work if its pending and remove from the queue */
		if (work->pending) {

			udelay = ((work_get_timer(queue) - work->timeout) /
				queue->ticks_per_msec) * 1000;

			/* work can run in non atomic context */
			spin_unlock_irq(&queue->lock, *flags);
			reschedule_usecs = work->cb(work->cb_data, udelay);
			spin_lock_irq(&queue->lock, *flags);

			/* do we need reschedule this work ? */
			if (reschedule_usecs == 0) {
				list_item_del(&work->list);
				atomic_sub(&work_shared_ctx->total_num_work,
					   1);

				/* don't enable irq, if no more work to do */
				if (!atomic_sub(&queue->num_work, 1))
					work_shared_ctx->timers[cpu] = NULL;
			} else {
				/* get next work timeout */
				work_next_timeout(queue, work, reschedule_usecs);
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
	} else
		return work - current;
}

/* re calculate timers for queue after CPU frequency change */
static void queue_recalc_timers(struct work_queue *queue,
				struct clock_notify_data *clk_data)
{
	struct list_item *wlist;
	struct work *work;
	uint64_t delta_ticks;
	uint64_t delta_msecs;
	uint64_t current;

	/* get current time */
	current = work_get_timer(queue);

	/* re calculate timers for each work item */
	list_for_item(wlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		delta_ticks = calc_delta_ticks(current, work->timeout);
		delta_msecs = delta_ticks / clk_data->old_ticks_per_msec;

		/* is work within next msec, then schedule it now */
		if (delta_msecs > 0)
			work->timeout = current + queue->ticks_per_msec *
				delta_msecs;
		else
			work->timeout = current + (queue->ticks_per_msec >> 3);
	}
}

/* enable all registered timers */
static void queue_enable_registered_timers(void)
{
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (work_shared_ctx->timers[i]) {
			atomic_add(&work_shared_ctx->timer_clients, 1);
			timer_enable(work_shared_ctx->timers[i]);
		}
	}
}

/* reschedule queue */
static void queue_reschedule(struct work_queue *queue)
{
	uint64_t ticks;

	/* clear only if all timer clients are done */
	if (!atomic_sub(&work_shared_ctx->timer_clients, 1)) {
		/* clear timer */
		queue->ts->timer_clear(&queue->ts->timer);

		/* re-arm only if there is work to do */
		if (atomic_read(&work_shared_ctx->total_num_work)) {
			/* re-arm timer */
			ticks = queue_calc_next_timeout
				(queue,
				 work_shared_ctx->last_tick);
			work_shared_ctx->last_tick = ticks;
			queue->ts->timer_set(&queue->ts->timer, ticks);

			queue_enable_registered_timers();
		}
	}
}

/* run the work queue */
static void queue_run(void *data)
{
	struct work_queue *queue = (struct work_queue *)data;
	uint32_t flags;

	timer_disable(&queue->ts->timer);

	spin_lock_irq(&queue->lock, flags);

	/* work can take variable time to complete so we re-check the
	  queue after running all the pending work to make sure no new work
	  is pending */
	while (is_work_pending(queue))
		run_work(queue, &flags);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock_irq(&queue->lock, flags);
}

/* notification of CPU frequency changes - atomic PRE and POST sequence */
static void work_notify(int message, void *data, void *event_data)
{
	struct work_queue *queue = (struct work_queue *)data;
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

void work_schedule(struct work_queue *queue, struct work *w, uint64_t timeout)
{
	struct work *work;
	struct list_item *wlist;
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

	/* check to see if we are already scheduled ? */
	list_for_item(wlist, &queue->work) {
		work = container_of(wlist, struct work, list);

		/* keep original timeout */
		if (work == w)
			goto out;
	}

	/* convert timeout micro seconds to CPU clock ticks */
	w->timeout = queue->ticks_per_msec * timeout / 1000 +
		work_get_timer(queue);

	/* insert work into list */
	list_item_prepend(&w->list, &queue->work);

	work_set_timer(queue);

out:
	spin_unlock_irq(&queue->lock, flags);
}

void work_schedule_default(struct work *w, uint64_t timeout)
{
	work_schedule(*arch_work_queue_get(), w, timeout);
}

static void reschedule(struct work_queue *queue, struct work *w, uint64_t time)
{
	struct work *work;
	struct list_item *wlist;
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

	/* check to see if we are already scheduled ? */
	list_for_item(wlist, &queue->work) {
		work = container_of(wlist, struct work, list);

		/* found it */
		if (work == w)
			goto found;
	}

	/* not found insert work into list */
	list_item_prepend(&w->list, &queue->work);

	work_set_timer(queue);

found:
	/* re-calc timer and re-arm */
	w->timeout = time;

	spin_unlock_irq(&queue->lock, flags);
}

void work_reschedule(struct work_queue *queue, struct work *w, uint64_t timeout)
{
	uint64_t time;

	/* convert timeout micro seconds to CPU clock ticks */
	time = queue->ticks_per_msec * timeout / 1000 +
		work_get_timer(queue);

	reschedule(queue, w, time);
}

void work_reschedule_default(struct work *w, uint64_t timeout)
{
	struct work_queue *queue = *arch_work_queue_get();
	uint64_t time;

	/* convert timeout micro seconds to CPU clock ticks */
	time = queue->ticks_per_msec * timeout / 1000 +
		work_get_timer(queue);

	reschedule(queue, w, time);
}

void work_reschedule_default_at(struct work *w, uint64_t time)
{
	reschedule(*arch_work_queue_get(), w, time);
}

void work_cancel(struct work_queue *queue, struct work *w)
{
	struct work *work;
	struct list_item *wlist;
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

	/* check to see if we are scheduled */
	list_for_item(wlist, &queue->work) {
		work = container_of(wlist, struct work, list);

		/* found it */
		if (work == w) {
			work_clear_timer(queue);
			break;
		}
	}

	/* remove work from list */
	list_item_del(&w->list);

	spin_unlock_irq(&queue->lock, flags);
}

void work_cancel_default(struct work *w)
{
	work_cancel(*arch_work_queue_get(), w);
}

struct work_queue *work_new_queue(struct work_queue_timesource *ts)
{
	struct work_queue *queue;

	/* init work queue */
	queue = rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*queue));

	list_init(&queue->work);
	spinlock_init(&queue->lock);
	atomic_init(&queue->num_work, 0);
	queue->ts = ts;
	queue->ticks_per_msec = clock_ms_to_ticks(queue->ts->clk, 1);
	queue->window_size = queue->ticks_per_msec *
		PLATFORM_WORKQ_WINDOW / 1000;

	/* TODO: configurable through IPC */
	queue->timeout = PLATFORM_WORKQ_DEFAULT_TIMEOUT;

	/* notification of clk changes */
	queue->notifier.cb = work_notify;
	queue->notifier.cb_data = queue;
	queue->notifier.id = ts->notifier;
	notifier_register(&queue->notifier);

	/* register system timer */
	timer_register(&queue->ts->timer, queue_run, queue);

	return queue;
}

void init_system_workq(struct work_queue_timesource *ts)
{
	struct work_queue **queue = arch_work_queue_get();

	*queue = work_new_queue(ts);

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID) {
		work_shared_ctx = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED,
					  SOF_MEM_CAPS_RAM,
					  sizeof(*work_shared_ctx));
		atomic_init(&work_shared_ctx->total_num_work, 0);
		atomic_init(&work_shared_ctx->timer_clients, 0);
	}
}

void free_system_workq(void)
{
	struct work_queue **queue = arch_work_queue_get();
	uint32_t flags;

	spin_lock_irq(&(*queue)->lock, flags);

	timer_unregister(&(*queue)->ts->timer);

	notifier_unregister(&(*queue)->notifier);

	list_item_del(&(*queue)->work);

	spin_unlock_irq(&(*queue)->lock, flags);
}
