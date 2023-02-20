// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google Inc. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <rtos/sof.h>
#include <sof/lib/dai.h>

const struct dai_type_info dti[] = {
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	sof->dai_info = &lib_dai;

	return 0;
}

void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
}

struct ipc4_ipcgtw_cmd;

int ipcgtw_process_cmd(const struct ipc4_ipcgtw_cmd *cmd,
		       void *reply_payload, uint32_t *reply_payload_size);

int ipcgtw_process_cmd(const struct ipc4_ipcgtw_cmd *cmd,
		       void *reply_payload, uint32_t *reply_payload_size)
{
	return 0;
}
