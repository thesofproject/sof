/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_CACHE_H__
#define __ZEPHYR_RTOS_CACHE_H__

/* TODO: align with Zephyr generic cache API when ready */
#define __SOF_LIB_CACHE_H__
#include <arch/lib/cache.h>

/* writeback and invalidate data */
#define CACHE_WRITEBACK_INV	0

/* invalidate data */
#define CACHE_INVALIDATE	1

#endif /* __ZEPHYR_RTOS_CACHE_H__ */
