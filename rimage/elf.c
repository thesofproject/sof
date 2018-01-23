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
#include <string.h>
#include "rimage.h"
#include "cse.h"
#include "manifest.h"

static int elf_read_sections(struct image *image, struct module *module)
{
	Elf32_Ehdr *hdr = &module->hdr;
	Elf32_Shdr *section = module->section;
	size_t count;
	int i, ret;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);

	/* read in section header */
	ret = fseek(module->fd, hdr->e_shoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to %s section header %d\n",
			module->elf_file, ret);
		return ret;
	}

	/* allocate space for each section header */
	section = calloc(sizeof(Elf32_Shdr), hdr->e_shnum);
	if (section == NULL)
		return -ENOMEM;
	module->section = section;

	/* read in sections */
	count = fread(section, sizeof(Elf32_Shdr), hdr->e_shnum, module->fd);
	if (count != hdr->e_shnum) {
		fprintf(stderr, "error: failed to read %s section header %d\n",
			module->elf_file, -errno);
		return -errno;
	}

	/* parse each section */
	for (i = 0; i < hdr->e_shnum; i++) {

		/* only write valid sections */
		if (!(section[i].sh_flags & valid))
			continue;

		switch (section[i].sh_type) {
		case SHT_NOBITS:
			/* bss */
			module->bss_size += section[i].sh_size;
			module->num_bss++;
			break;
		case SHT_PROGBITS:
			/* text or data */
			module->fw_size += section[i].sh_size;

			if (section[i].sh_flags & SHF_EXECINSTR)
				module->text_size += section[i].sh_size;
			else
				module->data_size += section[i].sh_size;
			break;
		default:
			continue;
		}

		module->num_sections++;

		if (!image->verbose)
			continue;

		fprintf(stdout, " %s section-%d: \ttype\t 0x%8.8x\n", module->elf_file,
			i, section[i].sh_type);
		fprintf(stdout, " %s section-%d: \tflags\t 0x%8.8x\n", module->elf_file,
			i, section[i].sh_flags);
		fprintf(stdout, " %s section-%d: \taddr\t 0x%8.8x\n", module->elf_file,
			i, section[i].sh_addr);
		fprintf(stdout, " %s section-%d: \toffset\t 0x%8.8x\n", module->elf_file,
			i, section[i].sh_offset);
		fprintf(stdout, " %s section-%d: \tsize\t 0x%8.8x\n", module->elf_file,
			i, section[i].sh_size);
		fprintf(stdout, " %s section-%d: \tlink\t 0x%8.8x\n", module->elf_file,
			i, section[i].sh_link);
		fprintf(stdout, " %s section-%d: \tinfo\t 0x%8.8x\n\n", module->elf_file,
			i, section[i].sh_info);
	}

	return 0;
}

static int elf_read_programs(struct image *image, struct module *module)
{
	Elf32_Ehdr *hdr = &module->hdr;
	Elf32_Phdr *prg = module->prg;
	size_t count;
	int i, ret;

	/* read in program header */
	ret = fseek(module->fd, hdr->e_phoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to %s program header %d\n",
			module->elf_file ,ret);
		return ret;
	}

	/* allocate space for programs */
	prg = calloc(sizeof(Elf32_Phdr), hdr->e_phnum);
	if (prg == NULL)
		return -ENOMEM;
	module->prg = prg;

	/* read in programs */
	count = fread(prg, sizeof(Elf32_Phdr), hdr->e_phnum, module->fd);
	if (count != hdr->e_phnum) {
		fprintf(stderr, "error: failed to read %s program header %d\n",
			module->elf_file, -errno);
		return -errno;
	}

	/* check each program */
	for (i = 0; i < hdr->e_phnum; i++) {

		if (prg[i].p_filesz == 0)
			continue;

		if (!image->verbose)
			continue;

		fprintf(stdout, "%s program-%d: \ttype\t 0x%8.8x\n",
			module->elf_file, i, prg[i].p_type);
		fprintf(stdout, "%s program-%d: \toffset\t 0x%8.8x\n",
			module->elf_file, i, prg[i].p_offset);
		fprintf(stdout, "%s program-%d: \tvaddr\t 0x%8.8x\n",
			module->elf_file, i, prg[i].p_vaddr);
		fprintf(stdout, "%s program-%d: \tpaddr\t 0x%8.8x\n",
			module->elf_file, i, prg[i].p_paddr);
		fprintf(stdout, "%s program-%d: \tfsize\t 0x%8.8x\n",
			module->elf_file, i, prg[i].p_filesz);
		fprintf(stdout, "%s program-%d: \tmsize\t 0x%8.8x\n",
			module->elf_file, i, prg[i].p_memsz);
		fprintf(stdout, "%s program-%d: \tflags\t 0x%8.8x\n\n",
			module->elf_file, i, prg[i].p_flags);
	}

	return 0;
}

static int elf_read_hdr(struct image *image, struct module *module)
{
	Elf32_Ehdr *hdr = &module->hdr;
	size_t count;

	/* read in elf header */
	count = fread(hdr, sizeof(*hdr), 1, module->fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to read %s elf header %d\n",
			module->elf_file, -errno);
		return -errno;
	}

	if (!image->verbose)
		return 0;

	fprintf(stdout, "%s elf: \tentry point\t 0x%8.8x\n",
		module->elf_file, hdr->e_entry);
	fprintf(stdout, "%s elf: \tprogram offset\t 0x%8.8x\n",
		module->elf_file, hdr->e_phoff);
	fprintf(stdout, "%s elf: \tsection offset\t 0x%8.8x\n",
		module->elf_file, hdr->e_shoff);
	fprintf(stdout, "%s elf: \tprogram size\t 0x%8.8x\n",
		module->elf_file, hdr->e_phentsize);
	fprintf(stdout, "%s elf: \tprogram count\t 0x%8.8x\n",
		module->elf_file, hdr->e_phnum);
	fprintf(stdout, "%s elf: \tsection size\t 0x%8.8x\n",
		module->elf_file, hdr->e_shentsize);
	fprintf(stdout, "%s elf: \tsection count\t 0x%8.8x\n",
		module->elf_file, hdr->e_shnum);
	fprintf(stdout, "%s elf: \tstring index\t 0x%8.8x\n\n",
		module->elf_file, hdr->e_shstrndx);

	return 0;
}

int elf_is_rom(struct image *image, Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	uint32_t start, end;

	start = section->sh_addr;
	end = section->sh_addr + section->sh_size;

	if (start < image->adsp->rom_base ||
		start > image->adsp->rom_base + image->adsp->rom_size)
		return 0;
	if (end < image->adsp->rom_base ||
		end > image->adsp->rom_base + image->adsp->rom_size)
		return 0;
	return 1;
}

static void elf_module_size(struct image *image, struct module *module,
	Elf32_Shdr *section)
{
	switch (section->sh_type) {
	case SHT_PROGBITS:
		/* text or data */
		if (section->sh_flags & SHF_EXECINSTR) {
			/* text */
			if (module->text_start > section->sh_addr)
				module->text_start = section->sh_addr;
			if (module->text_end < section->sh_addr + section->sh_size)
				module->text_end = section->sh_addr + section->sh_size;

			fprintf(stdout, "\tTEXT\n");
		} else {
			/* initialized data, also calc the writable sections */
			if (module->data_start > section->sh_addr)
				module->data_start = section->sh_addr;
			if (module->data_end < section->sh_addr + section->sh_size)
				module->data_end = section->sh_addr + section->sh_size;

			fprintf(stdout, "\tDATA\n");
		}
		break;
	case SHT_NOBITS:
		/* bss */
		if (module->bss_start > section->sh_addr)
			module->bss_start = section->sh_addr;
		if (module->bss_end < section->sh_addr + section->sh_size)
			module->bss_end = section->sh_addr + section->sh_size;

		fprintf(stdout, "\tBSS\n");
		break;
	default:
		break;
	}
}

static void elf_module_limits(struct image *image, struct module *module)
{
	Elf32_Shdr *section;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i;

	module->text_start = module->data_start = module->bss_start = 0xffffffff;
	module->text_end = module->data_end = module->bss_end = 0;

	fprintf(stdout, "  Found %d sections, listing valid sections......\n",
		module->hdr.e_shnum);

	fprintf(stdout, "\tNo\tStart\t\tEnd\t\tBytes\tType\n");

	/* iterate all sections and get size of segments */
	for (i = 0; i < module->hdr.e_shnum; i++) {

		section = &module->section[i];

		/* only check valid sections */
		if (!(section->sh_flags & valid))
			continue;

		if (section->sh_size == 0)
			continue;

		if (elf_is_rom(image, section))
			continue;

		fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t%d", i,
			section->sh_addr, section->sh_addr + section->sh_size,
			section->sh_size);

		/* text or data section */
		elf_module_size(image, module, section);
	}

	fprintf(stdout, "\n");
}

/* make sure no section overlap from any modules */
int elf_validate_section(struct image *image, struct module *module,
	Elf32_Shdr *section)
{
	struct module *m;
	Elf32_Shdr *s;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i, j, ret;

	/* for each module */
	for (i = 0; i < image->num_modules; i++) {
		m = &image->module[i];

		if (m == module)
			continue;

		/* for each section */
		for (j = 0; j < m->hdr.e_shnum; j++) {
			s = &m->section[j];

			if (s == section)
				continue;

			/* only check valid sections */
			if (!(section->sh_flags & valid))
				continue;

			if (section->sh_size == 0)
				continue;

			/* is section non overlapping ? */
			if (section->sh_addr >= s->sh_addr &&
				section->sh_addr + section->sh_size <=
				s->sh_addr + s->sh_size) {
				goto err;
			}
		}
	}

	return 0;

err:
	fprintf(stderr, "error: section overlap between %s and %s\n",
		module->elf_file, m->elf_file);
	fprintf(stderr, "     [0x%x : 0x%x] overlaps with [0x%x :0x%x]\n",
		section->sh_addr, section->sh_addr + section->sh_size,
		s->sh_addr, s->sh_addr + s->sh_size);
	return -EINVAL;
}

/* make sure no section overlaps from any modules */
int elf_validate_modules(struct image *image)
{
	struct module *module;
	Elf32_Shdr *section;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i, j, ret;

	/* for each module */
	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];

		/* for each section */
		for (j = 0; j < module->hdr.e_shnum; j++) {
			section = &module->section[j];

			/* only check valid sections */
			if (!(section->sh_flags & valid))
				continue;

			if (section->sh_size == 0)
				continue;

			/* is section non overlapping ? */
			ret = elf_validate_section(image, module, section);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int elf_find_section(struct image *image, struct module *module,
		const char *name)
{
	Elf32_Ehdr *hdr = &module->hdr;
	Elf32_Shdr *section, *s;
	char *buffer;
	size_t count;
	int ret, i;

	section = &module->section[hdr->e_shstrndx];

	/* alloc data data */
	buffer = calloc(1, section->sh_size);
	if (buffer == NULL)
		return -ENOMEM;

	/* read in section string data */
	ret = fseek(module->fd, section->sh_offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to string section %d\n", ret);
		goto out;
	}

	count = fread(buffer, 1, section->sh_size, module->fd);
	if (count != section->sh_size) {
		fprintf(stderr, "error: can't read string section %d\n", -errno);
		ret = -errno;
		goto out;
	}

	/* find section with name */
	for (i = 0; i < hdr->e_shnum; i++) {
		s = &module->section[i];
		if (!strcmp(name, buffer + s->sh_name)) {
			ret = i;
			goto out;
		}
	}

	fprintf(stderr, "error: can't find section %s in module %s\n", name,
		module->elf_file);
	ret = -EINVAL;

out:
	free(buffer);
	return ret;
}

int elf_parse_module(struct image *image, int module_index, const char *name)
{
	const struct adsp *adsp = image->adsp;
	struct module *module;
	uint32_t rem;
	int ret = 0;

	/* validate module index */
	if (module_index >= MAX_MODULES) {
		fprintf(stderr, "error: too any modules\n");
		return -EINVAL;
	}

	module = &image->module[module_index];

	/* open the elf input file */
	module->fd = fopen(name, "r");
	if (module->fd == NULL) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
				name, errno);
		return -EINVAL;
	}
	module->elf_file = name;

	/* read in elf header */
	ret = elf_read_hdr(image, module);
	if (ret < 0)
		goto hdr_err;

	/* read in programs */
	ret = elf_read_programs(image, module);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read program sections %d\n",
			ret);
		goto hdr_err;
	}

	/* read sections */
	ret = elf_read_sections(image, module);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read base sections %d\n",
			ret);
		goto sec_err;
	}

	/* check limits */
	elf_module_limits(image, module);

	elf_find_section(image, module, "");

	fprintf(stdout, " module: input size %d (0x%x) bytes %d sections\n",
		module->fw_size, module->fw_size, module->num_sections);
	fprintf(stdout, " module: text %d (0x%x) bytes\n"
			"    data %d (0x%x) bytes\n"
			"    bss  %d (0x%x) bytes\n\n",
		module->text_size, module->text_size,
		module->data_size, module->data_size,
		module->bss_size, module->bss_size);

	/* file sizes round up to nearest page */
	module->text_file_size = module->text_end - module->text_start;
	rem = module->text_file_size % MAN_PAGE_SIZE;
	if (rem)
		module->text_file_size += MAN_PAGE_SIZE - rem;


	/* apply any base FW fixups */
	if (image->adsp->base_fw_text_size_fixup &&
		module->text_start == image->adsp->sram_base) {
		module->text_file_size += image->adsp->base_fw_text_size_fixup;
	}

	/* data section */
	module->data_file_size = module->data_end - module->data_start;
	rem = module->data_file_size % MAN_PAGE_SIZE;
		if (rem)
			module->data_file_size += MAN_PAGE_SIZE - rem;

	/* bss section */
	module->bss_file_size = module->bss_end - module->bss_start;
	rem = module->bss_file_size % MAN_PAGE_SIZE;
		if (rem)
			module->bss_file_size += MAN_PAGE_SIZE - rem;

	return 0;

sec_err:
	free(module->prg);
hdr_err:
	fclose(module->fd);

	return ret;
}

void elf_free_module(struct image *image, int module_index)
{
	struct module *module = &image->module[module_index];

	free(module->prg);
	free(module->section);
	fclose(module->fd);
}
