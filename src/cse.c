// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <stdio.h>
#include <stdbool.h>
#include <rimage/rimage.h>
#include <rimage/cse.h>
#include <rimage/manifest.h>

void ri_cse_create(struct image *image)
{
	struct CsePartitionDirHeader *cse_hdr = image->fw_image;
	struct sof_man_adsp_meta_file_ext_v1_8 *meta = image->fw_image +
		MAN_META_EXT_OFFSET_V1_8;
	struct CsePartitionDirEntry *cse_entry =
		image->fw_image + sizeof(*cse_hdr);
	uint8_t csum = 0, *val = image->fw_image;
	int i, size;

	fprintf(stdout, " cse: completing CSE V1.8 manifest\n");

	cse_entry[2].length = meta->comp_desc[0].limit_offset -
		MAN_DESC_OFFSET_V1_8;

	/* calculate checksum using BSD algo */
	size = sizeof(*cse_hdr) + sizeof(*cse_entry) * MAN_CSE_PARTS;
	for (i = 0; i < size; i++) {
		if (i == 11)
			continue;
		csum += val[i];
	}
	cse_hdr->checksum = 0x100 - csum;
}

static uint32_t crc32(uint8_t *input, int size, uint32_t poly, uint32_t init,
		      bool rev_in, bool rev_out, uint32_t xor_out)
{
	uint32_t crc = init;
	uint32_t t32;
	uint8_t val;
	uint8_t t8;
	int i;
	int j;

	for (i = 0; i < size; i++) {
		val = input[i];
		if (rev_in) {
			t8 = 0;
			for (j = 0; j < 8; j++) {
				if (val & (1 << j))
					t8 |= (uint8_t)(1 << (7 - j));
			}
			val = t8;
		}
		crc ^= (uint32_t)(val << 24);
		for (j = 0; j < 8; j++) {
			if (crc & 0x80000000)
				crc = (uint32_t)((crc << 1) ^ poly);
			else
				crc <<= 1;
		}
	}

	if (rev_out) {
		t32 = 0;
		for (i = 0; i < 32; i++) {
			if (crc & (1 << i))
				t32 |= (uint32_t)(1 << (31 - i));
		}
		crc = t32;
	}

	return crc ^ xor_out;
}

void ri_cse_create_v2_5(struct image *image)
{
	struct CsePartitionDirHeader_v2_5 *cse_hdr = image->fw_image;
	struct sof_man_adsp_meta_file_ext_v2_5 *meta = image->fw_image +
		MAN_META_EXT_OFFSET_V2_5;
	struct CsePartitionDirEntry *cse_entry =
		image->fw_image + sizeof(*cse_hdr);
	uint8_t *val = image->fw_image;
	int size;

	fprintf(stdout, " cse: completing CSE V2.5 manifest\n");

	cse_entry[2].length = meta->comp_desc[0].limit_offset -
		MAN_DESC_OFFSET_V1_8;

	/*
	 * calculate checksum using crc-32/iso-hdlc
	 *
	 * polynomial: 0x04c11db7
	 * initial value: 0xffffffff
	 * reverse input: true
	 * reverse output: true
	 * xor output: 0xffffffff
	 */
	size = (sizeof(*cse_hdr) + (sizeof(*cse_entry) * MAN_CSE_PARTS));
	cse_hdr->checksum = crc32(val, size, 0x04c11db7, 0xffffffff, true, true, 0xffffffff);

	fprintf(stdout, " cse: cse checksum %x\n", cse_hdr->checksum);
}
