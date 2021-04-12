/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_SCHEDULE_H__
#define __SOF_IPC_SCHEDULE_H__

#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <stdbool.h>
#include <stdint.h>

/** \brief Scheduling period for IPC task in microseconds. */
#define IPC_PERIOD_USEC	100

struct ipc;

/**
 * \brief Get IPC command processing deadline.
 * @param data Task structure.
 */
static inline uint64_t ipc_task_deadline(void *data)
{
	/* TODO: Currently it's a workaround to execute IPC tasks ASAP.
	 * In the future IPCs should have a cycle budget and deadline
	 * should be calculated based on that value. This means every
	 * IPC should have its own maximum number of cycles that is required
	 * to finish processing. This will allow us to calculate task deadline.
	 */
	return SOF_TASK_DEADLINE_NOW;
}

/**
 * \brief Schedule processing of the next IPC command from host.
 * @param ipc The global IPC context.
 */
void ipc_schedule_process(struct ipc *ipc);

#endif
