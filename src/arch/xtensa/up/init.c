// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <platform/memory.h>
#include <sof/interrupt.h>
#include <platform/interrupt.h>
#include <sof/mailbox.h>
#include <arch/init.h>
#include <arch/task.h>
#include <sof/init.h>
#include <sof/lock.h>
#include <stdint.h>

/**
 * \file arch/xtensa/up/init.c
 * \brief Xtensa UP initialization functions
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#if DEBUG_LOCKS
/** \brief Debug lock. */
uint32_t lock_dbg_atomic

/** \brief Debug locks per user. */
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif

/**
 * \brief Initializes architecture.
 * \param[in,out] sof Firmware main context.
 * \return Error status.
 */
int arch_init(struct sof *sof)
{
	register_exceptions();
	arch_assign_tasks();
	return 0;
}

int slave_core_init(struct sof *sof) { return 0; }
