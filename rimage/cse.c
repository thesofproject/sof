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
 *
 *  Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *          Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdio.h>
#include "rimage.h"
#include "cse.h"
#include "manifest.h"

void ri_cse_create(struct image *image)
{
	struct CsePartitionDirHeader *cse_hdr = image->fw_image;
	struct sof_man_adsp_meta_file_ext *meta = image->fw_image +
		MAN_META_EXT_OFFSET_V1_8;
	struct CsePartitionDirEntry *cse_entry =
		image->fw_image + sizeof(*cse_hdr);
	uint8_t csum = 0, *val = image->fw_image;
	int i, size;

	fprintf(stdout, " cse: completing CSE manifest\n");

	cse_entry[2].length = meta->comp_desc[0].limit_offset -
		MAN_DESC_OFFSET;

	/* calculate checksum using BSD algo */
	size = sizeof(*cse_hdr) + sizeof(*cse_entry) * MAN_CSE_PARTS;
	for (i = 0; i < size; i++) {
		if (i == 11)
			continue;
		csum += val[i];
	}
	cse_hdr->checksum = 0x100 - csum;
}
