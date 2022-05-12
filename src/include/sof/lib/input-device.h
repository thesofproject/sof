/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@intel.com>
 */

#ifndef __SOF_LIB_INPUT_DEVICE_H__
#define __SOF_LIB_INPUT_DEVICE_H__

#include <sof/lib/memory.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <ipc/input-event.h>

#include <stdbool.h>
#include <stdint.h>

struct sof;

/* input-device data */
struct input_device {
	struct task work;
	uint64_t prev_stamp;		/* time of previous event */
	struct sof_ipc_input_event event;
	struct ipc_msg *msg;
};

#if CONFIG_INPUT_DEVICE

void input_device_init(struct sof *sof);
void input_device_exit(struct sof *sof);

#else

static inline void input_device_init(struct sof *sof) { }
static inline void input_device_exit(struct sof *sof) { }

#endif

#endif /* __SOF_LIB_INPUT_DEVICE_H__ */
