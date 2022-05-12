// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@intel.com>

#include <sof/drivers/timer.h>
#include <sof/lib/input-device.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <ipc/topology.h>
#include <user/trace.h>

#include <errno.h>
#include <stddef.h>
#include <limits.h>
#include <stdint.h>

LOG_MODULE_REGISTER(inputdev, CONFIG_SOF_LOG_LEVEL);

/* 2c979884-1546-470b-b5ab-3f5026ddc854 */
DECLARE_SOF_UUID("inputdev", inputdev_uuid, 0x2c979884, 0x1546, 0x470b,
		 0xb5, 0xab, 0x3f, 0x50, 0x26, 0xdd, 0xc8, 0x54);

DECLARE_TR_CTX(id_tr, SOF_UUID(inputdev_uuid), LOG_LEVEL_INFO);

/* 5708d56b-83be-448e-bbd9-d50f5bbfa9c1 */
DECLARE_SOF_UUID("inputdev-work", inputdev_task_uuid, 0x5708d56b, 0x83be,
		 0x448e, 0xbb, 0xd9, 0xd5, 0x0f, 0x5b, 0xbf, 0xa9, 0xc1);

static enum task_state input_device_task(void *data)
{
	struct input_device *inputdev = data;
	uint64_t stamp = k_cycle_get_64();
	static int value = 0;

	tr_info(&id_tr, "input_device_task() %u ms since previous event (%u)",
		(unsigned int) k_cyc_to_ms_near64(stamp -
						  inputdev->prev_stamp),
		(unsigned int) (stamp - inputdev->prev_stamp));

	value = !value;
	inputdev->event.code = BTN_1;
	inputdev->event.value = value;
	ipc_msg_send(inputdev->msg, &inputdev->event, false);

	inputdev->prev_stamp = stamp;

	return SOF_TASK_STATE_RESCHEDULE;
}

void input_device_init(struct sof *sof)
{
	tr_info(&id_tr, "input_device_init()");

	sof->input_device = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0,
				    SOF_MEM_CAPS_RAM,
				    sizeof(*sof->input_device));
	if (!sof->input_device) {
		tr_err(&id_tr, "input_device_init(), rzalloc failed");
		return;
	}

	ipc_build_input_event(&sof->input_device->event);
	sof->input_device->msg =
		ipc_msg_init(sof->input_device->event.rhdr.hdr.cmd,
			     sof->input_device->event.rhdr.hdr.size);

	if (!sof->input_device->msg) {
		tr_err(&id_tr, "input_device_init(), ipc_msg_init failed");
		goto err_exit;
	}

	if (schedule_task_init_ll(&sof->input_device->work,
				  SOF_UUID(inputdev_task_uuid),
				  SOF_SCHEDULE_LL_TIMER,
				  SOF_TASK_PRI_LOW, input_device_task,
				  sof->input_device, 0, 0)) {
		tr_err(&id_tr,
		       "input_device_init(), schedule_task_init_ll failed");
		goto err_exit;
	}

	if (schedule_task(&sof->input_device->work, 0, 3000000)) {
		schedule_task_free(&sof->input_device->work);
		tr_err(&id_tr,
		       "input_device_init(), schedule_task failed");
		goto err_exit;
	}

	/* initialize prev_stamp */
	sof->input_device->prev_stamp = k_cycle_get_64();
	return;

err_exit:
	ipc_msg_free(sof->input_device->msg);
	rfree(sof->input_device);
}

void input_device_exit(struct sof *sof)
{
	schedule_task_cancel(&sof->input_device->work);

	schedule_task_free(&sof->input_device->work);

	ipc_msg_free(sof->input_device->msg);
}
