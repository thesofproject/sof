/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ZEPHYR_RTOS_PANIC_H__
#define __ZEPHYR_RTOS_PANIC_H__

#include <rtos/kernel.h>

#ifndef __ZEPHYR__
#error "This file should only be included in Zephyr builds."
#endif /* __ZEPHYR__ */

#define sof_panic(x) k_panic()
#define assert(x) __ASSERT_NO_MSG(x)

/* To print the asserted expression on failure:
 * #define assert(x) __ASSERT(x, #x)
 */

#endif /* __ZEPHYR_RTOS_PANIC_H__ */
