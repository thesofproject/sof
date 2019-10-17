/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/lib/clk.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

int ipc_stream_send_xrun(struct comp_dev *cdev,
	struct sof_ipc_stream_posn *posn);

int arch_cpu_is_core_enabled(int id);

void cpu_power_down_core(void);

struct ipc_comp_dev *ipc_get_comp_by_id(struct ipc *ipc, uint32_t id);

void notifier_notify(void);

void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn);

void platform_host_timestamp(struct comp_dev *host,
	struct sof_ipc_stream_posn *posn);

uint64_t platform_timer_get(struct timer *timer);

struct pipeline_new_setup_data {
	struct sof_ipc_pipe_new ipc_data;
	struct comp_dev *comp_data;
};
