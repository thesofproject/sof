/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

/*
 * Extended manifest is a place to store metadata about firmware, known during
 * compilation time - for example firmware version or used compiler.
 * Given information are read on host side before firmware startup.
 * This part of output binary is not signed.
 *
 * To add new content to ext_man, in firmware code define struct which starts
 * with ext_man_elem_head followed by usage dependent content and place whole
 * struct in "fw_metadata" section. Moreover kernel code should be modified to
 * properly read new packet.
 *
 * Extended manifest is designed to be extensible. In header there is a field
 * which describe header length, so after appending some data to header then it
 * can be easily skipped by device with older version of this header.
 * Unknown ext_man elements should be just skipped by host,
 * to be backward compatible. Field `ext_man_elem_header.elem_size` should be
 * used in such a situation.
 */

#ifndef __KERNEL_EXT_MANIFEST_H__
#define __KERNEL_EXT_MANIFEST_H__

#include <ipc/info.h>
#include <sof/compiler_attributes.h>
#include <rimage/sof/kernel/ext_manifest.h>
#include <stdint.h>

/* Now define extended manifest elements */

/* Extended manifest elements identificators */
enum ext_man_elem_type {
	EXT_MAN_ELEM_FW_VERSION		= 0,
	EXT_MAN_ELEM_WINDOW		= SOF_IPC_EXT_WINDOW,
	EXT_MAN_ELEM_CC_VERSION		= SOF_IPC_EXT_CC_INFO,
	EXT_MAN_ELEM_PROBE_INFO		= SOF_IPC_EXT_PROBE_INFO,
	EXT_MAN_ELEM_DBG_ABI		= SOF_IPC_EXT_USER_ABI_INFO, /**< ABI3.17 */
	EXT_MAN_ELEM_CONFIG_DATA	= 5,    /**< ABI3.17 */
	EXT_MAN_ELEM_PLATFORM_CONFIG_DATA = 6,  /**< ABI3.17 */
};

/* EXT_MAN_ELEM_CONFIG_DATA elements identificators */
enum config_elem_type {
	EXT_MAN_CONFIG_IPC_MSG_SIZE	= 1,
	EXT_MAN_CONFIG_MEMORY_USAGE_SCAN = 2, /**< ABI3.18 */
	EXT_MAN_CONFIG_LAST_ELEM,	/**< keep it at the end of enum list */
};

struct config_elem {
	uint32_t token;
	uint32_t value;
} __attribute__((packed, aligned(4)));

/* FW version */
struct ext_man_fw_version {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_fw_version version;
	uint32_t flags;
} __attribute__((packed, aligned(4)));

/* windows info */
struct ext_man_windows {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_window window;
} __attribute__((packed, aligned(4)));

/* Used C compiler description */
struct ext_man_cc_version {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_cc_version cc_version;
} __attribute__((packed, aligned(4)));

struct ext_man_probe_support {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_probe_support probe;
} __attribute__((packed, aligned(4)));

struct ext_man_dbg_abi {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_user_abi_version dbg_abi;
} __attribute__((packed, aligned(4)));

/** EXT_MAN_ELEM_CONFIG_DATA elements (ABI3.17) */
struct ext_man_config_data {
	struct ext_man_elem_header hdr;

	struct config_elem elems[];
} __attribute__((packed, aligned(4)));

#endif /* __KERNEL_EXT_MANIFEST_H__ */
