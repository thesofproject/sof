// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

#ifndef __ZEPHYR_RTOS_KERNEL_H__
#define __ZEPHYR_RTOS_KERNEL_H__

#include <zephyr/kernel.h>

#include <stdbool.h>

static inline bool thread_is_userspace(struct k_thread *thread)
{
	return !!(thread->base.user_options & K_USER);
}

#endif /* __ZEPHYR_RTOS_KERNEL_H__ */
