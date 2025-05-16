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

void dbg_path_hot_start_watching(void);
void dbg_path_hot_stop_watching(void);
void dbg_path_hot_confirm(void);
void dbg_path_cold_enter(const char *fn);

static inline void __assert_can_be_cold(const char *fn)
{
	__ASSERT(!ll_sch_is_current(), "%s() called from an LL thread!", fn);
	dbg_path_cold_enter(fn);
}
#define assert_can_be_cold() __assert_can_be_cold(__func__)
#else
#define dbg_path_hot_start_watching() do {} while (0)
#define dbg_path_hot_stop_watching() do {} while (0)
#define dbg_path_hot_confirm() do {} while (0)
#define dbg_path_cold_enter() do {} while (0)
#define assert_can_be_cold() do {} while (0)
#endif

#endif /* __SOF_LIB_MEMORY_H__ */
