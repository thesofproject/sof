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

#ifndef __PLAT_AUTH_H__
#define __PLAT_AUTH_H__

#include <stdint.h>

struct image;

#define PLAT_AUTH_SHA256_LEN		32
#define PLAT_AUTH_NAME_LEN		12
#define PLAT_AUTH_PADDING		48	/* pad at end of struct */

#define SIGN_PKG_EXT_TYPE		15
#define SIGN_PKG_NUM_MODULE		1

struct signed_pkg_info_module {
	uint8_t name[PLAT_AUTH_NAME_LEN]; /* must be padded with 0 */
	uint8_t type;
	uint8_t hash_algo;
	uint16_t hash_size;
	uint32_t meta_size;
	uint8_t hash[PLAT_AUTH_SHA256_LEN];
} __attribute__((packed));

struct signed_pkg_info_ext {
	uint32_t ext_type;
	uint32_t ext_len;

	uint8_t name[4];
	uint32_t vcn;
	uint8_t bitmap[16];
	uint32_t svn;
	uint8_t fw_type;
	uint8_t fw_sub_type;
	uint8_t reserved[14];	/* must be 0 */

	/* variable length of modules */
	struct signed_pkg_info_module module[SIGN_PKG_NUM_MODULE];
} __attribute__((packed));


#define PART_INFO_EXT_TYPE		3
#define PART_INFO_NUM_MODULE		1

struct partition_info_module {
	uint8_t name[PLAT_AUTH_NAME_LEN]; /* must be padded with 0 */
	uint8_t type;
	uint8_t reserved[3];
	uint32_t meta_size;
	uint8_t hash[PLAT_AUTH_SHA256_LEN];
}  __attribute__((packed));

struct partition_info_ext {
	uint32_t ext_type;
	uint32_t ext_len;

	uint8_t name[4];	/* "ADSP" */
	uint32_t length;
	uint8_t hash[PLAT_AUTH_SHA256_LEN];

	uint32_t vcn;
	uint32_t part_version;
	uint32_t fmt_version;
	uint32_t instance_id;
	uint32_t part_flags;
	uint8_t reserved[20];	/* must be 0 */

	/* variable length of modules */
	struct partition_info_module module[PART_INFO_NUM_MODULE];
} __attribute__((packed));


#define PLAT_AUTH_SIZE \
	(sizeof(struct partition_info_ext) + \
	sizeof(struct signed_pkg_info_ext))

void ri_adsp_meta_data_create(struct image *image);
void ri_plat_ext_data_create(struct image *image);

#endif
