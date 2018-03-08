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

#ifndef __CSE_H__
#define __CSE_H__

#include <stdint.h>

struct image;

#define CSE_HEADER_MAKER   0x44504324	/* "DPC$" */

struct CsePartitionDirHeader {
	uint32_t header_marker;
	uint32_t nb_entries;
	uint8_t  header_version;
	uint8_t  entry_version;
	uint8_t  header_length;
	uint8_t  checksum;
	uint8_t  partition_name[4];
}  __attribute__((packed));

struct CsePartitionDirEntry {
	uint8_t  entry_name[12];
	uint32_t offset;
	uint32_t length;
	uint32_t reserved;
}  __attribute__((packed));

void ri_cse_create(struct image *image);

#endif
