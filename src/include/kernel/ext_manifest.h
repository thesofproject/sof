/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

/*
 * Parts of this file are copied to ext_manifest.h in Linux kernel tree
 * and to the rimage tree. This duplication is OK because it is part of
 * a versioned ABI.
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
#include <stdint.h>

/* Top-level headers shared with rimage */

#ifndef __packed
#define __packed __attribute__((packed))
#endif

/* In ASCII `XMan` */
#define EXT_MAN_MAGIC_NUMBER	0x6e614d58

/* Build u32 number in format MMmmmppp */
#define EXT_MAN_BUILD_VERSION(MAJOR, MINOR, PATH) ( \
	((uint32_t)(MAJOR) << 24) | \
	((uint32_t)(MINOR) << 12) | \
	(uint32_t)(PATH))

/* check extended manifest version consistency */
#define EXT_MAN_VERSION_INCOMPATIBLE(host_ver, cli_ver) ( \
	((host_ver) & GENMASK(31, 24)) != \
	((cli_ver) & GENMASK(31, 24)))

/* used extended manifest header version */
#define EXT_MAN_VERSION		EXT_MAN_BUILD_VERSION(1, 0, 0)

/* struct size alignment for ext_man elements */
#define EXT_MAN_ALIGN 16

/* extended manifest header, deleting any field breaks backward compatibility */
struct ext_man_header {
	uint32_t magic;		/**< identification number, */
				/**< EXT_MAN_MAGIC_NUMBER */
	uint32_t full_size;	/**< [bytes] full size of ext_man, */
				/**< (header + content + padding) */
	uint32_t header_size;	/**< [bytes] makes header extensionable, */
				/**< after append new field to ext_man header */
				/**< then backward compatible won't be lost */
	uint32_t header_version; /**< value of EXT_MAN_VERSION */
				/**< not related with following content */

	/* just after this header should be list of ext_man_elem_* elements */
} __packed;

/* Now define extended manifest elements */

/* extended manifest element header */
struct ext_man_elem_header {
	uint32_t type;		/**< EXT_MAN_ELEM_* */
	uint32_t elem_size;	/**< in bytes, including header size */

	/* just after this header should be type dependent content */
} __packed;

/* End of the top-level headers shared with rimage */

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
