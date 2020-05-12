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
#include <stdint.h>
#include <rimage/src/include/sof/kernel/ext_manifest.h>

/* FW version */
struct ext_man_fw_version {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_fw_version version;
	uint32_t flags;
} __packed;

/* Used C compiler description */
struct ext_man_cc_version {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_cc_version cc_version;
} __packed;

struct ext_man_probe_support {
	struct ext_man_elem_header hdr;
	/* use sof_ipc struct because of code re-use */
	struct sof_ipc_probe_support probe;
} __packed;

/* UUID dictionary */
struct ext_man_uuid_dict_elem {
	uint32_t addr;
	uint8_t uuid[16];
} __packed;

struct ext_man_uuid_dict {
	struct ext_man_elem_header hdr;
	/* filled in rimage by uuid_section content */
	struct ext_man_uuid_dict_elem entries[];
} __packed;

#endif /* __KERNEL_EXT_MANIFEST_H__ */
