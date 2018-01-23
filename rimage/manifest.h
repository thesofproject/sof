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

#ifndef __MANIFEST_H__
#define __MANIFEST_H__

#include <stdint.h>
#include <sof/manifest.h>
#include "css.h"
#include "cse.h"
#include "plat_auth.h"

#define MAN_PAGE_SIZE		4096

/* start offset for base FW module */
#define FILE_TEXT_OFFSET		0x8000

/*
 * CSE values for CNL
 */
#define MAN_CSE_PARTS			3


#define MAN_CSE_HDR_OFFSET		0
#define MAN_CSE_PADDING_SIZE		0x30
#define MAN_EXT_PADDING			0x20
#define MAN_DESC_OFFSET			0x2000

#define MAN_CSS_HDR_OFFSET \
	(MAN_CSE_HDR_OFFSET + \
	sizeof(struct CsePartitionDirHeader) + \
	MAN_CSE_PARTS * sizeof(struct CsePartitionDirEntry))

#define MAN_SIG_PKG_OFFSET \
	(MAN_CSS_HDR_OFFSET + \
	sizeof(struct css_header))

#define MAN_PART_INFO_OFFSET \
	(MAN_SIG_PKG_OFFSET + \
	sizeof(struct signed_pkg_info_ext))

#define MAN_META_EXT_OFFSET \
	(MAN_SIG_PKG_OFFSET + \
	sizeof(struct signed_pkg_info_ext) + \
	sizeof(struct partition_info_ext) + \
	MAN_CSE_PADDING_SIZE)

#define MAN_FW_DESC_OFFSET \
	(MAN_META_EXT_OFFSET + \
	sizeof(struct sof_man_adsp_meta_file_ext) + \
	MAN_EXT_PADDING)

#define MAN_DESC_PADDING_SIZE	\
	(MAN_DESC_OFFSET - MAN_FW_DESC_OFFSET)

/*
 * Firmware manifest header.
 */
struct fw_image_manifest {
	/* MEU tool needs these sections to be 0s */
	struct CsePartitionDirHeader cse_partition_dir_header;
	struct CsePartitionDirEntry cse_partition_dir_entry[MAN_CSE_PARTS];
	struct css_header css;
	struct signed_pkg_info_ext signed_pkg;
	struct partition_info_ext partition_info;
	uint8_t cse_padding[MAN_CSE_PADDING_SIZE];
	struct sof_man_adsp_meta_file_ext adsp_file_ext;

	/* reserved / pading at end of ext data - all 0s*/
	uint8_t reserved[MAN_EXT_PADDING];

	/* start of the unsigned binary for MEU input must start at MAN_DESC_OFFSET */
	uint8_t padding[MAN_DESC_PADDING_SIZE];

	struct sof_man_fw_desc desc;	/* at offset MAN_DESC_OFFSET */
} __attribute__((packed));

extern struct fw_image_manifest apl_manifest;
extern struct fw_image_manifest cnl_manifest;
#endif
