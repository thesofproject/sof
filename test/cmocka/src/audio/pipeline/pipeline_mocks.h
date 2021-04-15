/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/schedule.h>
#include <sof/lib/clk.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

struct pipeline_new_setup_data {
	uint32_t pipe_id;
	uint32_t priority;
	uint32_t comp_id;
	struct comp_dev *comp_data;
};
