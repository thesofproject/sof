/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 */

#ifndef __MANIFEST_H__
#define __MANIFEST_H__

#include <stdint.h>
#include <rimage/sof/user/manifest.h>
#include <rimage/css.h>
#include <rimage/cse.h>
#include <rimage/plat_auth.h>

#define MAN_PAGE_SIZE		4096

/* start offset for modules built using xcc */
#define DEFAULT_XCC_MOD_OFFSET		0x8

/* start offset for base FW module */
#define FILE_TEXT_OFFSET_V1_8		0x8000
#define FILE_TEXT_OFFSET_V1_5		0x2000
#define FILE_TEXT_OFFSET_V1_5_SUE	0xA000

/*
 * CSE values for CNL
 */
#define MAN_CSE_PARTS			3


#define MAN_CSE_HDR_OFFSET		0
#define MAN_CSE_PADDING_SIZE		0x30
#define MAN_EXT_PADDING			0x20
#define MAN_DESC_OFFSET_V1_8			0x2000
#define MAN_DESC_OFFSET_V1_5			0x284
#define MAN_DESC_OFFSET_V1_5_SUE		0x2000
#define MAN_DEFAULT_IMR_TYPE		3

#define MAN_CSS_HDR_OFFSET \
	(MAN_CSE_HDR_OFFSET + \
	sizeof(struct CsePartitionDirHeader) + \
	MAN_CSE_PARTS * sizeof(struct CsePartitionDirEntry))

#define MAN_CSS_HDR_OFFSET_2_5 \
	(MAN_CSE_HDR_OFFSET + \
	sizeof(struct CsePartitionDirHeader_v2_5) + \
	MAN_CSE_PARTS * sizeof(struct CsePartitionDirEntry))

#define MAN_FW_DESC_OFFSET_V2_5 \
	(MAN_META_EXT_OFFSET_V2_5 + \
	sizeof(struct sof_man_adsp_meta_file_ext_v2_5) + \
	MAN_EXT_PADDING)

#define MAN_DESC_PADDING_SIZE_V2_5	\
	(MAN_DESC_OFFSET_V1_8 - MAN_FW_DESC_OFFSET_V2_5)

#define MAN_SIG_PKG_OFFSET_V1_8 \
	(MAN_CSS_HDR_OFFSET + \
	sizeof(struct css_header_v1_8))

#define MAN_SIG_PKG_OFFSET_V2_5 \
	(MAN_CSS_HDR_OFFSET_2_5 + \
	sizeof(struct css_header_v2_5))

#define MAN_PART_INFO_OFFSET_V1_8 \
	(MAN_SIG_PKG_OFFSET_V1_8 + \
	sizeof(struct signed_pkg_info_ext))

#define MAN_PART_INFO_OFFSET_V2_5 \
	(MAN_SIG_PKG_OFFSET_V2_5 + \
	sizeof(struct signed_pkg_info_ext_v2_5))

#define MAN_META_EXT_OFFSET_V1_8 \
	(MAN_SIG_PKG_OFFSET_V1_8 + \
	sizeof(struct signed_pkg_info_ext) + \
	sizeof(struct partition_info_ext) + \
	MAN_CSE_PADDING_SIZE)

#define MAN_META_EXT_OFFSET_V2_5 \
	(MAN_SIG_PKG_OFFSET_V2_5 + \
	sizeof(struct signed_pkg_info_ext_v2_5) + \
	sizeof(struct info_ext_0x16) + \
	0)

#define MAN_FW_DESC_OFFSET_V1_8 \
	(MAN_META_EXT_OFFSET_V1_8 + \
	sizeof(struct sof_man_adsp_meta_file_ext_v1_8) + \
	MAN_EXT_PADDING)

#define MAN_DESC_PADDING_SIZE_V1_8	\
	(MAN_DESC_OFFSET_V1_8 - MAN_FW_DESC_OFFSET_V1_8)

#define MAN_SIG_PKG_OFFSET_V1_5 \
	(MAN_CSS_HDR_OFFSET + \
	sizeof(struct css_header_v1_5))

#define MAN_META_EXT_OFFSET_V1_5 \
	(MAN_SIG_PKG_OFFSET_V1_5 + \
	sizeof(struct signed_pkg_info_ext) + \
	sizeof(struct partition_info_ext) + \
	MAN_CSE_PADDING_SIZE)

#define MAN_FW_DESC_OFFSET_V1_5 \
	(MAN_META_EXT_OFFSET_V1_5 + \
	sizeof(struct sof_man_adsp_meta_file_ext_v1_8) + \
	MAN_EXT_PADDING)

/*
 * Firmware manifest header V2.5 used on TGL onwards
 */
struct fw_image_manifest_v2_5 {
	/* MEU tool needs these sections to be 0s */
	struct CsePartitionDirHeader_v2_5 cse_partition_dir_header;
	struct CsePartitionDirEntry cse_partition_dir_entry[MAN_CSE_PARTS];
	struct css_header_v2_5 css;
	struct signed_pkg_info_ext_v2_5 signed_pkg;
	struct info_ext_0x16 info_0x16;

	struct sof_man_adsp_meta_file_ext_v2_5 adsp_file_ext;

	/* reserved / pading at end of ext data - all 0s*/
	uint8_t reserved[MAN_EXT_PADDING];

	/* start of the unsigned binary for MEU input must start at MAN_DESC_OFFSET */
	uint8_t padding[MAN_DESC_PADDING_SIZE_V2_5];

	struct sof_man_fw_desc desc;	/* at offset MAN_DESC_OFFSET */
} __attribute__((packed));

extern struct fw_image_manifest_v2_5 tgl_manifest;

/*
 * Firmware manifest header V1.8 used on APL onwards
 */
struct fw_image_manifest_v1_8 {
	/* MEU tool needs these sections to be 0s */
	struct CsePartitionDirHeader cse_partition_dir_header;
	struct CsePartitionDirEntry cse_partition_dir_entry[MAN_CSE_PARTS];
	struct css_header_v1_8 css;
	struct signed_pkg_info_ext signed_pkg;
	struct partition_info_ext partition_info;
	uint8_t cse_padding[MAN_CSE_PADDING_SIZE];
	struct sof_man_adsp_meta_file_ext_v1_8 adsp_file_ext;

	/* reserved / pading at end of ext data - all 0s*/
	uint8_t reserved[MAN_EXT_PADDING];

	/* start of the unsigned binary for MEU input must start at MAN_DESC_OFFSET */
	uint8_t padding[MAN_DESC_PADDING_SIZE_V1_8];

	struct sof_man_fw_desc desc;	/* at offset MAN_DESC_OFFSET */
} __attribute__((packed));

extern struct fw_image_manifest_v1_8 apl_manifest;
extern struct fw_image_manifest_v1_8 cnl_manifest;

/*
 * Firmware manifest header V1.5 used on SKL and KBL
 */
struct fw_image_manifest_v1_5 {
	struct css_header_v1_5 css_header;
	struct sof_man_fw_desc desc;
} __attribute__((packed));

struct fw_image_manifest_v1_5_sue {
    struct sof_man_fw_desc desc;
} __attribute__((packed));

/*
 * TODO: verify if we need platform specific manifest, or just
 * define manifest_v1_5 and manifest_v1_8.
 */
extern struct fw_image_manifest_v1_5 skl_manifest;
extern struct fw_image_manifest_v1_5 kbl_manifest;
extern struct fw_image_manifest_v1_5_sue sue_manifest;

struct image;
int simple_write_firmware(struct image *image);
int man_write_fw_v1_5(struct image *image);
int man_write_fw_v1_5_sue(struct image *image);
int man_write_fw_v1_8(struct image *image);
int man_write_fw_v2_5(struct image *image);
int man_write_fw_meu_v1_5(struct image *image);
int man_write_fw_meu_v1_8(struct image *image);
int man_write_fw_meu_v2_5(struct image *image);

#endif
