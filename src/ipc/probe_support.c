// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <ipc/info.h>
#include <sof/fw-ready-metadata.h>
#include <sof/compiler_attributes.h>
#include <config.h>

const struct sof_ipc_probe_support probe_support
	__section(".fw_ready_metadata") = {
	.ext_hdr = {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_probe_support),
		.type = SOF_IPC_EXT_PROBE_INFO,
	},
#if CONFIG_PROBE
	.probe_points_max = CONFIG_PROBE_POINTS_MAX,
	.injection_dmas_max = CONFIG_PROBE_DMA_MAX
#endif
};
