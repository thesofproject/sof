// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2023 NXP
 */

#include <rtos/sof.h>
#include <rtos/clk.h>
#include <sof/lib/dma.h>
#include <sof/lib/dai.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/lib/mailbox.h>
#include <sof/drivers/mu.h>
#include <sof/fw-ready-metadata.h>
#include <sof_versions.h>
#include <kernel/abi.h>
#include <sof/debug/debug.h>

static const struct sof_ipc_fw_ready ready = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},
	.version = {
		.hdr.size = sizeof(struct sof_ipc_fw_version),
		.micro = SOF_MICRO,
		.minor = SOF_MINOR,
		.major = SOF_MAJOR,
		.build = -1,
		.date = "dtermin.\0",
		.time = "fwready.\0",
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
		.src_hash = SOF_SRC_HASH,
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

static const struct sof_ipc_window windows = {
	.ext_hdr = {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_window),
		.type = SOF_IPC_EXT_WINDOW,
	},
	.num_windows = 3,
	.window = {
		{
			.type = SOF_IPC_REGION_DOWNBOX,
			.flags = 0,
			.size = MAILBOX_HOSTBOX_SIZE,
			.offset = MAILBOX_HOSTBOX_OFFSET,
		},
		{
			.type = SOF_IPC_REGION_UPBOX,
			.id = 0,
			.flags = 0,
			.size = MAILBOX_DSPBOX_SIZE,
			.offset = MAILBOX_DSPBOX_OFFSET,
		},
		{
			.type = SOF_IPC_REGION_STREAM,
			.id = 0,
			.flags = 0,
			.size = MAILBOX_STREAM_SIZE,
			.offset = MAILBOX_STREAM_OFFSET,
		},
	},
};

int platform_boot_complete(uint32_t boot_message)
{
	struct sof_ipc_reply reply;

	/* prepare reply header */
	reply.error = 0;
	reply.hdr.cmd = SOF_IPC_FW_READY;
	reply.hdr.size = sizeof(reply);

	/* copy reply header to hostbox */
	mailbox_hostbox_write(0, &reply, sizeof(reply));

	/* copy firmware ready data to hostbox */
	mailbox_hostbox_write(sizeof(reply), &ready, sizeof(ready));

	/* write manifest data after firmware ready */
	mailbox_hostbox_write(sizeof(reply) + sizeof(ready),
			      &windows,
			      sizeof(windows));

	/* we can return, the IPC handler will take care of the doorbell */
	return 1;
}

int platform_context_save(struct sof *sof)
{
	/* TODO: nothing to do here, remove me if possible */
	return 0;
}

int platform_init(struct sof *sof)
{
	int ret;

	/* Initialize clock data */
	/* TODO: is this really required? */
	platform_clock_init(sof);

	/* Initialize EDF scheduler */
	scheduler_init_edf();

	/* Initialize Zephyr domain and timer-based scheduler */
	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	zephyr_ll_scheduler_init(sof->platform_timer_domain);

	/* Initialize DMAC data */
	ret = dmac_init(sof);
	if (ret < 0)
		return -ENODEV;

	/* Initialize IPC */
	ipc_init(sof);

	/* Initialize DAI */
	dai_init(sof);

	/* We're all set */
	return 0;
}
