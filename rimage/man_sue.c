/*
 * Copyright (c) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "css.h"
#include "cse.h"
#include "plat_auth.h"
#include "manifest.h"
#include <version.h>

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
			.major_version	= SOF_MAJOR,
			.minor_version	= SOF_MINOR,
			.hotfix_version	= 0,
			.build_version	= SOF_BUILD,
			.load_offset	= 0x2000,
		},
	},
};
