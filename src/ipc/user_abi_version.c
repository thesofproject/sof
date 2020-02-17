// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <ipc/info.h>
#include <user/abi_dbg.h>

const struct sof_ipc_user_abi_version user_abi_version
	__attribute__((section(".fw_ready_metadata"))) = {
	.ext_hdr = {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_user_abi_version),
		.type = SOF_IPC_EXT_USER_ABI_INFO,
	},
	.abi_dbg_version = SOF_ABI_DBG_VERSION,
};
