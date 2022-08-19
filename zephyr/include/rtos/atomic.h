// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

#ifndef __ZEPHYR_RTOS_ATOMIC_H__
#define __ZEPHYR_RTOS_ATOMIC_H__

#include <zephyr/sys/atomic.h>

/* Zephyr commit 174cb7f9f183 switched 'atomic_t' from 'int' to
 * 'long'. As we don't support 64 bits, this should have made no
 * difference but in reality it broke printk("%d / %ld",...)  The no-op
 * cast to 'long' below provides a transition by making it possible to
 * compile SOF both before _and_ after the Zephyr switch.
 */
#define atomic_read(p)		((long)atomic_get(p))
#define atomic_init(p, v)	atomic_set(p, v)

#endif /*__ZEPHYR_RTOS_ATOMIC_H__ */
