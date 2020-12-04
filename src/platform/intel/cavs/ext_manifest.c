// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>
//

#include <cavs/ext_manifest.h>
#include <kernel/ext_manifest.h>
#include <sof/common.h>
#include <sof/lib/memory.h>

/* Describes elements counter from ext_man_cavs_config dictionary */
#define CAVS_CONFIG_ELEM_CNT (EXT_MAN_CAVS_CONFIG_LAST_ELEM - 1)

const struct ext_man_cavs_config_data ext_man_cavs_config
	__aligned(EXT_MAN_ALIGN) __section(".fw_metadata") = {
	.hdr.type = EXT_MAN_ELEM_PLATFORM_CONFIG_DATA,
	.hdr.elem_size = ALIGN_UP_COMPILE(sizeof(struct ext_man_cavs_config_data) +
					  sizeof(struct config_elem) * CAVS_CONFIG_ELEM_CNT,
					  EXT_MAN_ALIGN),
	.elems = {
		{EXT_MAN_CAVS_CONFIG_LPRO,  IS_ENABLED(CONFIG_CAVS_LPRO_ONLY)},
		{EXT_MAN_CAVS_CONFIG_OUTBOX_SIZE, SRAM_OUTBOX_SIZE},
		{EXT_MAN_CAVS_CONFIG_INBOX_SIZE, SRAM_INBOX_SIZE},
	},
};
