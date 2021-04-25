// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>

#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/drivers/spi.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/header.h>
#include <stddef.h>
#include <stdint.h>

/* 7552b3a1-98dd-4419-ad6f-fbf21ebfceec */
DECLARE_SOF_UUID("ipc-task", ipc_task_uuid, 0x7552b3a1, 0x98dd, 0x4419,
		 0xad, 0x6f, 0xfb, 0xf2, 0x1e, 0xbf, 0xce, 0xec);

int ipc_platform_compact_write_msg(ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

int ipc_platform_compact_read_msg(ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

/* No private data for IPC */
enum task_state ipc_platform_do_cmd(void *data)
{
	struct ipc *ipc = data;
	ipc_cmd_hdr *hdr;
	struct sof_ipc_reply reply;

	/* perform command */
	hdr = mailbox_validate();
	ipc_cmd(hdr);

	mailbox_hostbox_read(&reply, SOF_IPC_MSG_MAX_SIZE,
			     0, sizeof(reply));
	spi_push(spi_get(SOF_SPI_INTEL_SLAVE), &reply, sizeof(reply));

	// TODO: signal audio work to enter D3 in normal context
	/* are we about to enter D3 ? */
	if (ipc->pm_prepare_D3) {

		while (1)
			wait_for_interrupt(0);
	}

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(void *data)
{
}

int ipc_platform_send_msg(struct ipc_msg *msg)
{
	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	tr_dbg(&ipc_tr, "ipc: msg tx -> 0x%x", msg->header);

	/* now interrupt host to tell it we have message sent */

	return 0;
}

int platform_ipc_init(struct ipc *ipc)
{
	ipc_set_drvdata(ipc, NULL);

	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	return 0;
}
