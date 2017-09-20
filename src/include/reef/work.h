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

#ifndef __INCLUDE_WORK__
#define __INCLUDE_WORK__

#include <stdint.h>
#include <reef/list.h>
#include <reef/timer.h>

struct work_queue;

/* flags */
#define WORK_ASYNC	(0 << 0)	/* default - work is scheduled asynchronously */
#define WORK_SYNC	(1 << 0)	/* work is scheduled synchronously */

struct work {
	uint32_t (*cb)(void*, uint32_t udelay);	/* returns reschedule timeout in msecs */
	void *cb_data;
	struct list_item list;
	uint32_t timeout;
	uint32_t pending;
	uint32_t flags;
};

struct work_queue_timesource {
	struct timer timer;
	int clk;
	int notifier;
	int (*timer_set)(struct timer *, uint64_t ticks);
	void (*timer_clear)(struct timer *);
	uint64_t (*timer_get)(struct timer *);
};

/* initialise our work */
#define work_init(w, x, xd, xflags) \
	(w)->cb = x; \
	(w)->cb_data = xd; \
	(w)->flags = xflags;

/* schedule/cancel work on work queue */
void work_schedule(struct work_queue *queue, struct work *w, uint64_t timeout);
void work_reschedule(struct work_queue *queue, struct work *w, uint64_t timeout);
void work_cancel(struct work_queue *queue, struct work *work);

/* schedule/cancel work on default system work queue */
void work_schedule_default(struct work *work, uint64_t timeout);
void work_reschedule_default(struct work *work, uint64_t timeout);
void work_reschedule_default_at(struct work *w, uint64_t time);
void work_cancel_default(struct work *work);

/* create new work queue */
struct work_queue *work_new_queue(struct work_queue_timesource *ts);

/* init system workq */
void init_system_workq(struct work_queue_timesource *ts);

#endif
