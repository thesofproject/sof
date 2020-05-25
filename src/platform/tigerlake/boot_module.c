// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/lib/memory.h>
#include <rimage/sof/user/manifest.h>

/*
 * Each module has an entry in the FW manifest header. This is NOT part of
 * the SOF executable image but is inserted by object copy as a ELF section
 * for parsing by rimage (to genrate the manifest).
 */
struct sof_man_module_manifest tgl_bootldr_manifest = {
	.module = {
		.name	= "BRNGUP",
		.uuid	= {0xf3, 0xe4, 0x79, 0x2b, 0x75, 0x46, 0x49, 0xf6,
				0x89, 0xdf, 0x3b, 0xc1, 0x94, 0xa9, 0x1a, 0xeb},
		.entry_point = IMR_BOOT_LDR_TEXT_ENTRY_BASE,
		.type = {
				.load_type = SOF_MAN_MOD_TYPE_MODULE,
				.domain_ll = 1,
		},
		.affinity_mask = 3,
	},
};

/* not used, but stops linker complaining */
int _start;
