/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
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

struct CsePartitionDirHeader_v2_5 {
	uint32_t header_marker;
	uint32_t nb_entries;
	uint8_t  header_version;
	uint8_t  entry_version;
	uint8_t  header_length;
	uint8_t  not_used; /* set to zero - old checksum */
	uint8_t  partition_name[4];
	uint32_t checksum; /* crc32 checksum */
}  __attribute__((packed));

struct CsePartitionDirEntry {
	uint8_t  entry_name[12];
	uint32_t offset;
	uint32_t length;
	uint32_t reserved;
}  __attribute__((packed));

void ri_cse_create(struct image *image);
void ri_cse_create_v2_5(struct image *image);

#endif
