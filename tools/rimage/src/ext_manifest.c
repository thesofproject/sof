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
		/* make sure a whole header remains before copying it out */
		if (offset + sizeof(head) > section_size) {
			fprintf(stderr,
				"error: extended manifest header straddles section end\n");
			return -EINVAL;
		}
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
	if (ret)
		/* ext_man_validate() already returns a negative errno; do not
		 * overwrite it with -errno, which is not set on validation
		 * failure and would mask the error
		 */
		goto out;

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
	write_ret = fwrite(&header, sizeof(header), 1, image->out_ext_man_fd);
	if (write_ret != 1) {
		ret = file_error("can't write fw_ext_man_cavs_header",
				 image->out_ext_man_file);
		goto out;
	}

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

/* gap before the fw header; must match kernel manifest_fw_hdr_offset (!= 0) */
#define IMX_EXT_MAN4_FW_HDR_OFFSET	48

/*
 * i.MX IPC4 extended manifest: the cAVS ext-manifest header, a gap, the fw
 * binary header and one module entry per ".module" record of the base FW.
 */
int ext_man_write_imx_ipc4(struct image *image)
{
	static const uint8_t fw_hdr_id[] = SOF_MAN_FW_HDR_ID;
	const struct sof_man_module_manifest *sof_mod;
	const struct manifest_module *module = NULL;
	struct fw_ext_man_cavs_header *ext_hdr;
	struct elf_section section = { 0 };
	struct sof_man_fw_header *fw_hdr;
	struct sof_man_module *mods;
	unsigned int n_mod, i;
	size_t ext_man_len;
	uint8_t *buf = NULL;
	int ret;

	for (i = 0; i < (unsigned int)image->num_modules; i++) {
		if (!image->module[i].is_bootloader) {
			module = &image->module[i];
			break;
		}
	}

	if (!module) {
		fprintf(stderr, "error: no base firmware module for IPC4 manifest\n");
		return -EINVAL;
	}

	ret = elf_section_read_by_name(&module->file.elf, ".module", &section);
	if (ret) {
		fprintf(stderr, "error: can't read '.module' section for IPC4 manifest\n");
		return ret;
	}

	n_mod = section.header.data.size / sizeof(*sof_mod);
	if (!n_mod || n_mod * sizeof(*sof_mod) != section.header.data.size) {
		fprintf(stderr, "error: invalid '.module' section for IPC4 manifest\n");
		ret = -EINVAL;
		goto out;
	}

	ext_man_len = sizeof(*ext_hdr) + IMX_EXT_MAN4_FW_HDR_OFFSET +
		      sizeof(*fw_hdr) + n_mod * sizeof(*mods);
	buf = calloc(1, ext_man_len);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* the kernel finds the fw header at ext_hdr->len + manifest_fw_hdr_offset */
	ext_hdr = (struct fw_ext_man_cavs_header *)buf;
	ext_hdr->id = EXTENDED_MANIFEST_MAGIC_HEADER_ID;
	ext_hdr->len = sizeof(*ext_hdr);
	ext_hdr->version_major = 4;
	ext_hdr->version_minor = 0;
	ext_hdr->num_module_entries = n_mod;

	fw_hdr = (struct sof_man_fw_header *)(buf + sizeof(*ext_hdr) +
					      IMX_EXT_MAN4_FW_HDR_OFFSET);
	memcpy(fw_hdr->header_id, fw_hdr_id, sizeof(fw_hdr->header_id));
	fw_hdr->header_len = sizeof(*fw_hdr);
	memcpy(fw_hdr->name, SOF_MAN_FW_HDR_NAME, sizeof(SOF_MAN_FW_HDR_NAME) - 1);
	fw_hdr->major_version = image->fw_ver_major;
	fw_hdr->minor_version = image->fw_ver_minor;
	fw_hdr->hotfix_version = image->fw_ver_micro;
	fw_hdr->build_version = image->fw_ver_build;
	fw_hdr->num_module_entries = n_mod;

	/* module id == table index; no config blocks or segments on i.MX */
	mods = (struct sof_man_module *)(fw_hdr + 1);
	sof_mod = section.data;
	for (i = 0; i < n_mod; i++) {
		memcpy(&mods[i], &sof_mod[i].module, sizeof(mods[i]));
		mods[i].runtime_info.module_id = i;
		mods[i].runtime_info.state_flags = 0;
		mods[i].cfg_offset = 0;
		mods[i].cfg_count = 0;
		mods[i].instance_bss_size = 0;
		memset(mods[i].segment, 0, sizeof(mods[i].segment));
	}

	ret = ext_man_open_file(image);
	if (ret)
		goto out;

	if (fwrite(buf, ext_man_len, 1, image->out_ext_man_fd) != 1) {
		ret = file_error("can't write IPC4 extended manifest",
				 image->out_ext_man_file);
		goto out;
	}

	fprintf(stdout, "imx: IPC4 extended manifest: %zu bytes, %u module(s)\n",
		ext_man_len, n_mod);
out:
	if (image->out_ext_man_fd) {
		fclose(image->out_ext_man_fd);
		image->out_ext_man_fd = NULL;
	}
	free(buf);
	elf_section_free(&section);
	return ret;
}
