// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <rimage/ext_manifest_gen.h>
#include <rimage/sof/kernel/ext_manifest.h>
#include <rimage/rimage.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/manifest.h>
#include <rimage/file_utils.h>

const struct ext_man_header ext_man_template = {
	.magic = EXT_MAN_MAGIC_NUMBER,
	.header_version = EXT_MAN_VERSION,
	.header_size = sizeof(struct ext_man_header),
	.full_size = 0,	/* runtime variable */
};

static int ext_man_open_file(struct image *image)
{
	int ret;

	ret = create_file_name(image->out_ext_man_file, sizeof(image->out_ext_man_file),
			       image->out_file, "xman");
	if (ret)
		return ret;

	/* open extended manifest outfile for writing */
	image->out_ext_man_fd = fopen(image->out_ext_man_file, "wb");
	if (!image->out_ext_man_fd)
		return file_error("unable to open file for writing", image->out_ext_man_file);

	return 0;
}

static const struct elf_file *ext_man_find_module(const struct image *image,
					     const struct elf_section_header **section)
{
	const struct manifest_module *module;
	int i;

	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];

		if (module->is_bootloader)
			continue;

		if (!elf_section_header_get_by_name(&module->file.elf, EXT_MAN_DATA_SECTION,
						    section))
			return &module->file.elf;
	}

	return NULL;
}

static int ext_man_validate(uint32_t section_size, const void *section_data)
{
	uint8_t *sbuf = (uint8_t *)section_data;
	struct ext_man_elem_header head;
	uint32_t offset = 0;

	/* copy each head to local struct to omit memory align issues */
	while (offset < section_size) {
		memcpy(&head, &sbuf[offset], sizeof(head));
		fprintf(stdout, "Extended manifest found module, type: 0x%04X size: 0x%04X (%4d) offset: 0x%04X\n",
			head.type, head.elem_size, head.elem_size, offset);
		if (head.elem_size == 0 || head.elem_size % EXT_MAN_ALIGN) {
			fprintf(stderr,
				"error: invalid extended manifest element size\n");
			return -EINVAL;
		}
		offset += head.elem_size;
	}

	/* sum of packets size != section size */
	if (offset != section_size) {
		fprintf(stderr,
			"error: fw_metadata section is inconsistent, section size: 0x%04X != 0x%04X sum of packets size\n",
			section_size, offset);
		return -EINVAL;
	} else {
		return 0;
	}
}

static int ext_man_build(const struct elf_file *file, const struct elf_section_header *section,
			 struct ext_man_header **dst_buff)
{
	struct ext_man_header *ext_man;
	size_t size;
	int ret;

	size = ext_man_template.header_size + section->data.size;
	if (size % 4) {
		fprintf(stderr, "error: extended manifest size must be aligned to 4\n");
		return -EINVAL;
	}

	ext_man = calloc(1, size);
	if (!ext_man)
		return -ENOMEM;

	/* fill ext_man struct, size aligned to 4 to avoid unaligned accesses */
	memcpy(ext_man, &ext_man_template, ext_man_template.header_size);
	ext_man->full_size = size;

	ret = elf_section_read_content(file, section, ext_man + 1,
				       size - ext_man_template.header_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read %s section content, code %d\n",
			EXT_MAN_DATA_SECTION, ret);
		free(ext_man);
		return ret;
	}

	*dst_buff = ext_man;
	return 0;
}

int ext_man_write(struct image *image)
{
	const struct elf_file *file;
	struct ext_man_header *ext_man = NULL;
	const struct elf_section_header *section;
	int count;
	int ret;

	ret = ext_man_open_file(image);
	if (ret)
		goto out;

	file = ext_man_find_module(image, &section);
	if (!file) {
		ret = -ECANCELED;
		goto out;
	}

	ret = ext_man_build(file, section, &ext_man);
	if (ret)
		goto out;

	/* validate metadata section */
	ret = ext_man_validate(ext_man->full_size - ext_man->header_size,
			       (char *)ext_man + ext_man->header_size);
	if (ret) {
		ret = -errno;
		goto out;
	}

	/* write extended metadata to file */
	count = fwrite(ext_man, 1, ext_man->full_size, image->out_ext_man_fd);

	if (count != ext_man->full_size) {
		ret = file_error("can't write extended manifest", image->out_ext_man_file);
		goto out;
	}

	fprintf(stdout, "Extended manifest saved to file %s size 0x%04X (%d) bytes\n\n",
		image->out_ext_man_file, ext_man->full_size,
		ext_man->full_size);

out:
	if (ext_man)
		free(ext_man);
	if (image->out_ext_man_fd)
		fclose(image->out_ext_man_fd);
	return ret;
}

int ext_man_write_cavs_25(struct image *image)
{
	struct fw_image_ext_module *mod_ext;
	struct fw_ext_man_cavs_header header;
	int pin_count;
	int count, i;
	int ret;
	size_t write_ret;

	ret = ext_man_open_file(image);
	if (ret)
		goto out;

	mod_ext = &image->adsp->modules->mod_ext;
	count = mod_ext->mod_conf_count;
	header.version_major = EXTENDED_MANIFEST_VERSION_MAJOR;
	header.version_minor = EXTENDED_MANIFEST_VERSION_MINOR;
	header.num_module_entries = count;
	header.id = EXTENDED_MANIFEST_MAGIC_HEADER_ID;
	header.len = sizeof(const struct fw_ext_man_cavs_header);

	for (i = 0; i < count; i++)
		header.len += mod_ext->ext_mod_config_array[i].header.ext_module_config_length;
	fwrite(&header, 1, sizeof(header), image->out_ext_man_fd);

	for (i = 0; i < count; i++) {
		write_ret = fwrite(&mod_ext->ext_mod_config_array[i].header,
				   sizeof(struct fw_ext_mod_config_header), 1,
				   image->out_ext_man_fd);
		if (write_ret != 1) {
			ret = file_error("can't write fw_ext_mod_config_header",
					 image->out_ext_man_file);
			goto out;
		}

		if (mod_ext->ext_mod_config_array[i].header.num_scheduling_capabilities) {
			write_ret = fwrite(&mod_ext->ext_mod_config_array[i].sched_caps,
					   sizeof(struct mod_scheduling_caps), 1,
					   image->out_ext_man_fd);
			if (write_ret != 1) {
				ret = file_error("can't write mod_scheduling_caps",
						 image->out_ext_man_file);
				goto out;
			}
		}

		pin_count = mod_ext->ext_mod_config_array[i].header.num_pin_entries;
		if (pin_count) {
			write_ret = fwrite(mod_ext->ext_mod_config_array[i].pin_desc,
					   sizeof(struct fw_pin_description), pin_count,
					   image->out_ext_man_fd);

			if (write_ret != pin_count) {
				ret = file_error("can't write fw_pin_description",
						 image->out_ext_man_file);
				goto out;
			}
		}
	}

out:
	if (image->out_ext_man_fd)
		fclose(image->out_ext_man_fd);
	return ret;
}
