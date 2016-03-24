/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#include <reef/work.h>
#include <reef/timer.h>
#include <reef/list.h>
#include <reef/clock.h>
#include <reef/alloc.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/notifier.h>
#include <reef/debug.h>
#include <platform/clk.h>
#include <platform/platform.h>

/*
 * Generic delayed work queue support.
 *
 * Work can be queued to run after a millisecond timeout on either the system
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
	struct list_head work;		/* list of work */
	uint32_t timeout;		/* timeout for next queue run */
	uint32_t window_size;		/* window size for pending work */
	spinlock_t lock;
	struct notifier notifier;	/* notify CPU freq changes */
	const struct work_queue_timesource *ts;	/* time source for work queue */
	uint32_t ticks_per_usec;	/* ticks per msec */
	uint32_t run_ticks;	/* ticks when last run */
};

/* generic system work queue */
static struct work_queue *queue_;

static inline void work_set_timer(struct work_queue *queue, uint32_t ticks)
{
	queue->ts->timer_set(queue->ts->timer, ticks);
}

static inline void work_clear_timer(struct work_queue *queue)
{
	queue->ts->timer_clear(queue->ts->timer);
}

static inline uint32_t work_get_timer(struct work_queue *queue)
{
	return queue->ts->timer_get(queue->ts->timer);
}

/* is there any work pending in the current time window ? */
static int is_work_pending(struct work_queue *queue)
{
	struct list_head *wlist;
	struct work *work;
	uint32_t win_end, win_start;
	int pending_count = 0;

	/* get the current valid window of work */
	win_end = work_get_timer(queue);
	win_start = win_end - queue->window_size;

	/* correct the pending flag window for overflow */
	if (win_end > win_start) {

		/* mark each valid work item in this time period as pending */
		list_for_each(wlist, &queue->work) {

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
		list_for_each(wlist, &queue->work) {

			work = container_of(wlist, struct work, list);

			/* if work has timed out then mark it as pending to run */
			if (work->timeout >= win_start && work->timeout <= win_end) {
				work->pending = 0;
			} else {
				work->pending = 1;
				pending_count++;
			}
		}
	}

	return pending_count;
}

static inline void work_next_timeout(struct work_queue *queue,
	struct work *work, uint32_t reschedule_usecs)
{
	/* reschedule work */
	if (work->flags & WORK_SYNC) {
		work->timeout += queue->ticks_per_usec * reschedule_usecs;
	} else {
		/* calc next run based on work request */
		work->timeout = queue->ticks_per_usec *
			reschedule_usecs + queue->run_ticks;
	}
}

/* run all pending work */
static void run_work(struct work_queue *queue)
{
	struct list_head *wlist, *tlist;
	struct work *work;
	uint32_t reschedule_usecs, current, udelay;

	/* check each work item in queue for pending */
	list_for_each_safe(wlist, tlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		/* run work if its pending and remove from the queue */
		if (work->pending) {

			udelay = (work_get_timer(queue) - work->timeout) /
				queue->ticks_per_usec;
			reschedule_usecs = work->cb(work->cb_data, udelay);

			/* do we need reschedule this work ? */
			if (reschedule_usecs == 0)
				list_del(&work->list);
			else {
				/* get next work timeout */	
				work_next_timeout(queue, work, reschedule_usecs);

				/* did work take too long ? */
				current = work_get_timer(queue);
				if (work->timeout <= current) {
					/* TODO: inform users */
					work->timeout = current;
				}
			}
		}
	}
}

static inline uint32_t calc_delta_ticks(uint32_t current, uint32_t work)
{
	uint32_t max = MAX_INT;

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
	struct list_head *wlist;
	struct work *work;
	uint32_t delta = MAX_INT, current, d, ticks;

	/* only recalc if work list not empty */
	if (list_empty(&queue->work)) {
		queue->timeout = 0;
		return;
	}

	ticks = current = work_get_timer(queue);

	/* find time for next work */
	list_for_each(wlist, &queue->work) {

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
	struct list_head *wlist;
	struct work *work;
	uint32_t delta_ticks, delta_msecs, current;

	/* get current time */
	current = work_get_timer(queue);

	/* re calculate timers for each work item */
	list_for_each(wlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		delta_ticks = calc_delta_ticks(current, work->timeout);
		delta_msecs = delta_ticks / clk_data->old_ticks_per_usec;

		/* is work within next msec, then schedule it now */
		if (delta_msecs > 0)
			work->timeout = current + queue->ticks_per_usec * delta_msecs;
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

	/* clear and disable interrupt */
	work_clear_timer(queue);
	timer_disable(queue->ts->timer);

	queue->run_ticks = work_get_timer(queue);

	/* work can take variable time to complete so we re-check the
	  queue after running all the pending work to make sure no new work
	  is pending */
	while (is_work_pending(queue))
		run_work(queue);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	/* the interrupt may need to be re-enabled outside IRQ context.
	 * wait_for_interrupt() will do this for us */
	timer_enable(queue->ts->timer);
}

/* notification of CPU frequency changes - atomic PRE and POST sequence */
static void work_notify(int message, void *data, void *event_data)
{
	struct work_queue *queue = (struct work_queue *)data;
	struct clock_notify_data *clk_data =
		(struct clock_notify_data *)event_data;

	/* we need to re-caclulate timer when CPU freqency changes */
	if (message == CLOCK_NOTIFY_POST) {

		/* CPU frequency update complete */
		/* scale the window size to clock speed */
		queue->ticks_per_usec = clock_us_to_ticks(queue->ts->clk, 1);
		queue->window_size = queue->ticks_per_usec * 2000;
		queue_recalc_timers(queue, clk_data);
		queue_reschedule(queue);
		timer_enable(queue->ts->timer);
	} else if (message == CLOCK_NOTIFY_PRE) {
		/* CPU frequency update pending */
		timer_disable(queue->ts->timer);
	}
}

void work_schedule(struct work_queue *queue, struct work *w, uint32_t timeout)
{
	spin_lock_local_irq(&queue->lock, queue->ts->timer);

	/* convert timeout millisecs to CPU clock ticks */
	w->timeout = queue->ticks_per_usec * timeout + work_get_timer(queue);

	/* insert work into list */
	list_add(&w->list, &queue->work);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock_local_irq(&queue->lock, queue->ts->timer);
}

void work_cancel(struct work_queue *queue, struct work *w)
{
	spin_lock_local_irq(&queue->lock, queue->ts->timer);

	/* remove work from list */
	list_del(&w->list);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock_local_irq(&queue->lock, queue->ts->timer);
}

void work_schedule_default(struct work *w, uint32_t timeout)
{
	spin_lock_local_irq(&queue_->lock, queue_->ts->timer);

	/* convert timeout millisecs to CPU clock ticks */
	w->timeout = queue_->ticks_per_usec * timeout + work_get_timer(queue_);

	/* insert work into list */
	list_add(&w->list, &queue_->work);

	/* re-calc timer and re-arm */
	queue_reschedule(queue_);

	spin_unlock_local_irq(&queue_->lock, queue_->ts->timer);
}

void work_cancel_default(struct work *w)
{
	spin_lock_local_irq(&queue_->lock, queue_->ts->timer);

	/* remove work from list */
	list_del(&w->list);

	/* re-calc timer and re-arm */
	queue_reschedule(queue_);

	spin_unlock_local_irq(&queue_->lock, queue_->ts->timer);
}

struct work_queue *work_new_queue(const struct work_queue_timesource *ts)
{
	struct work_queue *queue;

	/* init work queue */
	queue = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*queue_));

	list_init(&queue->work);
	spinlock_init(&queue->lock);
	queue->ts = ts;
	queue->ticks_per_usec = clock_us_to_ticks(queue->ts->clk, 1);
	queue->window_size = queue->ticks_per_usec * 2000;

	/* notification of clk changes */
	queue->notifier.cb = work_notify;
	queue->notifier.cb_data = queue;
	queue->notifier.id = NOTIFIER_ID_CPU_FREQ;
	notifier_register(&queue->notifier);

	/* register system timer */
	timer_register(queue->ts->timer, queue_run, queue);

	return queue;
}

void init_system_workq(const struct work_queue_timesource *ts)
{
	queue_ = work_new_queue(ts);
}
