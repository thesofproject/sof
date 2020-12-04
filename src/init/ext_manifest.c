// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
//

#include <sof/bit.h>
#include <sof/common.h>
#include <sof/compiler_info.h>
#include <sof/debug/debug.h>
#include <kernel/abi.h>
#include <kernel/ext_manifest.h>
#include <user/abi_dbg.h>
#include <version.h>

const struct ext_man_fw_version ext_man_fw_ver
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") = {
	.hdr.type = EXT_MAN_ELEM_FW_VERSION,
	.hdr.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_fw_version),
					  EXT_MAN_ALIGN),
	.version = {
		.hdr.size = sizeof(struct sof_ipc_fw_version),
		.micro = SOF_MICRO,
		.minor = SOF_MINOR,
		.major = SOF_MAJOR,
#if CONFIG_DEBUG
		/* only added in debug for reproducibility in releases */
		.build = SOF_BUILD,
		.date = __DATE__,
		.time = __TIME__,
#endif
		.tag = SOF_TAG,
		.abi_version = SOF_ABI_VERSION,
		.src_hash = SOF_SRC_HASH,
	},
	.flags = DEBUG_SET_FW_READY_FLAGS,
};

const struct ext_man_cc_version ext_man_cc_ver
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") = {
	.hdr.type = EXT_MAN_ELEM_CC_VERSION,
	.hdr.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_cc_version),
					  EXT_MAN_ALIGN),
	.cc_version = {
		.ext_hdr.hdr.size = sizeof(struct sof_ipc_cc_version),
		.ext_hdr.hdr.cmd = SOF_IPC_FW_READY,
		.ext_hdr.type = SOF_IPC_EXT_CC_INFO,
		.micro = CC_MICRO,
		.minor = CC_MINOR,
		.major = CC_MAJOR,
		.name = CC_NAME "\0",	///< eg. "XCC", "\0" is needed when
					///< sizeof(CC_NAME)-1 == sizeof(.name)
		.optim = CC_OPTIMIZE_FLAGS "\0", ///< eg. "O2"
		.desc = CC_DESC "\0", ///< eg. " RG-2017.8-linux"
	},
};

const struct ext_man_probe_support ext_man_probe
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") = {
	.hdr.type = EXT_MAN_ELEM_PROBE_INFO,
	.hdr.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_probe_support),
					  EXT_MAN_ALIGN),
	.probe = {
		.ext_hdr.hdr.size = sizeof(struct sof_ipc_probe_support),
		.ext_hdr.hdr.cmd = SOF_IPC_FW_READY,
		.ext_hdr.type = SOF_IPC_EXT_PROBE_INFO,
#if CONFIG_PROBE
		.probe_points_max = CONFIG_PROBE_POINTS_MAX,
		.injection_dmas_max = CONFIG_PROBE_DMA_MAX
#endif
	},
};

const struct ext_man_dbg_abi ext_man_dbg_info
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") = {
	.hdr.type = EXT_MAN_ELEM_DBG_ABI,
	.hdr.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_dbg_abi),
					  EXT_MAN_ALIGN),
	.dbg_abi = {
		.ext_hdr.hdr.size = sizeof(struct sof_ipc_user_abi_version),
		.ext_hdr.hdr.cmd = SOF_IPC_FW_READY,
		.ext_hdr.type = SOF_IPC_EXT_USER_ABI_INFO,
		.abi_dbg_version = SOF_ABI_DBG_VERSION,
	},
};

/* increment this value after adding any element to ext_man_config dictionary */
#define CONFIG_ELEM_CNT (EXT_MAN_CONFIG_LAST_ELEM - 1)

const struct ext_man_config_data ext_man_config
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") = {
	.hdr.type = EXT_MAN_ELEM_CONFIG_DATA,
	.hdr.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_config_data) +
					  sizeof(struct config_elem) * CONFIG_ELEM_CNT,
					  EXT_MAN_ALIGN),
	.elems = {
		{EXT_MAN_CONFIG_IPC_MSG_SIZE, SOF_IPC_MSG_MAX_SIZE},
		{EXT_MAN_CONFIG_MEMORY_USAGE_SCAN, IS_ENABLED(CONFIG_DEBUG_MEMORY_USAGE_SCAN)},
	},
};
