/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_USER_WORK_H__
#define __ZEPHYR_RTOS_USER_WORK_H__

#include <zephyr/kernel.h>

/**
 * Create a userspace work queue without starting its worker thread.
 *
 * Based on Zephyr's k_work_user_queue_start(), but does not start its
 * worker thread. The caller must configure the thread, for example, pin it
 * to the appropriate core, and call k_thread_start() when it is ready to
 * process work.
 *
 * @param work_q Work queue to initialize.
 * @param stack Worker thread stack.
 * @param stack_size Size of @p stack in bytes.
 * @param prio Worker thread priority.
 * @param name Optional worker thread name.
 */
void sof_work_user_queue_create(struct k_work_user_q *work_q, k_thread_stack_t *stack,
				size_t stack_size, int prio, const char *name);

#endif /* __ZEPHYR_RTOS_USER_WORK_H__ */
