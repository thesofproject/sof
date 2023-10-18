// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <rimage/sof/user/manifest.h>

#include <rimage/rimage.h>
#include <rimage/css.h>
#include <rimage/cse.h>
#include <rimage/plat_auth.h>
#include <rimage/manifest.h>
#include <rimage/file_utils.h>
#include <rimage/misc_utils.h>
#include <rimage/hash.h>

static int man_open_rom_file(struct image *image)
{
	uint32_t size;
	int ret;

	ret = create_file_name(image->out_rom_file, sizeof(image->out_rom_file), image->out_file,
			       "rom");
	if (ret)
		return ret;

	size = image->adsp->mem.zones[SOF_FW_BLK_TYPE_ROM].size;

	/* allocate ROM image  */
	image->rom_image = calloc(size, 1);
	if (!image->rom_image)
		return -ENOMEM;

	/* open ROM outfile for writing */
	image->out_rom_fd = fopen(image->out_rom_file, "wb");
	if (!image->out_rom_fd)
		return file_error("unable to open file for writing", image->out_rom_file);

	return 0;
}

static int man_open_unsigned_file(struct image *image)
{
	int ret;

	ret = create_file_name(image->out_unsigned_file, sizeof(image->out_unsigned_file),
			       image->out_file, "uns");
	if (ret)
		return ret;

	/* open unsigned FW outfile for writing */
	image->out_unsigned_fd = fopen(image->out_unsigned_file, "wb");
	if (!image->out_unsigned_fd)
		return file_error("unable to open file for writing", image->out_unsigned_file);

	return 0;
}

static int man_open_manifest_file(struct image *image)
{
	int ret;

	ret = create_file_name(image->out_man_file, sizeof(image->out_man_file), image->out_file,
			       "met");
	if (ret)
		return ret;

	/* open manifest outfile for writing */
	image->out_man_fd = fopen(image->out_man_file, "wb");
	if (!image->out_man_fd)
		return file_error("unable to open file for writing", image->out_man_file);

	return 0;
}

static int man_init_image_v1_5(struct image *image)
{
	/* allocate image and copy template manifest */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image)
		return -ENOMEM;

	memcpy(image->fw_image, image->adsp->man_v1_5,
	       sizeof(struct fw_image_manifest_v1_5));

	return 0;
}

static int man_init_image_v1_5_sue(struct image *image)
{
	/* allocate image and copy template manifest */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image)
		return -ENOMEM;

	/* copy 1.5 sue manifest */
	memcpy(image->fw_image + MAN_DESC_OFFSET_V1_5_SUE,
	       image->adsp->man_v1_5_sue,
	       sizeof(struct fw_image_manifest_v1_5_sue));

	return 0;
}

static int man_init_image_v1_8(struct image *image)
{
	/* allocate image and copy template manifest */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image)
		return -ENOMEM;

	memcpy(image->fw_image, image->adsp->man_v1_8,
	       sizeof(struct fw_image_manifest_v1_8));

	return 0;
}

static int man_init_image_v2_5(struct image *image)
{
	/* allocate image and copy template manifest */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image)
		return -ENOMEM;

	memcpy(image->fw_image, image->adsp->man_v2_5,
	       sizeof(struct fw_image_manifest_v2_5));

	return 0;
}

/* write SRAM sections */
static int man_copy_sram(struct image *image, const struct manifest_module *module,
			 const struct sof_man_segment_desc *segment,
			 const struct module_section *section)
{
	uint32_t offset, end;
	int ret;

	assert(section->load_address >= segment->v_base_addr);
	offset = segment->file_offset + section->load_address - segment->v_base_addr;
	end = offset + section->size;
	assert((uint64_t)offset + section->size <= image->adsp->image_size);

	ret = module_read_section(&module->file, section, image->fw_image + offset,
				  image->adsp->image_size - offset);
	if (ret)
		return ret;

	/* get module end offset  ? */
	if (end > image->image_end)
		image->image_end = end;

	fprintf(stdout, "\t0x%x\t0x%zx\t\t0x%x\t%s\t%s\n", section->load_address, section->size,
		offset, section->type == MST_TEXT ? "TEXT" : "DATA", section->header->name);

	return 0;
}

/**
 * Write all linked sections
 * 
 * @param image program global structure
 * @param module modules manifest description
 * @param section module section descriptor
 * @return error code on error, 0 on success
 */
static int man_copy_elf_sections(struct image *image, struct manifest_module *module,
				 const struct sof_man_segment_desc *segment,
				 const struct module_section *section)
{
	int ret;

	while (section) {
		ret = man_copy_sram(image, module, segment, section);
		if (ret < 0) {
			fprintf(stderr, "error: failed to write section %s\n",
				section->header->name);
			return ret;
		}

		section = section->next_section;
	}

	return 0;
}

static int man_get_module_manifest(struct image *image, struct manifest_module *module,
				   struct sof_man_module *man_module)
{
	struct elf_section section;
	struct sof_man_segment_desc *segment;
	const struct sof_man_module_manifest *sof_mod;
	int ret;

	fprintf(stdout, "Module Write: %s\n", module->file.elf.filename);

	/* load in module manifest data */
	ret = elf_section_read_by_name(&module->file.elf, ".module", &section);
	if (ret) {
		fprintf(stderr, "error: can't read module manifest from '.module' section.\n");
		return ret;
	}

	if (sizeof(*sof_mod) > section.header.data.size) {
		fprintf(stderr, "error: Invalid module manifest in '.module' section.\n");
		ret = -ENODATA;
		goto error;
	}
	sof_mod = section.data;

	/* configure man_module with sofmod data */
	memcpy(man_module->struct_id, "$AME", 4);
	man_module->entry_point = sof_mod->module.entry_point;
	memcpy(man_module->name, sof_mod->module.name, SOF_MAN_MOD_NAME_LEN);
	memcpy(man_module->uuid, sof_mod->module.uuid, 16);
	man_module->affinity_mask = sof_mod->module.affinity_mask;
	man_module->type.auto_start = sof_mod->module.type.auto_start;
	man_module->type.domain_dp = sof_mod->module.type.domain_dp;
	man_module->type.domain_ll = sof_mod->module.type.domain_ll;
	man_module->type.load_type = sof_mod->module.type.load_type;

	/* read out text_fixup_size from memory mapping */
	module->text_fixup_size = sof_mod->text_size;

	/* text segment */
	segment = &man_module->segment[SOF_MAN_SEGMENT_TEXT];
	segment->flags.r.contents = 1;
	segment->flags.r.alloc = 1;
	segment->flags.r.load = 1;
	segment->flags.r.readonly = 1;
	segment->flags.r.code = 1;

	/* data segment */
	segment = &man_module->segment[SOF_MAN_SEGMENT_RODATA];
	segment->flags.r.contents = 1;
	segment->flags.r.alloc = 1;
	segment->flags.r.load = 1;
	segment->flags.r.readonly = 1;
	segment->flags.r.data = 1;
	segment->flags.r.type = 1;

	/* bss segment */
	segment = &man_module->segment[SOF_MAN_SEGMENT_BSS];
	segment->flags.r.alloc = 1;
	segment->flags.r.type = 2;

	fprintf(stdout, " Entry point 0x%8.8x\n", man_module->entry_point);

error:
	elf_section_free(&section);
	return ret;
}

static inline const char *segment_name(int i)
{
	switch (i) {
	case SOF_MAN_SEGMENT_TEXT:
		return "TEXT";
	case SOF_MAN_SEGMENT_RODATA:
		return "DATA";
	case SOF_MAN_SEGMENT_BSS:
		return "BSS";
	default:
		return "NONE";
	}
}

/* make sure no segments collide */
static int man_module_validate(struct sof_man_module *man_module)
{
	uint32_t istart, iend;
	uint32_t jstart, jend;
	int i, j;

	for (i = 0; i < 3; i++) {
		istart = man_module->segment[i].v_base_addr;
		iend = istart + man_module->segment[i].flags.r.length *
			MAN_PAGE_SIZE;

		for (j = 0; j < 3; j++) {
			/* don't validate segment against itself */
			if (i == j)
				continue;

			jstart = man_module->segment[j].v_base_addr;
			jend = jstart + man_module->segment[j].flags.r.length *
				MAN_PAGE_SIZE;

			if (jstart > istart && jstart < iend)
				goto err;

			if (jend > istart && jend < iend)
				goto err;
		}
	}

	/* success, no overlapping segments */
	return 0;

err:
	fprintf(stderr, "error: segment %s [0x%8.8x:0x%8.8x] overlaps",
		segment_name(i), istart, iend);
	fprintf(stderr, " with %s [0x%8.8x:0x%8.8x]\n",
		segment_name(j), jstart, jend);
	return -EINVAL;
}

static int man_module_create(struct image *image, struct manifest_module *module,
			     struct sof_man_module *man_module)
{
	/* create module and segments */
	int err;
	unsigned int pages;
	const struct elf_section_header *bss;

	image->image_end = 0;

	err = man_get_module_manifest(image, module, man_module);
	if (err < 0)
		return err;

	/* stack size ??? convert sizes to PAGES */
	man_module->instance_bss_size = 1;

	/* max number of instances of this module ?? */
	man_module->instance_max_count = 1;

	module_print_zones(&module->file);

	/* main module */
	fprintf(stdout, "\tAddress\t\tSize\t\tFile\tType\tName\n");

	/* text section is first */
	man_module->segment[SOF_MAN_SEGMENT_TEXT].file_offset = module->foffset;
	man_module->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr = module->file.text.start;

	/* calculates those padding 0s by the start of next segment */
	/* file_size is already aligned to MAN_PAGE_SIZE */
	pages = module->file.text.file_size / MAN_PAGE_SIZE;

	if (module->text_fixup_size == 0)
		module->text_fixup_size = module->file.text.file_size;

	/* check if text_file_size is bigger then text_fixup_size */
	if (module->file.text.file_size > module->text_fixup_size) {
		fprintf(stderr, "error: too small text size assigned!\n");
		return -EINVAL;
	}

	man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length = pages;

	/* Copy text sections content */
	err = man_copy_elf_sections(image, module, &man_module->segment[SOF_MAN_SEGMENT_TEXT],
				    module->file.text.first_section);
	if (err)
		return err;


	/* data section */
	man_module->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr = module->file.data.start;
	man_module->segment[SOF_MAN_SEGMENT_RODATA].file_offset = module->foffset +
								  module->text_fixup_size;

	/* file_size is already aligned to MAN_PAGE_SIZE */
	pages = module->file.data.file_size / MAN_PAGE_SIZE;

	man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length = pages;

	/* Copy data sections content */
	err = man_copy_elf_sections(image, module, &man_module->segment[SOF_MAN_SEGMENT_RODATA],
				    module->file.data.first_section);
	if (err)
		return err;

	/* bss is last */

	/* I do not understand why only the section named .bss was taken into account. Other
	 * sections of the same type were ignored (type = SHT_NOBITS, flags = SHF_ALLOC). I added
	 * the reading of the .bss section here, to not change the behavior of the program.
	 */
	bss = NULL;

	if (module->is_bootloader) {
		/* Bootloader should not have .bss section. */
		fprintf(stdout, "info: ignore .bss section for bootloader module\n");
	} else {
		err = elf_section_header_get_by_name(&module->file.elf, ".bss", &bss);
		if (err)
			fprintf(stderr, "warning: can't find '.bss' section in module %s.\n",
				module->file.elf.filename);

	}

	man_module->segment[SOF_MAN_SEGMENT_BSS].file_offset = 0;
	man_module->segment[SOF_MAN_SEGMENT_BSS].v_base_addr = 0;
	pages = 0;

	if (bss) {
		man_module->segment[SOF_MAN_SEGMENT_BSS].v_base_addr =
			uncache_to_cache(&image->adsp->mem.alias, bss->data.vaddr);

		pages = DIV_ROUND_UP(bss->data.size, MAN_PAGE_SIZE);
	}

	man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.length = pages;
	if (pages == 0) {
		man_module->segment[SOF_MAN_SEGMENT_BSS].flags.ul = 0;
		man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.type = SOF_MAN_SEGMENT_EMPTY;
	}

	if (man_module_validate(man_module) < 0)
		return -EINVAL;

	fprintf(stdout, "\n");

	/* no need to update end for exec headers */
	if (module->exec_header) {
		image->image_end = FILE_TEXT_OFFSET_V1_5_SUE;
		goto out;
	}

	/* round module end upto nearest page */
	if (image->image_end % MAN_PAGE_SIZE) {
		image->image_end = (image->image_end / MAN_PAGE_SIZE) + 1;
		image->image_end *= MAN_PAGE_SIZE;
	}

out:
	fprintf(stdout, " Total pages text %d data %d bss %d module file limit: 0x%x\n\n",
		man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length,
		man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length,
		man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.length,
		image->image_end);
	return 0;
}

static int man_module_create_reloc(struct image *image, struct manifest_module *module,
				   struct sof_man_module *man_module)
{
	/* create module and segments */
	int err;

	image->image_end = 0;

	err = man_get_module_manifest(image, module, man_module);
	if (err < 0)
		return err;

	/* stack size ??? convert sizes to PAGES */
	man_module->instance_bss_size = 1;

	/* max number of instances of this module ?? */
	man_module->instance_max_count = 1;

	module_print_zones(&module->file);

	/* main module */
	/* text section is first */
	man_module->segment[SOF_MAN_SEGMENT_TEXT].file_offset =
		module->foffset;
	man_module->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr = 0;
	man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length = 0;

	/* data section */
	man_module->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr = 0;
	man_module->segment[SOF_MAN_SEGMENT_RODATA].file_offset = module->foffset;
	man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length = module->file.data.file_size /
								     MAN_PAGE_SIZE;

	/* bss is last */
	man_module->segment[SOF_MAN_SEGMENT_BSS].file_offset = 0;
	man_module->segment[SOF_MAN_SEGMENT_BSS].v_base_addr = 0;
	man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.length = 0;

	fprintf(stdout, "\tNo\tAddress\t\tSize\t\tFile\tType\n");

	assert((module->file.elf.file_size + module->foffset) <= image->adsp->image_size);
	err = module_read_whole_elf(&module->file, image->fw_image + module->foffset,
				    image->image_end - module->foffset);
	if (err)
		return err;

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8zx\t0x%x\t%s\n", 0,
		0, module->file.elf.file_size, 0, "DATA");

	fprintf(stdout, "\n");
	image->image_end = module->foffset + module->file.elf.file_size;

	/* round module end up to nearest page */
	if (image->image_end % MAN_PAGE_SIZE) {
		image->image_end = (image->image_end / MAN_PAGE_SIZE) + 1;
		image->image_end *= MAN_PAGE_SIZE;
	}

	fprintf(stdout, " Total pages text %d data %d bss %d module file limit: 0x%x\n\n",
		man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length,
		man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length,
		man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.length,
		image->image_end);
	return 0;
}

static int man_write_unsigned_mod(struct image *image, int meta_start_offset,
				  int meta_end_offset, size_t ext_file_size)
{
	int count;

	/* write metadata file for unsigned FW */
	count = fwrite(image->fw_image + meta_start_offset,
		       ext_file_size, 1,
		       image->out_man_fd);

	/* did the metadata/manifest write succeed ? */
	if (count != 1)
		return file_error("failed to write meta", image->out_man_file);

	fclose(image->out_man_fd);

	/* now prepare the unsigned rimage */
	count = fwrite(image->fw_image + meta_end_offset,
		       image->image_end - meta_end_offset,
		       1, image->out_unsigned_fd);

	/* did the unsigned FW write succeed ? */
	if (count != 1)
		return file_error("failed to write firmware", image->out_unsigned_file);
	fclose(image->out_unsigned_fd);

	return 0;
}

static int man_write_fw_mod(struct image *image)
{
	int count;

	/* write manifest and signed image */
	count = fwrite(image->fw_image, image->image_end, 1, image->out_fd);

	/* did the image write succeed ? */
	if (count != 1)
		return file_error("failed to write signed firmware", image->out_file);

	return 0;
}

static int man_create_modules(struct image *image, struct sof_man_fw_desc *desc,
			      int file_text_offset)
{
	struct manifest_module *module;
	struct sof_man_module *man_module;
	int err;
	int i = 0, offset = 0;

	/* if first module is executable then write before manifest */
	if (image->adsp->exec_boot_ldr) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(0);
		module = &image->module[0];

		fprintf(stdout, "Module: %s used as executable header\n",
			module->file.elf.filename);
		module->exec_header = 1;

		/* set module file offset */
		module->foffset = 0;

		err = man_module_create(image, module, man_module);
		if (err < 0)
			return err;

		/* setup man_modules for missing exec loader module */
		i = 1;
		offset = 1;
	}

	for (; i < image->num_modules; i++) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(i - offset);

		/* Some platforms dont have modules configuration in toml file */
		if (image->adsp->modules) {
			/* Use manifest created using toml files as template */
			assert(i < image->adsp->modules->mod_man_count);
			memcpy(man_module, &image->adsp->modules->mod_man[i], sizeof(*man_module));
		}

		module = &image->module[i];

		if (i == 0)
			module->foffset = file_text_offset;
		else
			module->foffset = image->image_end;

		if (image->reloc)
			err = man_module_create_reloc(image, module,
						      man_module);
		else
			err = man_module_create(image, module, man_module);

		if (err < 0)
			return err;
	}

	return 0;
}

static void man_create_modules_in_config(struct image *image, struct sof_man_fw_desc *desc)
{
	struct fw_image_manifest_module *modules;
	struct sof_man_module *man_module;
	void *cfg_start;
	int i;

	modules = image->adsp->modules;
	if (!modules)
		return;

	/* skip modules passed as parameters. Their manifests have already been copied by the
	 * man_create_modules function. */
	for (i = image->num_modules; i < modules->mod_man_count; i++) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(i);
		memcpy(man_module, &modules->mod_man[i], sizeof(*man_module));
	}

	/* We need to copy the configurations for all modules. */
	cfg_start = (void *)desc + SOF_MAN_MODULE_OFFSET(i);
	memcpy(cfg_start, modules->mod_cfg, modules->mod_cfg_count * sizeof(struct sof_man_mod_config));

	desc->header.num_module_entries = modules->mod_man_count;
}

static int man_hash_modules(struct image *image, struct sof_man_fw_desc *desc)
{
	struct sof_man_module *man_module;
	size_t mod_offset, mod_size;
	int i, ret = 0;

	for (i = 0; i < image->num_modules; i++) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(i);

		if (image->adsp->exec_boot_ldr && i == 0) {
			fprintf(stdout, " module: no need to hash %s\n as its exec header\n",
				man_module->name);
			continue;
		}

		mod_offset = man_module->segment[SOF_MAN_SEGMENT_TEXT].file_offset;
		mod_size = (man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length +
			man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length) *
			MAN_PAGE_SIZE;

		assert((mod_offset + mod_size) <= image->adsp->image_size);

		ret = hash_sha256(image->fw_image + mod_offset, mod_size, man_module->hash, sizeof(man_module->hash));
		if (ret)
			break;
	}

	return ret;
}

/* used by others */
int man_write_fw_v1_5(struct image *image)
{
	struct sof_man_fw_desc *desc;
	struct fw_image_manifest_v1_5 *m;
	int ret;

	/* init image */
	ret = man_init_image_v1_5(image);
	if (ret < 0)
		goto err;

	/* open ROM image */
	ret = man_open_rom_file(image);
	if (ret < 0)
		goto err;

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	m = image->fw_image;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_5;

	/* firmware and build version */
	m->desc.header.major_version = image->fw_ver_major;
	m->desc.header.minor_version = image->fw_ver_minor;
	m->desc.header.hotfix_version = image->fw_ver_micro;
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_5);
	if (ret)
		goto err;

	fprintf(stdout, "Firmware completing manifest v1.5\n");

	/* create structures from end of file to start of file */
	ri_css_v1_5_hdr_create(image);

	fprintf(stdout, "Firmware file size 0x%x page count %d\n",
		FILE_TEXT_OFFSET_V1_5 - MAN_DESC_OFFSET_V1_5 + image->image_end,
		desc->header.preload_page_count);

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* sign manifest */
	ret = ri_manifest_sign_v1_5(image);
	if (ret < 0)
		goto err;

	/* write the firmware */
	ret = man_write_fw_mod(image);
	if (ret < 0)
		goto err;

	/* write the unsigned files*/
	ret = man_write_unsigned_mod(image, MAN_META_EXT_OFFSET_V1_5,
				     MAN_FW_DESC_OFFSET_V1_5,
				     sizeof(struct sof_man_adsp_meta_file_ext_v1_8));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest and signing completed !\n");
	return 0;

err:
	free(image->rom_image);
	free(image->fw_image);
	unlink(image->out_file);
	unlink(image->out_rom_file);
	return ret;
}

/* used by others */
int man_write_fw_v1_5_sue(struct image *image)
{
	struct fw_image_manifest_v1_5_sue *m;
	uint32_t preload_size;
	int ret;

	/* init image */
	ret = man_init_image_v1_5_sue(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	m = image->fw_image + MAN_DESC_OFFSET_V1_5_SUE;

	/* firmware and build version */
	m->desc.header.major_version = image->fw_ver_major;
	m->desc.header.minor_version = image->fw_ver_minor;
	m->desc.header.hotfix_version = image->fw_ver_micro;
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module - subtract the boot loader exec header */
	m->desc.header.num_module_entries = image->num_modules - 1;
	ret = man_create_modules(image, &m->desc, FILE_TEXT_OFFSET_V1_5_SUE);
	if (ret)
		goto err;

	fprintf(stdout, "Firmware completing manifest v1.5\n");

	/* write preload page count */
	preload_size = image->image_end - MAN_DESC_OFFSET_V1_5_SUE;
	preload_size += MAN_PAGE_SIZE - (preload_size % MAN_PAGE_SIZE);
	m->desc.header.preload_page_count = preload_size / MAN_PAGE_SIZE;

	fprintf(stdout, "Firmware file size 0x%x page count %d\n",
		FILE_TEXT_OFFSET_V1_5_SUE - MAN_DESC_OFFSET_V1_5_SUE +
		image->image_end, m->desc.header.preload_page_count);

	/* calculate hash for each module */
	man_hash_modules(image, &m->desc);

	/* write the firmware */
	ret = man_write_fw_mod(image);
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest and signing completed !\n");
	return 0;

err:
	free(image->fw_image);
	unlink(image->out_file);
	return ret;
}

/* used by others */
int man_write_fw_v1_8(struct image *image)
{
	struct sof_man_fw_desc *desc;
	struct fw_image_manifest_v1_8 *m;
	int ret;

	/* init image */
	ret = man_init_image_v1_8(image);
	if (ret < 0)
		goto err;

	/* open ROM image */
	ret = man_open_rom_file(image);
	if (ret < 0)
		goto err;

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	m = image->fw_image;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	/* firmware and build version */
	m->css.version.major_version = image->fw_ver_major;
	m->css.version.minor_version = image->fw_ver_minor;
	m->css.version.build_version = image->fw_ver_build;
	m->desc.header.major_version = image->fw_ver_major;
	m->desc.header.minor_version = image->fw_ver_minor;
	m->desc.header.hotfix_version = image->fw_ver_micro;
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
	if (ret)
		goto err;

	fprintf(stdout, "Firmware completing manifest v1.8\n");

	/* create structures from end of file to start of file */
	ri_adsp_meta_data_create_v1_8(image, MAN_META_EXT_OFFSET_V1_8,
				      MAN_FW_DESC_OFFSET_V1_8);
	ri_plat_ext_data_create(image);
	ri_css_v1_8_hdr_create(image);
	ri_cse_create(image);

	fprintf(stdout, "Firmware file size 0x%x page count %d\n",
		FILE_TEXT_OFFSET_V1_8 - MAN_DESC_OFFSET_V1_8 + image->image_end,
		desc->header.preload_page_count);

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* calculate hash for ADSP meta data extension - 0x480 to end */
	/* image_end is updated every time a section is added */
	assert(image->image_end > MAN_FW_DESC_OFFSET_V1_8);
	ret = hash_sha256(image->fw_image + MAN_FW_DESC_OFFSET_V1_8,
			  image->image_end - MAN_FW_DESC_OFFSET_V1_8,
			  m->adsp_file_ext.comp_desc[0].hash,
			  sizeof(m->adsp_file_ext.comp_desc[0].hash));
	if (ret)
		goto err;

	/* calculate hash for platform auth data - repeated in hash 2 and 4 */
	assert(image->image_end > (MAN_FW_DESC_OFFSET_V1_8 +
				   sizeof(struct sof_man_adsp_meta_file_ext_v1_8)));

	ret = hash_sha256(image->fw_image + MAN_FW_DESC_OFFSET_V1_8,
			  image->image_end - MAN_FW_DESC_OFFSET_V1_8,
			  m->signed_pkg.module[0].hash,
			  sizeof(m->signed_pkg.module[0].hash));
	if (ret)
		goto err;

	/* hash values in reverse order */
	bytes_swap(m->signed_pkg.module[0].hash, sizeof(m->signed_pkg.module[0].hash));

	/* Copy module hash to partition_info */
	assert(sizeof(m->partition_info.module[0].hash) == sizeof(m->signed_pkg.module[0].hash));
	memcpy(m->partition_info.module[0].hash, m->signed_pkg.module[0].hash,
	       sizeof(m->partition_info.module[0].hash));

	/* sign manifest */
	ret = ri_manifest_sign_v1_8(image);
	if (ret < 0)
		goto err;

	/* write the firmware */
	ret = man_write_fw_mod(image);
	if (ret < 0)
		goto err;

	/* write the unsigned files*/
	ret = man_write_unsigned_mod(image, MAN_META_EXT_OFFSET_V1_8,
				     MAN_FW_DESC_OFFSET_V1_8,
				     sizeof(struct sof_man_adsp_meta_file_ext_v1_8));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest and signing completed !\n");
	return 0;

err:
	free(image->rom_image);
	free(image->fw_image);
	unlink(image->out_file);
	unlink(image->out_rom_file);
	return ret;
}

/* used to sign with MEU */
int man_write_fw_meu_v1_5(struct image *image)
{
	const int meta_start_offset = image->meu_offset -
		sizeof(struct sof_man_adsp_meta_file_ext_v1_8) - MAN_EXT_PADDING;
	struct sof_man_adsp_meta_file_ext_v1_8 *meta;
	struct sof_man_fw_desc *desc;
	uint32_t preload_size;
	int ret;

	/* allocate image */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image) {
		ret = -ENOMEM;
		goto err;
	}

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	meta = image->fw_image + meta_start_offset;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_5;

	/* copy data */
	memcpy(desc, &image->adsp->man_v1_5->desc,
	       sizeof(struct sof_man_fw_desc));

	/* firmware and build version */
	desc->header.major_version = image->fw_ver_major;
	desc->header.minor_version = image->fw_ver_minor;
	desc->header.hotfix_version = image->fw_ver_micro;
	desc->header.build_version = image->fw_ver_build;

	/* create each module */
	desc->header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_5);
	if (ret)
		goto err;

	fprintf(stdout, "Firmware completing manifest v1.5\n");

	/* create structures from end of file to start of file */
	ri_adsp_meta_data_create_v1_8(image, meta_start_offset,
				      image->meu_offset);

	/* write preload page count */
	preload_size = meta->comp_desc[0].limit_offset - MAN_DESC_OFFSET_V1_5;
	preload_size += MAN_PAGE_SIZE - (preload_size % MAN_PAGE_SIZE);
	desc->header.preload_page_count = preload_size / MAN_PAGE_SIZE;

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* calculate hash for ADSP meta data extension */
	assert(image->meu_offset < image->image_end);
	ret = hash_sha256(image->fw_image + image->meu_offset, image->image_end - image->meu_offset,
			  meta->comp_desc[0].hash, sizeof(meta->comp_desc[0].hash));
	if (ret)
		goto err;

	/* write the unsigned files */
	ret = man_write_unsigned_mod(image, meta_start_offset,
				     image->meu_offset,
				     sizeof(struct sof_man_adsp_meta_file_ext_v1_8));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest completed!\n");
	return 0;

err:
	free(image->fw_image);
	unlink(image->out_file);
	return ret;
}

/* used to sign with MEU */
int man_write_fw_meu_v1_8(struct image *image)
{
	const int meta_start_offset = image->meu_offset -
		sizeof(struct sof_man_adsp_meta_file_ext_v1_8) - MAN_EXT_PADDING;
	struct sof_man_adsp_meta_file_ext_v1_8 *meta;
	struct sof_man_fw_desc *desc;
	uint32_t preload_size;
	int ret;

	/* allocate image */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image) {
		ret = -ENOMEM;
		goto err;
	}

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	meta = image->fw_image + meta_start_offset;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	/* copy data */
	memcpy(meta, &image->adsp->man_v1_8->adsp_file_ext,
	       sizeof(struct sof_man_adsp_meta_file_ext_v1_8));
	memcpy(desc, &image->adsp->man_v1_8->desc,
	       sizeof(struct sof_man_fw_desc));

	/* firmware and build version */
	desc->header.major_version = image->fw_ver_major;
	desc->header.minor_version = image->fw_ver_minor;
	desc->header.hotfix_version = image->fw_ver_micro;
	desc->header.build_version = image->fw_ver_build;

	/* create each module */
	desc->header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
	if (ret)
		goto err;

	fprintf(stdout, "Firmware completing manifest v1.8\n");

	/* create structures from end of file to start of file */
	ri_adsp_meta_data_create_v1_8(image, meta_start_offset,
				      image->meu_offset);

	/* write preload page count */
	preload_size = meta->comp_desc[0].limit_offset - MAN_DESC_OFFSET_V1_8;
	preload_size += MAN_PAGE_SIZE - (preload_size % MAN_PAGE_SIZE);
	desc->header.preload_page_count = preload_size / MAN_PAGE_SIZE;

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* calculate hash for ADSP meta data extension */
	assert(image->meu_offset < image->image_end);
	ret = hash_sha256(image->fw_image + image->meu_offset, image->image_end - image->meu_offset,
			  meta->comp_desc[0].hash, sizeof(meta->comp_desc[0].hash));
	if (ret)
		goto err;

	/* write the unsigned files */
	ret = man_write_unsigned_mod(image, meta_start_offset,
				     image->meu_offset,
				     sizeof(struct sof_man_adsp_meta_file_ext_v1_8));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest completed!\n");
	return 0;

err:
	free(image->fw_image);
	unlink(image->out_file);
	return ret;
}

/* used to sign with MEU */
int man_write_fw_meu_v2_5(struct image *image)
{
	const int meta_start_offset = image->meu_offset -
		sizeof(struct sof_man_adsp_meta_file_ext_v2_5) - MAN_EXT_PADDING;
	struct sof_man_adsp_meta_file_ext_v2_5 *meta;
	struct sof_man_fw_desc *desc;
	uint32_t preload_size;
	int ret;

	/* allocate image */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image) {
		ret = -ENOMEM;
		goto err;
	}

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	meta = image->fw_image + meta_start_offset;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	/* copy data */
	memcpy(meta, &image->adsp->man_v2_5->adsp_file_ext,
	       sizeof(struct sof_man_adsp_meta_file_ext_v2_5));
	memcpy(desc, &image->adsp->man_v2_5->desc,
	       sizeof(struct sof_man_fw_desc));

	/* firmware and build version */
	desc->header.major_version = image->fw_ver_major;
	desc->header.minor_version = image->fw_ver_minor;
	desc->header.hotfix_version = image->fw_ver_micro;
	desc->header.build_version = image->fw_ver_build;

	/* create each module */
	desc->header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
	if (ret)
		goto err;

	/* platform config defines some modules except bringup & base modules */
	man_create_modules_in_config(image, desc);

	fprintf(stdout, "Firmware completing manifest v2.5\n");

	/* create structures from end of file to start of file */
	ri_adsp_meta_data_create_v2_5(image, meta_start_offset,
				      image->meu_offset);

	/* write preload page count */
	preload_size = meta->comp_desc[0].limit_offset - MAN_DESC_OFFSET_V1_8;
	preload_size += MAN_PAGE_SIZE - (preload_size % MAN_PAGE_SIZE);
	desc->header.preload_page_count = preload_size / MAN_PAGE_SIZE;

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* calculate hash for ADSP meta data extension */
	assert(image->meu_offset < image->image_end);
	ret = hash_sha384(image->fw_image + image->meu_offset, image->image_end - image->meu_offset,
			  meta->comp_desc[0].hash, sizeof(meta->comp_desc[0].hash));
	if (ret)
		goto err;

	/* write the unsigned files */
	ret = man_write_unsigned_mod(image, meta_start_offset,
				     image->meu_offset,
				     sizeof(struct sof_man_adsp_meta_file_ext_v2_5));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest completed!\n");
	return 0;

err:
	free(image->fw_image);
	unlink(image->out_file);
	return ret;
}

/* used by others */
int man_write_fw_v2_5(struct image *image)
{
	struct sof_man_fw_desc *desc;
	struct fw_image_manifest_v2_5 *m;
	int ret;

	/* init image */
	ret = man_init_image_v2_5(image);
	if (ret < 0)
		goto err;

	/* use default meu offset for TGL if not provided */
	if (!image->meu_offset)
		image->meu_offset = MAN_FW_DESC_OFFSET_V2_5 - 0x10;

	/* open ROM image */
	ret = man_open_rom_file(image);
	if (ret < 0)
		goto err;

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	m = image->fw_image;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	/* firmware and build version */
	m->css.version.major_version = image->fw_ver_major;
	m->css.version.minor_version = image->fw_ver_minor;
	m->css.version.build_version = image->fw_ver_build;
	m->desc.header.major_version = image->fw_ver_major;
	m->desc.header.minor_version = image->fw_ver_minor;
	m->desc.header.hotfix_version = image->fw_ver_micro;
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
	if (ret)
		goto err;

	/* platform config defines some modules except bringup & base modules */
	man_create_modules_in_config(image, desc);

	fprintf(stdout, "Firmware completing manifest v2.5\n");

	/* create structures from end of file to start of file */
	ri_adsp_meta_data_create_v2_5(image, MAN_META_EXT_OFFSET_V2_5,
			image->meu_offset);
	ri_plat_ext_data_create_v2_5(image);
	ri_css_v2_5_hdr_create(image);
	ri_cse_create_v2_5(image);

	fprintf(stdout, "Firmware file size 0x%x page count %d\n",
		FILE_TEXT_OFFSET_V1_8 - MAN_DESC_OFFSET_V1_8 + image->image_end,
		desc->header.preload_page_count);

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* calculate hash inside ADSP meta data extension for padding to end */
	assert(image->meu_offset < image->image_end);
	ret = hash_sha384(image->fw_image + image->meu_offset, image->image_end - image->meu_offset,
			  m->adsp_file_ext.comp_desc[0].hash,
			  sizeof(m->adsp_file_ext.comp_desc[0].hash));
	if (ret)
		goto err;

	/* mue writes 0xff to 16 bytes of padding */
	memset(m->reserved, 0xff, 16);

	/* calculate hash inside ext info 16 of sof_man_adsp_meta_file_ext_v2_5 */
	assert((MAN_META_EXT_OFFSET_V2_5 + sizeof(struct sof_man_adsp_meta_file_ext_v2_5)) <
	       image->image_end);

	ret = hash_sha384(image->fw_image + MAN_META_EXT_OFFSET_V2_5,
			  sizeof(struct sof_man_adsp_meta_file_ext_v2_5),
			  m->signed_pkg.module[0].hash, sizeof(m->signed_pkg.module[0].hash));
	if (ret)
		goto err;

	/* hash values in reverse order */
	bytes_swap(m->signed_pkg.module[0].hash, sizeof(m->signed_pkg.module[0].hash));

	/* sign manifest */
	ret = ri_manifest_sign_v2_5(image);
	if (ret < 0)
		goto err;

#if 0
	/* calculate hash - SHA384 on CAVS2_5+ */
	module_sha384_create(image);
	module_sha_update(image, image->fw_image,
			sizeof(struct CsePartitionDirHeader_v2_5) +
			sizeof(struct CsePartitionDirEntry) * 3);
	module_sha_update(image, image->fw_image + 0x4c0, image->image_end - 0x4c0);
	module_sha_complete(image, hash);

	/* hash values in reverse order */
	for (i = 0; i < SOF_MAN_MOD_SHA384_LEN; i++) {
		m->info_0x16.hash[i] =
			hash[SOF_MAN_MOD_SHA384_LEN - 1 - i];
	}
#endif
	/* write the firmware */
	ret = man_write_fw_mod(image);
	if (ret < 0)
		goto err;

	/* write the unsigned files*/
	ret = man_write_unsigned_mod(image, MAN_META_EXT_OFFSET_V2_5,
				     MAN_FW_DESC_OFFSET_V2_5,
				     sizeof(struct sof_man_adsp_meta_file_ext_v2_5));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest and signing completed !\n");
	return 0;

err:
	free(image->rom_image);
	free(image->fw_image);
	unlink(image->out_file);
	unlink(image->out_rom_file);
	return ret;
}

static int man_init_image_ace_v1_5(struct image *image)
{
	/* allocate image and copy template manifest */
	image->fw_image = calloc(image->adsp->image_size, 1);
	if (!image->fw_image)
		return -ENOMEM;

	memcpy(image->fw_image, image->adsp->man_ace_v1_5,
	       sizeof(struct fw_image_manifest_ace_v1_5));

	return 0;
}

int man_write_fw_ace_v1_5(struct image *image)
{
	struct hash_context hash;
	struct sof_man_fw_desc *desc;
	struct fw_image_manifest_ace_v1_5 *m;
	int ret;

	/* init image */
	ret = man_init_image_ace_v1_5(image);
	if (ret < 0)
		goto err;

	/* use default meu offset for TGL if not provided */
	if (!image->meu_offset)
		image->meu_offset = MAN_FW_DESC_OFFSET_ACE_V1_5 - 0x10;

	/* open ROM image */
	ret = man_open_rom_file(image);
	if (ret < 0)
		goto err;

	/* open unsigned firmware */
	ret = man_open_unsigned_file(image);
	if (ret < 0)
		goto err;

	/* create the manifest */
	ret = man_open_manifest_file(image);
	if (ret < 0)
		goto err;

	/* create the module */
	m = image->fw_image;
	desc = image->fw_image + MAN_DESC_OFFSET_V1_8;

	/* firmware and build version */
	m->css.version.major_version = image->fw_ver_major;
	m->css.version.minor_version = image->fw_ver_minor;
	m->css.version.build_version = image->fw_ver_build;
	m->desc.header.major_version = image->fw_ver_major;
	m->desc.header.minor_version = image->fw_ver_minor;
	m->desc.header.hotfix_version = image->fw_ver_micro;
	m->desc.header.build_version = image->fw_ver_build;

	m->desc.header.feature_mask = 0x2; // -> should be feature mask - to fix
	m->desc.header.fw_image_flags = 0x2; // -> should be feature mask - to fix
	m->desc.header.fw_compat = 0x100000; // -> PUT PROPER STRUCT

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	ret = man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
	if (ret)
		goto err;

	/* platform config defines some modules except bringup & base modules */
	man_create_modules_in_config(image, desc);

	fprintf(stdout, "Firmware completing manifest v2.5\n");

	/* create structures from end of file to start of file */
	ri_adsp_meta_data_create_v2_5(image, MAN_META_EXT_OFFSET_ACE_V1_5,
			image->meu_offset);
	ri_plat_ext_data_create_ace_v1_5(image);
	ri_css_v2_5_hdr_create(image);
	ri_cse_create_ace_v1_5(image);

	fprintf(stdout, "Firmware file size 0x%x page count %d\n",
		FILE_TEXT_OFFSET_V1_8 - MAN_DESC_OFFSET_V1_8 + image->image_end,
		desc->header.preload_page_count);

	/* calculate hash for each module */
	man_hash_modules(image, desc);

	/* calculate hash inside ADSP meta data extension for padding to end */
	assert(image->meu_offset < image->image_end);
	ret = hash_sha384(image->fw_image + image->meu_offset, image->image_end - image->meu_offset,
			  m->adsp_file_ext.comp_desc[0].hash,
			  sizeof(m->adsp_file_ext.comp_desc[0].hash));
	if (ret)
		goto err;

	/* mue writes 0xff to 16 bytes of padding */
	memset(m->reserved, 0xff, 16);

	/* calculate hash inside ext info 16 of sof_man_adsp_meta_file_ext_v2_5 */
	assert((MAN_META_EXT_OFFSET_ACE_V1_5 + sizeof(struct sof_man_adsp_meta_file_ext_v2_5)) <
	       image->image_end);

	ret = hash_sha384(image->fw_image + MAN_META_EXT_OFFSET_ACE_V1_5,
			  sizeof(struct sof_man_adsp_meta_file_ext_v2_5),
			  m->signed_pkg.module[0].hash, sizeof(m->signed_pkg.module[0].hash));
	if (ret)
		goto err;

	/* hash values in reverse order */
	bytes_swap(m->signed_pkg.module[0].hash, sizeof(m->signed_pkg.module[0].hash));

	/* calculate hash - SHA384 on CAVS2_5+ */
	hash_sha384_init(&hash);
	hash_update(&hash, image->fw_image,
		    sizeof(struct CsePartitionDirHeader_v2_5) +
		    sizeof(struct CsePartitionDirEntry) * 3);

	hash_update(&hash, image->fw_image + 0x4c0, image->image_end - 0x4c0);
	hash_finalize(&hash);

	/* hash values in reverse order */
	ret = hash_get_digest(&hash, m->info_0x16.hash, sizeof(m->info_0x16.hash));
	if (ret < 0)
		goto err;
	bytes_swap(m->info_0x16.hash, sizeof(m->info_0x16.hash));

	/* sign manifest */
	ret = ri_manifest_sign_ace_v1_5(image);
	if (ret < 0)
		goto err;

	/* write the firmware */
	ret = man_write_fw_mod(image);
	if (ret < 0)
		goto err;

	/* write the unsigned files*/
	ret = man_write_unsigned_mod(image, MAN_META_EXT_OFFSET_ACE_V1_5,
				     MAN_FW_DESC_OFFSET_ACE_V1_5,
				     sizeof(struct sof_man_adsp_meta_file_ext_v2_5));
	if (ret < 0)
		goto err;

	fprintf(stdout, "Firmware manifest and signing completed !\n");
	return 0;

err:
	free(image->rom_image);
	free(image->fw_image);
	unlink(image->out_file);
	unlink(image->out_rom_file);
	return ret;
}

int verify_image(struct image *image)
{
	FILE *in_file;
	int ret;
	void *buffer;
	size_t size, read, i;

	/* is verify supported for target ? */
	if (!image->adsp->verify_firmware) {
		fprintf(stderr, "error: verify not supported for target\n");
		return -EINVAL;
	}

	/* open image for reading */
	in_file = fopen(image->verify_file, "rb");
	if (!in_file)
		return file_error("unable to open file for reading", image->verify_file);

	/* get file size */
	ret = get_file_size(in_file, image->verify_file, &size);
	if (ret < 0) {
		goto out;
	}

	/* allocate buffer for parsing */
	buffer = malloc(size);
	if (!buffer) {
		ret = -ENOMEM;
		goto out;
	}

	/* find start of fw image and verify */
	read = fread(buffer, 1, size, in_file);
	if (read != size) {
		ret = file_error("unable to read whole file", image->verify_file);
		goto out;
	}
	for (i = 0; i < size; i += sizeof(uint32_t)) {
		/* find CSE header marker "$CPD" */
		if (*(uint32_t *)(buffer + i) == CSE_HEADER_MAKER) {
			image->fw_image = buffer + i;
			ret = image->adsp->verify_firmware(image);
			goto out;
		}
	}

	/* no header found */
	fprintf(stderr, "error: could not find valid CSE header $CPD in %s\n",
		image->verify_file);
out:
	fclose(in_file);
	return 0;
}


int resign_image(struct image *image)
{
	int key_size, key_file_size;
	void *buffer = NULL;
	size_t size, read;
	FILE *in_file;
	int ret, i;

	/* open image for reading */
	in_file = fopen(image->in_file, "rb");
	if (!in_file)
		return file_error("unable to open file for reading", image->in_file);

	/* get file size */
	ret = get_file_size(in_file, image->in_file, &size);
	if (ret < 0) {
		goto out;
	}

	/* allocate buffer for parsing */
	buffer = malloc(size);
	if (!buffer) {
		ret = -ENOMEM;
		goto out;
	}

	/* read file into buffer */
	read = fread(buffer, 1, size, in_file);
	if (read != size) {
		ret = file_error("unable to read whole file", image->in_file);
		goto out;
	}

	fclose(in_file);

	for (i = 0; i < size; i += sizeof(uint32_t)) {
		/* find CSE header marker "$CPD" */
		if (*(uint32_t *)(buffer + i) == CSE_HEADER_MAKER) {
			image->fw_image = buffer + i;
			break;
		}
	}

	if (i >= size) {
		fprintf(stderr, "error: didn't found header marker %d\n", i);
		ret = -EINVAL;
		goto out;
	}

	image->image_end = size;

	/* check that key size matches */
	if (image->adsp->man_v2_5) {
		key_size = 384;
	} else {
		key_size = 256;
	}

	key_file_size = get_key_size(image);

	if (key_file_size > key_size) {
		fprintf(stderr, "error: key size %d is longer than original key %d\n",
			key_file_size, key_size);
		ret = -EINVAL;
		goto out;
	}

	/* resign */
	if (image->adsp->man_v1_5)
		ret = ri_manifest_sign_v1_5(image);
	else if (image->adsp->man_v1_8)
		ret = ri_manifest_sign_v1_8(image);
	else if (image->adsp->man_v2_5)
		ret = ri_manifest_sign_v2_5(image);
	else
		ret = -EINVAL;

	if (ret < 0) {
		fprintf(stderr, "error: unable to sign image\n");
		goto out;
	}

	/* open outfile for writing */
	unlink(image->out_file);
	image->out_fd = fopen(image->out_file, "wb");
	if (!image->out_fd) {
		ret = file_error("unable to open file for writting", image->out_file);
		goto out;
	}

	man_write_fw_mod(image);

out:
	free(buffer);
	return ret;
}
