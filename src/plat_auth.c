// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <rimage/rimage.h>
#include <rimage/manifest.h>
#include <rimage/plat_auth.h>

void ri_adsp_meta_data_create_v1_8(struct image *image, int meta_start_offset,
				   int meta_end_offset)
{
	struct sof_man_adsp_meta_file_ext_v1_8 *meta =
		image->fw_image + meta_start_offset;

	fprintf(stdout, " meta: completing ADSP manifest\n");

	meta->comp_desc[0].limit_offset = MAN_DESC_OFFSET_V1_8 +
		image->image_end - meta_end_offset;

	fprintf(stdout, " meta: limit is 0x%x\n",
		meta->comp_desc[0].limit_offset);
	/* now hash the AdspFwBinaryDesc -> EOF */
}

void ri_adsp_meta_data_create_v2_5(struct image *image, int meta_start_offset,
				   int meta_end_offset)
{
	struct sof_man_adsp_meta_file_ext_v2_5 *meta =
		image->fw_image + meta_start_offset;

	fprintf(stdout, " meta: completing ADSP manifest\n");

	meta->comp_desc[0].limit_offset = MAN_DESC_OFFSET_V1_8 +
		image->image_end - meta_end_offset;

	fprintf(stdout, " meta: limit is 0x%x\n",
		meta->comp_desc[0].limit_offset);
	/* now hash the AdspFwBinaryDesc -> EOF */
}

void ri_plat_ext_data_create(struct image *image)
{
	struct partition_info_ext *part = image->fw_image
		+ MAN_PART_INFO_OFFSET_V1_8;
	struct sof_man_adsp_meta_file_ext_v1_8 *meta =
		image->fw_image + MAN_META_EXT_OFFSET_V1_8;
	struct sof_man_fw_desc *desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	fprintf(stdout, " auth: completing authentication manifest\n");

	part->length = meta->comp_desc[0].limit_offset - MAN_DESC_OFFSET_V1_8;
	part->length += MAN_PAGE_SIZE - (part->length % MAN_PAGE_SIZE);

	/* do this here atm */
	desc->header.preload_page_count = part->length / MAN_PAGE_SIZE;
}

void ri_plat_ext_data_create_v2_5(struct image *image)
{
	struct sof_man_adsp_meta_file_ext_v2_5 *meta =
		image->fw_image + MAN_META_EXT_OFFSET_V2_5;
	struct sof_man_fw_desc *desc = image->fw_image + MAN_DESC_OFFSET_V1_8;
	struct info_ext_0x16 *ext = image->fw_image + MAN_PART_INFO_OFFSET_V2_5;
	uint32_t size;

	fprintf(stdout, " auth: completing authentication manifest\n");

	size = meta->comp_desc[0].limit_offset - MAN_DESC_OFFSET_V1_8;
	size += MAN_PAGE_SIZE - (size % MAN_PAGE_SIZE);

	/* do this here atm */
	desc->header.preload_page_count = size / MAN_PAGE_SIZE;
	ext->size = image->image_end;
}
