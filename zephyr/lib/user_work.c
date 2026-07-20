/*
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file is mostly copied from Zephyr's user_work.c. Retain the original
 * license header above.
 */

/*
 * On Xtensa, with CONFIG_SCHED_CPU_MASK_PIN_ONLY=y, k_thread_cpu_pin()
 * cannot safely change a thread's core after it has been active on another
 * core. Enabling CONFIG_SCHED_CPU_MASK_PIN_ONLY also enables an optimization
 * in the thread-switch code: because Zephyr assumes that a thread will resume
 * on the same core, it does not write back the thread's cached stack.
 *
 * If k_thread_cpu_pin() subsequently moves the thread to another core, the
 * cached stack on the new core contains garbage. The cached stack was not
 * written back on the previous core and is not invalidated on the new core.
 * With CONFIG_SCHED_CPU_MASK_PIN_ONLY disabled, Zephyr writes back the stack
 * when the thread becomes inactive and invalidates it when the thread becomes
 * active on another core.
 *
 * k_work_user_queue_start() starts its worker thread on core 0, leaving no
 * safe way to re-pin it when CONFIG_SCHED_CPU_MASK_PIN_ONLY=y. Because we do
 * not want to disable CONFIG_SCHED_CPU_MASK_PIN_ONLY or use an uncached stack,
 * this file implements a copy of k_work_user_queue_start() that does not
 * start the worker thread. The caller configures the thread, for example by
 * pinning it to the required core, and then calls k_thread_start().
 */

#include <zephyr/kernel.h>

/* This is an intact copy of Zephyr's z_work_user_q_main(). */
static void z_work_user_q_main(void *work_q_ptr, void *p2, void *p3)
{
	struct k_work_user_q *work_q = work_q_ptr;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		struct k_work_user *work;
		k_work_user_handler_t handler;

		work = k_queue_get(&work_q->queue, K_FOREVER);
		if (work == NULL) {
			continue;
		}

		handler = work->handler;
		__ASSERT(handler != NULL, "handler must be provided");

		/* Reset pending state so it can be resubmitted by handler */
		if (atomic_test_and_clear_bit(&work->flags,
					      K_WORK_USER_STATE_PENDING)) {
			handler(work);
		}

		/* Make sure we don't hog up the CPU if the FIFO never (or
		 * very rarely) gets empty.
		 */
		k_yield();
	}
}

/* This is Zephyr's k_work_user_queue_start() with k_thread_start() removed. */
void sof_work_user_queue_create(struct k_work_user_q *work_q, k_thread_stack_t *stack,
			    size_t stack_size, int prio, const char *name)
{
	k_queue_init(&work_q->queue);

	/* Created worker thread will inherit object permissions and memory
	 * domain configuration of the caller
	 */
	k_thread_create(&work_q->thread, stack, stack_size, z_work_user_q_main,
			work_q, NULL, NULL, prio, K_USER | K_INHERIT_PERMS,
			K_FOREVER);
	k_object_access_grant(&work_q->queue, &work_q->thread);
	if (name != NULL) {
		k_thread_name_set(&work_q->thread, name);
	}
}
