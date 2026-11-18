// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
#include <sof/lib/mailbox.h>
#include <sof/ipc/common.h>
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
	return 0;
}
