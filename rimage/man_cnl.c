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
#include <config.h>
#include <version.h>

/* manifest template */
struct fw_image_manifest_v1_8 cnl_manifest = {

	.cse_partition_dir_header = {
		.header_marker = CSE_HEADER_MAKER,
		.nb_entries = MAN_CSE_PARTS,
		.header_version = 1,
		.entry_version = 1,
		.header_length = sizeof(struct CsePartitionDirHeader),
		.partition_name = "ADSP",
	},

	.cse_partition_dir_entry = {
		{
			/* CssHeader + platformFirmwareAuthenticationExtension - padding */
			.entry_name = "ADSP.man",
			.offset = MAN_CSS_HDR_OFFSET_V1_8,
			.length = sizeof(struct css_header_v1_8) +
				PLAT_AUTH_SIZE,
		},
		{	/* ADSPMetadataFileExtension */
			.entry_name = "cavs0015.met",
			.offset = MAN_META_EXT_OFFSET_V1_8,
			.length = sizeof(struct sof_man_adsp_meta_file_ext),
		},
		{	/* AdspFwBinaryDesc */
			.entry_name = "cavs0015",
			.offset = MAN_FW_DESC_OFFSET_V1_8,
			.length = 0,	/* calculated by rimage - */
		},

	},

	.css = {
		.header_type	= MAN_CSS_MOD_TYPE,
		.header_len	= MAN_CSS_HDR_SIZE,
		.header_version	= MAN_CSS_HDR_VERSION,
		.module_vendor	= MAN_CSS_MOD_VENDOR,
		.size		= 222,
		.header_id	= MAN_CSS_HDR_ID,
		.padding	= 0,
		.version = {
			.major_version	= SOF_MAJOR,
			.minor_version	= SOF_MINOR,
			.hotfix_version = 0,
			.build_version	= SOF_BUILD,
		},
		.modulus_size	= MAN_CSS_MOD_SIZE,
		.exponent_size	= MAN_CSS_EXP_SIZE,
	},

	.signed_pkg = {
		.ext_type	= SIGN_PKG_EXT_TYPE,
		.ext_len	= sizeof(struct signed_pkg_info_ext),
		.name		= "ADSP",
		.vcn		= 0,
		.bitmap		= {0, 0, 0, 0, 8},

		.module[0]	= {
			.name		= "cavs0015.met",
			.meta_size	= 96,
			.type		= 0x03,
			.hash_algo	= 0x02, /* SHA 256 */
			.hash_size	= 0x20,
		},
	},

	.partition_info = {

		.ext_type	= PART_INFO_EXT_TYPE,
		.ext_len	= sizeof(struct partition_info_ext),

		.name		= "ADSP",
		.length		= 0,	/* calculated by rimage - rounded up to nearest PAGE */
		.part_version	= 0x10000000,
		.instance_id	= 1,
		.reserved[0 ... 19]	= 0xff,

		.module[0]	= {
			.name		= "cavs0015.met",
			.meta_size	= 96,
			.type		= 0x03,
			.reserved	= {0x00, 0xff, 0xff},
		},

	},

	.cse_padding[0 ... 47]	= 0xff,

	.adsp_file_ext = {
		.ext_type = 17,
		.ext_len	= sizeof(struct sof_man_adsp_meta_file_ext),
		.imr_type = 3,
		.comp_desc[0] = {
			.version = 0,
			.base_offset = MAN_DESC_OFFSET,
			.limit_offset = 0, /* calculated length + MAN_DESC_OFFSET */
		},

	},

	.reserved[0 ... 31]	= 0xff,

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
			.load_offset	= 0x30000,
		},
	},
};
