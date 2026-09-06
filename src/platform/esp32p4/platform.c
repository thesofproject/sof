// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#include <rtos/sof.h>
#include <rtos/clk.h>
#include <platform/lib/clk.h>
#include <sof/lib/dma.h>
#include <sof/lib/dai.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/lib/mailbox.h>
#include <sof/fw-ready-metadata.h>
#include <sof_versions.h>
#include <kernel/abi.h>
#include <sof/debug/debug.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(esp32p4_platform, CONFIG_SOF_LOG_LEVEL);

uint8_t sof_esp32_mailbox[0x4000] __aligned(64);

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

	reply.error = 0;
	reply.hdr.cmd = SOF_IPC_FW_READY;
	reply.hdr.size = sizeof(reply);

	mailbox_hostbox_write(0, &reply, sizeof(reply));
	mailbox_hostbox_write(sizeof(reply), &ready, sizeof(ready));
	mailbox_hostbox_write(sizeof(reply) + sizeof(ready),
			      &windows,
			      sizeof(windows));

	LOG_INF("ESP32-P4 platform boot complete");
	return 0;
}

int platform_context_save(struct sof *sof)
{
	return 0;
}

#include <sof/ipc/common.h>
#include <sof/ipc/schedule.h>
#include <sof/ipc/msg.h>
#include <sof/lib/uuid.h>

SOF_DEFINE_REG_UUID(zipc_task);
extern struct task_ops ipc_task_ops;

int platform_ipc_init(struct ipc *ipc)
{
	ipc_set_drvdata(ipc, NULL);
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(zipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);
	return 0;
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr = mailbox_validate();

	if (hdr)
		ipc_cmd(hdr);
	return SOF_TASK_STATE_COMPLETED;
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	return 0;
}

int platform_init(struct sof *sof)
{
	LOG_INF("Initializing ESP32-P4 SOF Platform...");

	platform_clock_init(sof);

	/* Initialize EDF scheduler */
	scheduler_init_edf();

	/* Initialize Zephyr domain and timer-based LL scheduler */
	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	if (!sof->platform_timer_domain) {
		LOG_ERR("Failed to initialize platform timer domain");
		return -ENODEV;
	}

	zephyr_ll_scheduler_init(sof->platform_timer_domain);

	/* Initialize platform DMAC */
	dmac_init(sof);

	/* Initialize Zephyr native DAI subsystem */
	dai_init(sof);

	/* Initialize IPC */
	ipc_init(sof);

	LOG_INF("ESP32-P4 SOF Platform initialized successfully");
	return 0;
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	ARG_UNUSED(ipc);
}
