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
	int timer;			/* timer this queue uses */
	int irq;			/* IRQ this timer uses */
	spinlock_t lock;
	struct notifier notifier;	/* notify CPU freq changes */
};

/* generic system work queue */
static struct work_queue *queue_;

/* is there any work pending in the current time window ? */
static int is_work_pending(struct work_queue *queue)
{
	struct list_head *wlist;
	struct work *work;
	uint32_t win_end, win_start;
	int pending, pending_count = 0;

	/* get the current valid window of work */
	win_end = timer_get_system();
	win_start = win_end - queue->window_size;

	/* correct the pending flag window for overflow */
	if (win_end > win_start) {

		/* mark each valid work item in this time period as pending */
		list_for_each(wlist, &queue->work) {

			work = container_of(wlist, struct work, list);

			/* if work has timed out then mark it as pending to run */
			if (work->count >= win_start && work->count <= win_end) {
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
			if (work->count >= win_start && work->count <= win_end) {
				work->pending = 0;
			} else {
				work->pending = 1;
				pending_count++;
			}
		}
	}

	return pending_count;
}

/* run all pending work */
static int run_work(struct work_queue *queue)
{
	struct list_head *wlist, *tlist;
	struct work *work;

	/* check each work item in queue for pending */
	list_for_each_safe(wlist, tlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		/* run work if its pending and remove from the queue */
		if (work->pending) {
			work->cb(work->cb_data);
			list_del(&work->list);
		}
	}
}

/* calculate next timeout */
static uint32_t queue_get_next_timeout(struct work_queue *queue)
{
	struct list_head *wlist;
	struct work *work;
	uint32_t ticks = MAX_INT, current, t;

	current = timer_get_system();

	/* find time for next work */
	list_for_each(wlist, &queue->work) {

		work = container_of(wlist, struct work, list);

		/* does work run in next cycle ? */
		if (work->count < current) {
			t = MAX_INT - current + work->count;
		} else {
			t = work->count - current;
		}

		/* is work next ? */
		if (t < ticks)
			ticks = t;
	}

	return ticks;
}

static void queue_reschedule(struct work_queue *queue)
{
	queue->timeout = queue_get_next_timeout(queue);
	timer_set(queue->timer, queue->timeout);
	timer_enable(queue->timer);
}

/* run the work queue */
static void queue_run(void *data)
{
	struct work_queue *queue = (struct work_queue *)data;

	spin_lock_local_irq(&queue->lock, queue->irq);

	/* work can take variable time to complete so we re-check the
	  queue after running all the pending work to make sure no new work
	  is pending */
	while (is_work_pending(queue))
		run_work(queue);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock_local_irq(&queue_->lock, queue_->irq);
}

static void work_notify(int message, void *cb_data, void *event_data)
{

}

void work_schedule(struct work_queue *queue, struct work *w, int timeout)
{
	/* convert timeout millisecs to CPU clock ticks */
	w->count = clock_ms_to_ticks(CLK_CPU, timeout);

	spin_lock_local_irq(&queue->lock, queue->irq);

	/* insert work into list */
	list_add(&w->list, &queue->work);

	/* re-calc timer and re-arm */
	queue_reschedule(queue);

	spin_unlock_local_irq(&queue->lock, queue->irq);
}

void work_schedule_default(struct work *w, int timeout)
{
	/* convert timeout millisecs to CPU clock ticks */
	w->count = clock_ms_to_ticks(CLK_CPU, timeout);

	spin_lock_local_irq(&queue_->lock, queue_->irq);

	/* insert work into list */
	list_add(&w->list, &queue_->work);

	/* re-calc timer and re-arm */
	queue_reschedule(queue_);

	spin_unlock_local_irq(&queue_->lock, queue_->irq);
}

//TODO: add notifier for clock changes in order to re-calc timeouts
void init_system_workq(void)
{
	/* init system work queue */
	queue_ = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*queue_));
	queue_->timer = REEF_SYS_TIMER;
	queue_->irq = timer_get_irq(REEF_SYS_TIMER);
	queue_->window_size = REEF_WORK_WINDOW;
	list_init(&queue_->work);
	queue_->notifier.cb = work_notify;
	queue_->notifier.cb_data = queue_;
	queue_->notifier.id = NOTIFIER_ID_CPU_FREQ;
	notifier_register(&queue_->notifier);

	/* register system timer */
	timer_register(REEF_SYS_TIMER, queue_run, queue_);
}
