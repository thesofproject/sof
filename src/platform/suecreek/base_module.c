// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/lib/memory.h>
#include <rimage/sof/user/manifest.h>
#include <sof/common.h>

/*
 * Each module has an entry in the FW manifest header. This is NOT part of
 * the SOF executable image but is inserted by object copy as a ELF section
 * for parsing by rimage (to genrate the manifest).
 */
struct sof_man_module_manifest cnl_manifest = {
	.module = {
		.name	= "BASEFW",
		.uuid	= {0x32, 0x8c, 0x39, 0x0e, 0xde, 0x5a, 0x4b, 0xba,
				0x93, 0xb1, 0xc5, 0x04, 0x32, 0x28, 0x0e, 0xe4},
		.entry_point = SOF_TEXT_START,
		.type = {
				.load_type = SOF_MAN_MOD_TYPE_MODULE,
				.domain_ll = 1,
		},
		.affinity_mask = 3,
	},
};

/* not used, but stops linker complaining */
int _start;
