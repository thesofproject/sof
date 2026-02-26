// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
#include <sof/lib/mailbox.h>
#include <sof/ipc/common.h>
#include <sof/ipc/driver.h>
#include <rtos/sof.h>

void ipc_platform_complete_cmd(struct ipc *ipc)
{
}

int platform_boot_complete(uint32_t boot_message)
{
	return 0;
}

int platform_init(struct sof *sof)
{
	ipc_init(sof);
	return 0;
}

int platform_ipc_init(struct ipc *ipc)
{
	return 0;
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	return 0;
}
