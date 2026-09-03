/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

/*
 * A set of functions that allows implement a watchdog protection for ll scheduler tasks. The
 * watchdog_enable function is called after creating a ll thread for specified core. Its task is to
 * start the watchdog for a given core. When all tasks are finished, function watchdog_disable will
 * be called before stopping the thread. It should stop the watchdog on the given core.
 * Periodically, after each tick has been handled, the watchdog counter is reseted by calling
 * watchdog_feed.
 */

#ifndef __SOF_LIB_WATCHDOG_H__
#define __SOF_LIB_WATCHDOG_H__

#include <sof/common.h>

#if IS_ENABLED(CONFIG_LL_WATCHDOG)
/**
 * \brief Enable a watchdog timer for specified core
 * \param[in] core Core id
 */
void watchdog_enable(int core);

/**
 * \brief Disable a watchdog timer for specified core
 * \param[in] core Core id
 */
void watchdog_disable(int core);

/**
 * \brief Feed a watchdog timer for specified core
 * \param[in] core Core id
 */
void watchdog_feed(int core);

/**
 * \brief ll watchdog infrastructure initialization
 */
void watchdog_init(void);

/**
 * \brief Watchdog timeout notification on secondary core
 *
 * This function is called by the idc handler after receiving a watchdog timeout notification for
 * secondary core. Executes on primary core.
 *
 * \param[in] core Core id
 */
void watchdog_secondary_core_timeout(int core);
#else	/* IS_ENABLED(CONFIG_LL_WATCHDOG) */
static inline void watchdog_enable(int core) {}
static inline void watchdog_disable(int core) {}
static inline void watchdog_feed(int core) {}
static inline void watchdog_init(void) {}
static inline void watchdog_secondary_core_timeout(int core) {}
#endif	/* IS_ENABLED(CONFIG_LL_WATCHDOG) */

#endif /* __SOF_LIB_WATCHDOG_H__ */
