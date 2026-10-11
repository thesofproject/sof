// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

/**
 * @file rtos/kernel.h
 * @brief RTOS kernel threading and privilege helpers.
 * @ingroup rtos_api
 */

#ifndef __ZEPHYR_RTOS_KERNEL_H__
#define __ZEPHYR_RTOS_KERNEL_H__

#include <zephyr/kernel.h>

#include <stdbool.h>

/**
 * @brief Test if a thread is running with user-space (unprivileged) options.
 * @param thread Pointer to the Zephyr thread control structure.
 * @return True if the thread has K_USER option set, false otherwise.
 */
static inline bool thread_is_userspace(struct k_thread *thread)
{
	return !!(thread->base.user_options & K_USER);
}

#endif /* __ZEPHYR_RTOS_KERNEL_H__ */
