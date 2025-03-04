/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_LIB_MEMORY_H__
#define __SOF_LIB_MEMORY_H__

#include <platform/lib/memory.h>

#ifndef __cold
#define __cold
#endif

#ifndef __cold_rodata
#define __cold_rodata
#endif

#if CONFIG_COLD_STORE_EXECUTE_DEBUG
#include <rtos/panic.h>

#ifdef __ZEPHYR__
bool ll_sch_is_current(void);
#else
#define ll_sch_is_current() false
#endif

static inline void assert_can_be_cold(void)
{
	assert(!ll_sch_is_current());
}
#else
#define assert_can_be_cold() do {} while (0)
#endif

#endif /* __SOF_LIB_MEMORY_H__ */
