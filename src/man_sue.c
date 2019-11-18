// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.

#include "css.h"
#include "cse.h"
#include "plat_auth.h"
#include "manifest.h"

/* manifest template */
struct fw_image_manifest_v1_5_sue sue_manifest = {
	.desc = {
		.header = {
			.header_id	= SOF_MAN_FW_HDR_ID,
			.header_len	= sizeof(struct sof_man_fw_header),
			.name		= SOF_MAN_FW_HDR_NAME,
			.preload_page_count	= 0,	/* size in pages from $CPD */
			.fw_image_flags	= SOF_MAN_FW_HDR_FLAGS,
			.feature_mask	= SOF_MAN_FW_HDR_FEATURES,
			.major_version	= 0,
			.minor_version	= 0,
			.hotfix_version	= 0,
			.build_version	= 0,
			.load_offset	= 0x2000,
		},
	},
};
