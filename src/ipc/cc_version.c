// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <ipc/info.h>
#include <sof/fw-ready-metadata.h>
#include <sof/common.h>
#include <sof/compiler_info.h>

/* copy CC_NAME to const arrays during compilation time */
#define CC_NAME_COPY(field) \
	field = CC_NAME, \
	field[ARRAY_SIZE(((struct sof_ipc_cc_version *)(0))->name) - 1] = 0

/* copy CC_OPTIM to const arrays during compilation time */
#define CC_OPTIM_COPY(field) \
	field = CC_OPTIMIZE_FLAGS, \
	field[ARRAY_SIZE(((struct sof_ipc_cc_version *)(0))->optim) - 1] = 0

/* copy CC_DESC to arrays during compilation time */
#define CC_DESC_COPY(field) \
	field = CC_DESC, \
	field[ARRAY_SIZE(((struct sof_ipc_cc_version *)(0))->desc) - 1] = 0

const struct sof_ipc_cc_version cc_version
	__section(".fw_ready_metadata") = {
	.ext_hdr = {
		.hdr.cmd = SOF_IPC_FW_READY,
		.hdr.size = sizeof(struct sof_ipc_cc_version),
		.type = SOF_IPC_EXT_CC_INFO,
	},
	.micro = CC_MICRO,
	.minor = CC_MINOR,
	.major = CC_MAJOR,
	CC_NAME_COPY(.name),
	CC_OPTIM_COPY(.optim),
	CC_DESC_COPY(.desc),
};
