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
	uint32_t ticks_per_usec;	/* ticks per usec */
	uint32_t ticks_per_msec;	/* ticks per msec */
	uint64_t run_ticks;	/* ticks when last run */
};

static inline int work_set_timer(struct work_queue *queue, uint64_t ticks)
{
	int ret;

	ret = queue->ts->timer_set(&queue->ts->timer, ticks);
	timer_enable(&queue->ts->timer);

	return ret;
}

static inline void work_clear_timer(struct work_queue *queue)
{
	queue->ts->timer_clear(&queue->ts->timer);
	timer_disable(&queue->ts->timer);
}

static inline uint64_t work_get_timer(struct work_queue *queue)
{
	return queue->ts->timer_get(&queue->ts->timer);
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

	if (reschedule_usecs % 1000)
		next_d = queue->ticks_per_usec * reschedule_usecs;
	else
		next_d = queue->ticks_per_msec * (reschedule_usecs / 1000);

	if (work->flags & WORK_SYNC) {
		work->timeout += next_d;
	} else {
		/* calc next run based on work request */
		work->timeout = next_d + queue->run_ticks;
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

	/* check each work item in queue for pending */
	list_for_item_safe(wlist, tlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		/* run work if its pending and remove from the queue */
		if (work->pending) {

			udelay = (work_get_timer(queue) - work->timeout) /
				queue->ticks_per_usec;

			/* work can run in non atomic context */
			spin_unlock_irq(&queue->lock, *flags);
			reschedule_usecs = work->cb(work->cb_data, udelay);
			spin_lock_irq(&queue->lock, *flags);

			/* do we need reschedule this work ? */
			if (reschedule_usecs == 0)
				list_item_del(&work->list);
			else {
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

/* calculate next timeout */
static void queue_get_next_timeout(struct work_queue *queue)
{
	struct list_item *wlist;
	struct work *work;
	uint64_t delta = ULONG_LONG_MAX;
	uint64_t current;
	uint64_t d;
	uint64_t ticks;

	/* only recalc if work list not empty */
	if (list_is_empty(&queue->work)) {
		queue->timeout = 0;
		return;
	}

	ticks = current = work_get_timer(queue);

	/* find time for next work */
	list_for_item(wlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		d = calc_delta_ticks(current, work->timeout);

		/* is work next ? */
		if (d < delta) {
			ticks = work->timeout;
			delta = d;
		}
	}

	queue->timeout = ticks;
}

/* re calculate timers for queue after CPU frequency change */
static void queue_recalc_timers(struct work_queue *queue,
	struct clock_notify_data *clk_data)
{
	struct list_item *wlist;
	struct work *work;
	uint64_t delta_ticks;
	uint64_t delta_usecs;
	uint64_t current;

	/* get current time */
	current = work_get_timer(queue);

	/* re calculate timers for each work item */
	list_for_item(wlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		delta_ticks = calc_delta_ticks(current, work->timeout);
		delta_usecs = delta_ticks / clk_data->old_ticks_per_usec;

		/* is work within next msec, then schedule it now */
		if (delta_usecs > 0)
			work->timeout = current + queue->ticks_per_usec * delta_usecs;
		else
			work->timeout = current + (queue->ticks_per_usec >> 3);
	}
}

static void queue_reschedule(struct work_queue *queue)
{
	queue_get_next_timeout(queue);

	if (queue->timeout)
		work_set_timer(queue, queue->timeout);
}

/* run the work queue */
static void queue_run(void *data)
{
	struct work_queue *queue = (struct work_queue *)data;
	uint32_t flags;

	/* clear interrupt */
	work_clear_timer(queue);

	spin_lock_irq(&queue->lock, flags);

	queue->run_ticks = work_get_timer(queue);

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
		queue->ticks_per_usec = clock_us_to_ticks(queue->ts->clk, 1);
		queue->window_size =
			queue->ticks_per_usec * PLATFORM_WORKQ_WINDOW;
		queue_recalc_timers(queue, clk_data);
		queue_reschedule(queue);
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
	if (timeout % 1000)
		w->timeout = queue->ticks_per_usec * timeout +
			work_get_timer(queue);
	else
		w->timeout = queue->ticks_per_msec * (timeout / 1000) +
			work_get_timer(queue);

	/* insert work into list */
	list_item_prepend(&w->list, &queue->work);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

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

found:
	/* re-calc timer and re-arm */
	w->timeout = time;
	queue_reschedule(queue);

	spin_unlock_irq(&queue->lock, flags);
}

void work_reschedule(struct work_queue *queue, struct work *w, uint64_t timeout)
{
	uint64_t time;

	/* convert timeout micro seconds to CPU clock ticks */
	time = queue->ticks_per_usec * timeout + work_get_timer(queue);

	reschedule(queue, w, time);
}

void work_reschedule_default(struct work *w, uint64_t timeout)
{
	struct work_queue *queue = *arch_work_queue_get();
	uint64_t time;

	/* convert timeout micro seconds to CPU clock ticks */
	time = queue->ticks_per_usec * timeout + work_get_timer(queue);

	reschedule(queue, w, time);
}

void work_reschedule_default_at(struct work *w, uint64_t time)
{
	reschedule(*arch_work_queue_get(), w, time);
}

void work_cancel(struct work_queue *queue, struct work *w)
{
	uint32_t flags;

	spin_lock_irq(&queue->lock, flags);

	/* remove work from list */
	list_item_del(&w->list);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

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
	queue->ts = ts;
	queue->ticks_per_usec = clock_us_to_ticks(queue->ts->clk, 1);
	queue->ticks_per_msec = clock_ms_to_ticks(queue->ts->clk, 1);
	queue->window_size = queue->ticks_per_usec * PLATFORM_WORKQ_WINDOW;

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID) {
		/* notification of clk changes */
		queue->notifier.cb = work_notify;
		queue->notifier.cb_data = queue;
		queue->notifier.id = ts->notifier;
		notifier_register(&queue->notifier);
	}

	/* register system timer */
	timer_register(&queue->ts->timer, queue_run, queue);

	return queue;
}

void init_system_workq(struct work_queue_timesource *ts)
{
	struct work_queue **queue = arch_work_queue_get();
	*queue = work_new_queue(ts);
}

void free_system_workq(void)
{
	struct work_queue **queue = arch_work_queue_get();
	uint32_t flags;

	spin_lock_irq(&(*queue)->lock, flags);

	timer_unregister(&(*queue)->ts->timer);

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID)
		notifier_unregister(&(*queue)->notifier);

	list_item_del(&(*queue)->work);

	spin_unlock_irq(&(*queue)->lock, flags);
}
