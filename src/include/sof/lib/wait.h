/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/*
 * Simple wait for event completion and signaling with timeouts.
 */

#ifndef __SOF_LIB_WAIT_H__
#define __SOF_LIB_WAIT_H__

#include <arch/lib/wait.h>
#include <sof/drivers/timer.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

static inline void wait_for_interrupt(int level)
{
	tracev_event(TRACE_CLASS_WAIT, "WFE");
#if CONFIG_DEBUG_LOCKS
	if (lock_dbg_atomic)
		trace_error_atomic(TRACE_CLASS_WAIT, "atm");
#endif
	platform_wait_for_interrupt(level);
	tracev_event(TRACE_CLASS_WAIT, "WFX");
}

/**
 * \brief Waits at least passed number of clocks.
 * \param[in] number_of_clks Minimum number of clocks to wait.
 */
void wait_delay(uint64_t number_of_clks);

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us);

#endif /* __SOF_LIB_WAIT_H__ */
