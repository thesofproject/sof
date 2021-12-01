// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
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

static int man_open_rom_file(struct image *image)
{
	uint32_t size;

	sprintf(image->out_rom_file, "%s.rom", image->out_file);
	unlink(image->out_rom_file);

	size = image->adsp->mem_zones[SOF_FW_BLK_TYPE_ROM].size;

	/* allocate ROM image  */
	image->rom_image = calloc(size, 1);
	if (!image->rom_image)
		return -ENOMEM;

	/* open ROM outfile for writing */
	image->out_rom_fd = fopen(image->out_rom_file, "wb");
	if (!image->out_rom_fd) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image->out_rom_file, errno);
		return -errno;
	}

	return 0;
}

static int man_open_unsigned_file(struct image *image)
{
	sprintf(image->out_unsigned_file, "%s.uns", image->out_file);
	unlink(image->out_unsigned_file);

	/* open unsigned FW outfile for writing */
	image->out_unsigned_fd = fopen(image->out_unsigned_file, "wb");
	if (!image->out_unsigned_fd) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image->out_unsigned_file, errno);
		return -errno;
	}

	return 0;
}

static int man_open_manifest_file(struct image *image)
{
	/* open manifest outfile for writing */
	sprintf(image->out_man_file, "%s.met", image->out_file);
	unlink(image->out_man_file);

	image->out_man_fd = fopen(image->out_man_file, "wb");
	if (!image->out_man_fd) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image->out_man_file, errno);
		return -errno;
	}

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

/* we should call this after all segments size set up via iterate */
static uint32_t elf_to_file_offset(struct image *image,
				   struct module *module,
				   struct sof_man_module *man_module,
				   Elf32_Shdr *section)
{
	uint32_t elf_addr = section->vaddr, file_offset = 0, i;

	if (section->type == SHT_PROGBITS || section->type == SHT_INIT_ARRAY) {
		/* check programs for lma/vma change */
		for (i = 0; i < module->hdr.phnum; i++) {
			if (section->vaddr == module->prg[i].vaddr) {
				elf_addr = module->prg[i].paddr;
				break;
			}
		}
		if (section->flags & SHF_EXECINSTR) {
			/* text segment */
			file_offset = elf_addr - module->text_start +
				module->foffset;
		} else {
			/* rodata segment, append to text segment */
			file_offset = elf_addr - module->data_start +
				module->foffset + module->text_fixup_size;
		}
	} else if (section->type == SHT_NOBITS) {
		/* bss segment */
		file_offset = 0;
	}

	return file_offset;
}

/* write SRAM sections */
static int man_copy_sram(struct image *image, Elf32_Shdr *section,
			 struct module *module,
			 struct sof_man_module *man_module,
			 int section_idx)
{
	uint32_t offset = elf_to_file_offset(image, module,
		man_module, section);
	uint32_t end = offset + section->size;
	int seg_type = -1;
	void *buffer = image->fw_image + offset;
	size_t count;

	assert((uint64_t)offset + section->size <= image->adsp->image_size);

	switch (section->type) {
	case SHT_INIT_ARRAY:
		/* fall through */
	case SHT_PROGBITS:
		/* text or data */
		if (section->flags & SHF_EXECINSTR)
			seg_type = SOF_MAN_SEGMENT_TEXT;
		else
			seg_type = SOF_MAN_SEGMENT_RODATA;
		break;
	case SHT_NOBITS:
		seg_type = SOF_MAN_SEGMENT_BSS;
		/* FALLTHRU */
	default:
		return 0;
	}

	/* file_offset for segment should not be 0s, we set it to
	 * the smallest offset of its modules ATM.
	 */
	if (man_module->segment[seg_type].file_offset > offset ||
	    man_module->segment[seg_type].file_offset == 0)
		man_module->segment[seg_type].file_offset = offset;

	count = fread(buffer, 1, section->size, module->fd);
	if (count != section->size) {
		fprintf(stderr, "error: cant read section %d\n", -errno);
		return -errno;
	}

	/* get module end offset  ? */
	if (end > image->image_end)
		image->image_end = end;

	fprintf(stdout, "\t%d\t0x%x\t0x%x\t\t0x%x\t%s\n", section_idx,
		section->vaddr, section->size, offset,
		seg_type == SOF_MAN_SEGMENT_TEXT ? "TEXT" : "DATA");

	return 0;
}

static int man_copy_elf_section(struct image *image, Elf32_Shdr *section,
				struct module *module,
				struct sof_man_module *man_module, int idx)
{
	int ret;

	/* seek to ELF section */
	ret = fseek(module->fd, section->off, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to section %d\n", ret);
		return ret;
	}

	/* write data to DRAM or ROM image */
	if (!elf_is_rom(image, section))
		return man_copy_sram(image, section, module, man_module, idx);

	return 0;
}

static int man_get_module_manifest(struct image *image, struct module *module,
				   struct sof_man_module *man_module)
{
	Elf32_Shdr *section;
	struct sof_man_segment_desc *segment;
	struct sof_man_module_manifest sof_mod;
	size_t count;
	int ret, man_section_idx;

	fprintf(stdout, "Module Write: %s\n", module->elf_file);

	/* find manifest module data */
	man_section_idx = elf_find_section(module, ".module");
	if (man_section_idx < 0)
		return -EINVAL;

	fprintf(stdout, " Manifest module metadata section at index %d\n",
		man_section_idx);
	section = &module->section[man_section_idx];

	/* load in manifest data */
	ret = fseek(module->fd, section->off, SEEK_SET);

	if (ret < 0) {
		fprintf(stderr, "error: can't seek to section %d\n", ret);
		return ret;
	}

	count = fread(&sof_mod, 1, sizeof(sof_mod), module->fd);
	if (count != sizeof(sof_mod)) {
		fprintf(stderr, "error: can't read section %d\n", -errno);
		return -errno;
	}

	/* configure man_module with sofmod data */
	memcpy(man_module->struct_id, "$AME", 4);
	man_module->entry_point = sof_mod.module.entry_point;
	memcpy(man_module->name, sof_mod.module.name, SOF_MAN_MOD_NAME_LEN);
	memcpy(man_module->uuid, sof_mod.module.uuid, 16);
	man_module->affinity_mask = sof_mod.module.affinity_mask;
	man_module->type.auto_start = sof_mod.module.type.auto_start;
	man_module->type.domain_dp = sof_mod.module.type.domain_dp;
	man_module->type.domain_ll = sof_mod.module.type.domain_ll;
	man_module->type.load_type = sof_mod.module.type.load_type;

	/* read out text_fixup_size from memory mapping */
	module->text_fixup_size = sof_mod.text_size;

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

	return 0;
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

static int man_module_create(struct image *image, struct module *module,
			     struct sof_man_module *man_module)
{
	/* create module and segments */
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	Elf32_Shdr *section;
	int i, err;
	unsigned int pages;

	image->image_end = 0;

	err = man_get_module_manifest(image, module, man_module);
	if (err < 0)
		return err;

	/* stack size ??? convert sizes to PAGES */
	man_module->instance_bss_size = 1;

	/* max number of instances of this module ?? */
	man_module->instance_max_count = 1;

	fprintf(stdout, "\n\tTotals\tStart\t\tEnd\t\tSize");

	fprintf(stdout, "\n\tTEXT\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->text_start, module->text_end,
		module->text_end - module->text_start);
	fprintf(stdout, "\tDATA\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->data_start, module->data_end,
		module->data_end - module->data_start);
	fprintf(stdout, "\tBSS\t0x%8.8x\t0x%8.8x\t0x%x\n\n ",
		module->bss_start, module->bss_end,
		module->bss_end - module->bss_start);

	/* main module */
	/* text section is first */
	man_module->segment[SOF_MAN_SEGMENT_TEXT].file_offset =
		module->foffset;
	man_module->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr =
		module->text_start;

	/* calculates those padding 0s by the start of next segment */
	pages = module->text_file_size / MAN_PAGE_SIZE;
	if (module->text_file_size % MAN_PAGE_SIZE)
		pages += 1;

	if (module->text_fixup_size == 0)
		module->text_fixup_size = module->text_file_size;

	/* check if text_file_size is bigger then text_fixup_size */
	if (module->text_file_size > module->text_fixup_size) {
		fprintf(stderr, "error: too small text size assigned!\n");
		return -EINVAL;
	}

	man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length = pages;

	/* data section */
	man_module->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr =
		module->data_start;
	man_module->segment[SOF_MAN_SEGMENT_RODATA].file_offset =
			module->foffset + module->text_fixup_size;
	pages = module->data_file_size / MAN_PAGE_SIZE;
	if (module->data_file_size % MAN_PAGE_SIZE)
		pages += 1;

	man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length = pages;

	/* bss is last */
	man_module->segment[SOF_MAN_SEGMENT_BSS].file_offset = 0;
	man_module->segment[SOF_MAN_SEGMENT_BSS].v_base_addr =
			module->bss_start;
	pages = (module->bss_end - module->bss_start) / MAN_PAGE_SIZE;
	if ((module->bss_end - module->bss_start) % MAN_PAGE_SIZE)
		pages += 1;
	man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.length = pages;
	if (pages == 0) {
		man_module->segment[SOF_MAN_SEGMENT_BSS].flags.ul = 0;
		man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.type =
				SOF_MAN_SEGMENT_EMPTY;
	}

	fprintf(stdout, "\tNo\tAddress\t\tSize\t\tFile\tType\n");

	if (man_module_validate(man_module) < 0)
		return -EINVAL;

	/* find all sections and copy to corresponding segments */
	for (i = 0; i < module->hdr.shnum; i++) {
		section = &module->section[i];

		/* only check valid sections */
		if (!(section->flags & valid))
			continue;

		if (section->size == 0)
			continue;

		/* text or data section */
		if (!elf_is_rom(image, section))
			err = man_copy_elf_section(image, section, module,
						   man_module, i);

		if (err < 0) {
			fprintf(stderr, "error: failed to write section #%d\n",
				i);
			return err;
		}
	}
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

static int man_module_create_reloc(struct image *image, struct module *module,
				   struct sof_man_module *man_module)
{
	/* create module and segments */
	int err;
	unsigned int pages;
	void *buffer = image->fw_image + module->foffset;
	size_t count;

	image->image_end = 0;

	err = man_get_module_manifest(image, module, man_module);
	if (err < 0)
		return err;

	/* stack size ??? convert sizes to PAGES */
	man_module->instance_bss_size = 1;

	/* max number of instances of this module ?? */
	man_module->instance_max_count = 1;

	fprintf(stdout, "\n\tTotals\tStart\t\tEnd\t\tSize");

	fprintf(stdout, "\n\tTEXT\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->text_start, module->text_end,
		module->text_end - module->text_start);
	fprintf(stdout, "\tDATA\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->data_start, module->data_end,
		module->data_end - module->data_start);
	fprintf(stdout, "\tBSS\t0x%8.8x\t0x%8.8x\t0x%x\n\n ",
		module->bss_start, module->bss_end,
		module->bss_end - module->bss_start);

	/* main module */
	/* text section is first */
	man_module->segment[SOF_MAN_SEGMENT_TEXT].file_offset =
		module->foffset;
	man_module->segment[SOF_MAN_SEGMENT_TEXT].v_base_addr = 0;
	man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length = 0;

	/* data section */
	man_module->segment[SOF_MAN_SEGMENT_RODATA].v_base_addr = 0;
	man_module->segment[SOF_MAN_SEGMENT_RODATA].file_offset =
			module->foffset;
	pages = module->data_file_size / MAN_PAGE_SIZE;
	if (module->data_file_size % MAN_PAGE_SIZE)
		pages += 1;

	man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length = pages;

	/* bss is last */
	man_module->segment[SOF_MAN_SEGMENT_BSS].file_offset = 0;
	man_module->segment[SOF_MAN_SEGMENT_BSS].v_base_addr = 0;
	man_module->segment[SOF_MAN_SEGMENT_BSS].flags.r.length = 0;

	fprintf(stdout, "\tNo\tAddress\t\tSize\t\tFile\tType\n");

	/* seek to beginning of file */
	err = fseek(module->fd, 0, SEEK_SET);
	if (err < 0) {
		fprintf(stderr, "error: can't seek to section %d\n", err);
		return err;
	}

	count = fread(buffer, 1, module->file_size, module->fd);
	if (count != module->file_size) {
		fprintf(stderr, "error: can't read section %d\n", -errno);
		return -errno;
	}

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%x\t%s\n", 0,
		0, module->file_size, 0, "DATA");

	fprintf(stdout, "\n");
	image->image_end = module->foffset + module->file_size;

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
	if (count != 1) {
		fprintf(stderr, "error: failed to write meta %s %d\n",
			image->out_man_file, -errno);
		return -errno;
	}
	fclose(image->out_man_fd);

	/* now prepare the unsigned rimage */
	count = fwrite(image->fw_image + meta_end_offset,
		       image->image_end - meta_end_offset,
		       1, image->out_unsigned_fd);

	/* did the unsigned FW write succeed ? */
	if (count != 1) {
		fprintf(stderr, "error: failed to write firmware %s %d\n",
			image->out_unsigned_file, -errno);
		return -errno;
	}
	fclose(image->out_unsigned_fd);

	return 0;
}

static int man_write_fw_mod(struct image *image)
{
	int count;

	/* write manifest and signed image */
	count = fwrite(image->fw_image, image->image_end, 1, image->out_fd);

	/* did the image write succeed ? */
	if (count != 1) {
		fprintf(stderr, "error: failed to write signed firmware %s %d\n",
			image->out_file, -errno);
		return -errno;
	}

	return 0;
}

static int man_create_modules(struct image *image, struct sof_man_fw_desc *desc,
			      int file_text_offset)
{
	struct module *module;
	struct sof_man_module *man_module;
	int err;
	int i = 0, offset = 0;

	/* if first module is executable then write before manifest */
	if (image->adsp->exec_boot_ldr) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(0);
		module = &image->module[0];

		fprintf(stdout, "Module: %s used as executable header\n",
			module->elf_file);
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

	/*skip bringup and base module */
	for (i = 2; i < modules->mod_man_count; i++) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(i);
		memcpy(man_module, &modules->mod_man[i], sizeof(*man_module));
	}

	cfg_start = (void *)desc + SOF_MAN_MODULE_OFFSET(i);
	memcpy(cfg_start, modules->mod_cfg, modules->mod_cfg_count * sizeof(struct sof_man_mod_config));

	desc->header.num_module_entries = modules->mod_man_count;
}

static int man_hash_modules(struct image *image, struct sof_man_fw_desc *desc)
{
	struct sof_man_module *man_module;
	int i;

	for (i = 0; i < image->num_modules; i++) {
		man_module = (void *)desc + SOF_MAN_MODULE_OFFSET(i);

		if (image->adsp->exec_boot_ldr && i == 0) {
			fprintf(stdout, " module: no need to hash %s\n as its exec header\n",
				man_module->name);
			continue;
		}

		ri_sha256(image,
			 man_module->segment[SOF_MAN_SEGMENT_TEXT].file_offset,
			 (man_module->segment[SOF_MAN_SEGMENT_TEXT].flags.r.length +
			 man_module->segment[SOF_MAN_SEGMENT_RODATA].flags.r.length) *
			 MAN_PAGE_SIZE, man_module->hash);
	}

	return 0;
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
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_5);

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
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module - subtract the boot loader exec header */
	m->desc.header.num_module_entries = image->num_modules - 1;
	man_create_modules(image, &m->desc, FILE_TEXT_OFFSET_V1_5_SUE);
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
	uint8_t hash[SOF_MAN_MOD_SHA256_LEN];
	int ret, i;

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
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);

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
	ri_sha256(image, MAN_FW_DESC_OFFSET_V1_8, image->image_end
		  - MAN_FW_DESC_OFFSET_V1_8,
		  m->adsp_file_ext.comp_desc[0].hash);

	/* calculate hash for platform auth data - repeated in hash 2 and 4 */
	ri_sha256(image, MAN_META_EXT_OFFSET_V1_8,
		  sizeof(struct sof_man_adsp_meta_file_ext_v1_8), hash);

	/* hash values in reverse order */
	for (i = 0; i < SOF_MAN_MOD_SHA256_LEN; i++) {
		m->signed_pkg.module[0].hash[i] =
		m->partition_info.module[0].hash[i] =
			hash[SOF_MAN_MOD_SHA256_LEN - 1 - i];
	}

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
	desc->header.build_version = image->fw_ver_build;

	/* create each module */
	desc->header.num_module_entries = image->num_modules;
	man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_5);

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
	ri_sha256(image, image->meu_offset, image->image_end -
		image->meu_offset, meta->comp_desc[0].hash);

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
	desc->header.build_version = image->fw_ver_build;

	/* create each module */
	desc->header.num_module_entries = image->num_modules;
	man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);

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
	ri_sha256(image, image->meu_offset, image->image_end -
		image->meu_offset, meta->comp_desc[0].hash);

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
	desc->header.build_version = image->fw_ver_build;

	/* create each module */
	desc->header.num_module_entries = image->num_modules;
	man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
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
	ri_sha384(image, image->meu_offset, image->image_end -
		  image->meu_offset, meta->comp_desc[0].hash);

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
	uint8_t hash[SOF_MAN_MOD_SHA384_LEN];
	int ret, i;

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
	m->desc.header.build_version = image->fw_ver_build;

	/* create each module */
	m->desc.header.num_module_entries = image->num_modules;
	man_create_modules(image, desc, FILE_TEXT_OFFSET_V1_8);
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
	ri_sha384(image, image->meu_offset, image->image_end - image->meu_offset,
		  m->adsp_file_ext.comp_desc[0].hash);

	/* mue writes 0xff to 16 bytes of padding */
	for (i = 0; i < 16; i++)
		m->reserved[i] = 0xff;

	/* calculate hash inside ext info 16 of sof_man_adsp_meta_file_ext_v2_5 */
	ri_sha384(image, MAN_META_EXT_OFFSET_V2_5,
		  sizeof(struct sof_man_adsp_meta_file_ext_v2_5), hash);

	/* hash values in reverse order */
	for (i = 0; i < SOF_MAN_MOD_SHA384_LEN; i++) {
		m->signed_pkg.module[0].hash[i] =
			hash[SOF_MAN_MOD_SHA384_LEN - 1 - i];
	}

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

int verify_image(struct image *image)
{
	FILE *in_file;
	int ret, i;
	long size;
	void *buffer;
	size_t read;

	/* is verify supported for target ? */
	if (!image->adsp->verify_firmware) {
		fprintf(stderr, "error: verify not supported for target\n");
		return -EINVAL;
	}

	/* open image for reading */
	in_file = fopen(image->verify_file, "rb");
	if (!in_file) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
			image->verify_file, errno);
		return -errno;
	}

	/* get file size */
	ret = fseek(in_file, 0, SEEK_END);
	if (ret < 0) {
		fprintf(stderr, "error: unable to seek eof %s for reading %d\n",
			image->verify_file, errno);
		ret = -errno;
		goto out;
	}
	size = ftell(in_file);
	if (size < 0) {
		fprintf(stderr, "error: unable to get file size for %s %d\n",
			image->verify_file, errno);
		ret = -errno;
		goto out;
	}
	ret = fseek(in_file, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: unable to seek %s for reading %d\n",
			image->verify_file, errno);
		ret = -errno;
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
		fprintf(stderr, "error: unable to read %ld bytes from %s err %d\n",
					size, image->verify_file, errno);
		ret = errno;
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
	if (!in_file) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
			image->in_file, errno);
		return -errno;
	}

	/* get file size */
	ret = fseek(in_file, 0, SEEK_END);
	if (ret < 0) {
		fprintf(stderr, "error: unable to seek eof %s for reading %d\n",
			image->verify_file, errno);
		ret = -errno;
		goto out;
	}

	size = ftell(in_file);
	if (size < 0) {
		fprintf(stderr, "error: unable to get file size for %s %d\n",
			image->verify_file, errno);
		ret = -errno;
		goto out;
	}

	ret = fseek(in_file, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: unable to seek %s for reading %d\n",
			image->verify_file, errno);
		ret = -errno;
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
		fprintf(stderr, "error: unable to read %ld bytes from %s err %d\n",
					size, image->in_file, errno);
		ret = errno;
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
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image->out_file, errno);
		ret = -EINVAL;
		goto out;
	}

	man_write_fw_mod(image);

out:
	free(buffer);
	return ret;
}
