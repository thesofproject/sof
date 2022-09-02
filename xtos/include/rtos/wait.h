/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/*
 * Simple wait for event completion and signaling with timeouts.
 */

#ifndef __XTOS_RTOS_WAIT_H__
#define __XTOS_RTOS_WAIT_H__

#include <stddef.h>
#include <stdint.h>

#if !CONFIG_LIBRARY
#include <arch/lib/wait.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

extern struct tr_ctx wait_tr;

static inline void wait_for_interrupt(int level)
{
	LOG_MODULE_DECLARE(wait, CONFIG_SOF_LOG_LEVEL);

	tr_dbg(&wait_tr, "WFE");
#if CONFIG_DEBUG_LOCKS
	if (lock_dbg_atomic)
		tr_err_atomic(&wait_tr, "atm");
#endif
	platform_wait_for_interrupt(level);
	tr_dbg(&wait_tr, "WFX");
}

/**
 * \brief Waits at least passed number of clocks.
 * \param[in] number_of_clks Minimum number of clocks to wait.
 */
void wait_delay(uint64_t number_of_clks);

/**
 * \brief Waits at least passed number of milliseconds.
 * \param[in] ms Minimum number of milliseconds to wait.
 */
void wait_delay_ms(uint64_t ms);

/**
 * \brief Waits at least passed number of microseconds.
 * \param[in] us Minimum number of microseconds to wait.
 */
void wait_delay_us(uint64_t us);
#else
static inline void wait_delay(uint64_t number_of_clks) {}
static inline void wait_delay_ms(uint64_t ms) {}
static inline void wait_delay_us(uint64_t us) {}
#endif

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us);

#endif /* __XTOS_RTOS_WAIT_H__ */
