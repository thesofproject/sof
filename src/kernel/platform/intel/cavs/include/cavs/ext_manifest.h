/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
 */

#ifndef __CAVS_EXT_MANIFEST_H__
#define __CAVS_EXT_MANIFEST_H__

#include <kernel/ext_manifest.h>

/* EXT_MAN_ELEM_PLATFORM_CONFIG_DATA elements identificators */
enum cavs_config_elem_type {
	EXT_MAN_CAVS_CONFIG_LPRO	= 1,
	EXT_MAN_CAVS_CONFIG_OUTBOX_SIZE,
	EXT_MAN_CAVS_CONFIG_INBOX_SIZE,
	EXT_MAN_CAVS_CONFIG_LAST_ELEM,	/**< keep it at the end of enum list */
};

/* EXT_MAN_ELEM_PLATFORM_CONFIG_DATA elements */
struct ext_man_cavs_config_data {
	struct ext_man_elem_header hdr;

	struct config_elem elems[];
} __packed;

#endif /* __KERNEL_EXT_MANIFEST_H__ */
