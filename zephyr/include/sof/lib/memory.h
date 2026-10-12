/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

/**
 * @file sof/lib/memory.h
 * @brief Memory attributes, cold/hot execution paths, and platform memory inclusions.
 * @ingroup memory_api
 */

#ifndef __SOF_LIB_MEMORY_H__
#define __SOF_LIB_MEMORY_H__

#include <platform/lib/memory.h>
#include <stdbool.h>

/** \addtogroup memory_api
 *  @{
 */

/**
 * @brief Attribute marking non-latency-critical code for placement in cold memory sections.
 *
 * Cold code (e.g. initialization, IPC setup, or offload modules) may be placed in
 * DRAM / L3 memory or powered-down banks to conserve high-speed internal SRAM.
 */
#ifndef __cold
#define __cold
#endif

/**
 * @brief Attribute marking read-only constants for placement in cold memory sections.
 */
#ifndef __cold_rodata
#define __cold_rodata
#endif

#ifdef __ZEPHYR__
bool ll_sch_is_current(void);
#else
#define ll_sch_is_current() false
#endif

#if CONFIG_COLD_STORE_EXECUTE_DEBUG
#include <rtos/panic.h>

/**
 * @brief Start monitoring hot execution paths for cold storage violations.
 */
void dbg_path_hot_start_watching(void);

/**
 * @brief Stop monitoring hot execution paths.
 */
void dbg_path_hot_stop_watching(void);

/**
 * @brief Confirm that execution remained on expected hot paths.
 */
void dbg_path_hot_confirm(void);

/**
 * @brief Record entry into a cold execution function.
 * @param[in] fn Calling function name.
 */
void dbg_path_cold_enter(const char *fn);

/**
 * @brief Internal assertion validating that a function can safely execute on cold storage.
 * @param[in] fn Calling function name.
 */
static inline void __assert_can_be_cold(const char *fn)
{
	if (k_is_user_context())
		return;

	__ASSERT(!ll_sch_is_current(), "%s() called from an LL thread!", fn);
	dbg_path_cold_enter(fn);
}

/**
 * @brief Assert that the current function is not executing from a latency-critical LL thread.
 */
#define assert_can_be_cold() __assert_can_be_cold(__func__)
#else
#define dbg_path_hot_start_watching() do {} while (0)
#define dbg_path_hot_stop_watching() do {} while (0)
#define dbg_path_hot_confirm() do {} while (0)
#define dbg_path_cold_enter() do {} while (0)
/**
 * @brief Assert that the current function is not executing from a latency-critical LL thread.
 */
#define assert_can_be_cold() do {} while (0)
#endif

/** @}*/

#endif /* __SOF_LIB_MEMORY_H__ */
