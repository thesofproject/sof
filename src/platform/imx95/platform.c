// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2024 NXP
 */

#include <rtos/sof.h>
#include <rtos/clk.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/lib/dma.h>
#include <sof/lib/dai.h>
#include <sof/debug/debug.h>
#include <sof_versions.h>
#include <kernel/abi.h>
#include <kernel/ext_manifest.h>
#include <sof/drivers/mu.h>

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

const struct ext_man_windows windows
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") __unused = {
	.hdr = {
		.type = EXT_MAN_ELEM_WINDOW,
		.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_windows), EXT_MAN_ALIGN),
	},
	.window = {
		.ext_hdr = {
			.hdr.cmd = SOF_IPC_FW_READY,
			.hdr.size = sizeof(struct sof_ipc_window),
			.type = SOF_IPC_EXT_WINDOW,
		},
		.num_windows = 3,
		.window = {
			{
				.type = SOF_IPC_REGION_DOWNBOX,
				.size = MAILBOX_HOSTBOX_SIZE,
				.offset = MAILBOX_HOSTBOX_OFFSET,
			},
			{
				.type = SOF_IPC_REGION_UPBOX,
				.size = MAILBOX_DSPBOX_SIZE,
				.offset = MAILBOX_DSPBOX_OFFSET,
			},
			{
				.type = SOF_IPC_REGION_STREAM,
				.size = MAILBOX_STREAM_SIZE,
				.offset = MAILBOX_STREAM_OFFSET,
			},
		},
	},
};

int platform_boot_complete(uint32_t boot_message)
{
	mailbox_dspbox_write(0, &ready, sizeof(ready));

	imx_mu_xcr_rmw(IMX_MU_VERSION, IMX_MU_GCR,
		       IMX_MU_xCR_GIRn(IMX_MU_VERSION, 1), 0);

	return 0;
}

int platform_context_save(struct sof *sof)
{
	/* nothing to be done here */
	return 0;
}

int platform_init(struct sof *sof)
{
	int ret;

	platform_clock_init(sof);

	scheduler_init_edf();

	sof->platform_timer_domain = zephyr_domain_init(PLATFORM_DEFAULT_CLOCK);
	zephyr_ll_scheduler_init(sof->platform_timer_domain);

	ret = dmac_init(sof);
	if (ret < 0)
		return ret;

	ipc_init(sof);

	dai_init(sof);

	return 0;
}
