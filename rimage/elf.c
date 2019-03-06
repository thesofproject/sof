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
	int man_section_idx;

	/* read in section header */
	ret = fseek(module->fd, hdr->shoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to %s section header %d\n",
			module->elf_file, ret);
		return ret;
	}

	/* allocate space for each section header */
	section = calloc(sizeof(Elf32_Shdr), hdr->shnum);
	if (!section)
		return -ENOMEM;
	module->section = section;

	/* read in sections */
	count = fread(section, sizeof(Elf32_Shdr), hdr->shnum, module->fd);
	if (count != hdr->shnum) {
		fprintf(stderr, "error: failed to read %s section header %d\n",
			module->elf_file, -errno);
		return -errno;
	}

	/* read in strings */
	module->strings = calloc(1, section[hdr->shstrndx].size);
	if (!module->strings) {
		fprintf(stderr, "error: failed %s to read ELF strings for %d\n",
			module->elf_file, -errno);
			return -errno;
	}

	ret = fseek(module->fd, section[hdr->shstrndx].off, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to %s stringss %d\n",
			module->elf_file, ret);
		return ret;
	}

	count = fread(module->strings, 1, section[hdr->shstrndx].size,
		      module->fd);
	if (count != section[hdr->shstrndx].size) {
		fprintf(stderr, "error: failed to read %s strings %d\n",
			module->elf_file, -errno);
		return -errno;
	}

	/* find manifest module data */
	man_section_idx = elf_find_section(image, module, ".bss");
	if (man_section_idx < 0) {
		/* no bss - it is OK for boot_ldr */
		module->bss_start = 0;
		module->bss_end = 0;
	} else {
		module->bss_index = man_section_idx;
	}

	fprintf(stdout, " BSS module metadata section at index %d\n",
		man_section_idx);

	/* find log entries and fw ready sections */
	module->logs_index = elf_find_section(image, module,
					      ".static_log_entries");
	fprintf(stdout, " static log entries section at index %d\n",
		module->logs_index);
	module->fw_ready_index = elf_find_section(image, module,
						  ".fw_ready");
	fprintf(stdout, " fw ready section at index %d\n",
		module->fw_ready_index);

	/* parse each section */
	for (i = 0; i < hdr->shnum; i++) {
		/* only write valid sections */
		if (!(section[i].flags & valid))
			continue;

		switch (section[i].type) {
		case SHT_NOBITS:
			/* bss */
			module->bss_size += section[i].size;
			module->num_bss++;
			break;
		case SHT_PROGBITS:
			/* text or data */
			module->fw_size += section[i].size;

			if (section[i].flags & SHF_EXECINSTR)
				module->text_size += section[i].size;
			else
				module->data_size += section[i].size;
			break;
		default:
			continue;
		}

		module->num_sections++;

		if (!image->verbose)
			continue;

		fprintf(stdout, " %s section-%d: \ttype\t 0x%8.8x\n",
			module->elf_file, i, section[i].type);
		fprintf(stdout, " %s section-%d: \tflags\t 0x%8.8x\n",
			module->elf_file, i, section[i].flags);
		fprintf(stdout, " %s section-%d: \taddr\t 0x%8.8x\n",
			module->elf_file, i, section[i].vaddr);
		fprintf(stdout, " %s section-%d: \toffset\t 0x%8.8x\n",
			module->elf_file, i, section[i].off);
		fprintf(stdout, " %s section-%d: \tsize\t 0x%8.8x\n",
			module->elf_file, i, section[i].size);
		fprintf(stdout, " %s section-%d: \tlink\t 0x%8.8x\n",
			module->elf_file, i, section[i].link);
		fprintf(stdout, " %s section-%d: \tinfo\t 0x%8.8x\n\n",
			module->elf_file, i, section[i].info);
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
	ret = fseek(module->fd, hdr->phoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to %s program header %d\n",
			module->elf_file, ret);
		return ret;
	}

	/* allocate space for programs */
	prg = calloc(sizeof(Elf32_Phdr), hdr->phnum);
	if (!prg)
		return -ENOMEM;
	module->prg = prg;

	/* read in programs */
	count = fread(prg, sizeof(Elf32_Phdr), hdr->phnum, module->fd);
	if (count != hdr->phnum) {
		fprintf(stderr, "error: failed to read %s program header %d\n",
			module->elf_file, -errno);
		return -errno;
	}

	/* check each program */
	for (i = 0; i < hdr->phnum; i++) {
		if (prg[i].filesz == 0)
			continue;

		if (!image->verbose)
			continue;

		fprintf(stdout, "%s program-%d: \ttype\t 0x%8.8x\n",
			module->elf_file, i, prg[i].type);
		fprintf(stdout, "%s program-%d: \toffset\t 0x%8.8x\n",
			module->elf_file, i, prg[i].off);
		fprintf(stdout, "%s program-%d: \tvaddr\t 0x%8.8x\n",
			module->elf_file, i, prg[i].vaddr);
		fprintf(stdout, "%s program-%d: \tpaddr\t 0x%8.8x\n",
			module->elf_file, i, prg[i].paddr);
		fprintf(stdout, "%s program-%d: \tfsize\t 0x%8.8x\n",
			module->elf_file, i, prg[i].filesz);
		fprintf(stdout, "%s program-%d: \tmsize\t 0x%8.8x\n",
			module->elf_file, i, prg[i].memsz);
		fprintf(stdout, "%s program-%d: \tflags\t 0x%8.8x\n\n",
			module->elf_file, i, prg[i].flags);
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
		module->elf_file, hdr->entry);
	fprintf(stdout, "%s elf: \tprogram offset\t 0x%8.8x\n",
		module->elf_file, hdr->phoff);
	fprintf(stdout, "%s elf: \tsection offset\t 0x%8.8x\n",
		module->elf_file, hdr->shoff);
	fprintf(stdout, "%s elf: \tprogram size\t 0x%8.8x\n",
		module->elf_file, hdr->phentsize);
	fprintf(stdout, "%s elf: \tprogram count\t 0x%8.8x\n",
		module->elf_file, hdr->phnum);
	fprintf(stdout, "%s elf: \tsection size\t 0x%8.8x\n",
		module->elf_file, hdr->shentsize);
	fprintf(stdout, "%s elf: \tsection count\t 0x%8.8x\n",
		module->elf_file, hdr->shnum);
	fprintf(stdout, "%s elf: \tstring index\t 0x%8.8x\n\n",
		module->elf_file, hdr->shstrndx);

	return 0;
}

int elf_is_rom(struct image *image, Elf32_Shdr *section)
{
	uint32_t start, end;
	uint32_t base, size;

	start = section->vaddr;
	end = section->vaddr + section->size;

	base = image->adsp->mem_zones[SOF_FW_BLK_TYPE_ROM].base;
	size = image->adsp->mem_zones[SOF_FW_BLK_TYPE_ROM].size;

	if (start < base || start > base + size)
		return 0;
	if (end < base || end > base + size)
		return 0;
	return 1;
}

static void elf_module_size(struct image *image, struct module *module,
			    Elf32_Shdr *section, int index)
{
	switch (section->type) {
	case SHT_PROGBITS:
		/* text or data */
		if (section->flags & SHF_EXECINSTR) {
			/* text */
			if (module->text_start > section->vaddr)
				module->text_start = section->vaddr;
			if (module->text_end < section->vaddr + section->size)
				module->text_end = section->vaddr +
					section->size;

			fprintf(stdout, "\tTEXT\t");
		} else {
			/* initialized data, also calc the writable sections */
			if (module->data_start > section->vaddr)
				module->data_start = section->vaddr;
			if (module->data_end < section->vaddr + section->size)
				module->data_end = section->vaddr +
					section->size;

			fprintf(stdout, "\tDATA\t");
		}
		break;
	case SHT_NOBITS:
		/* bss */
		if (index == module->bss_index) {
			/* updated the .bss segment */
			module->bss_start = section->vaddr;
			module->bss_end = section->vaddr + section->size;
			fprintf(stdout, "\tBSS\t");
		} else {
			fprintf(stdout, "\tHEAP\t");
		}
		break;
	case SHT_NOTE:
		fprintf(stdout, "\tNOTE\t");
		break;
	default:
		break;
	}
}

static void elf_module_size_reloc(struct image *image, struct module *module,
				  Elf32_Shdr *section, int index)
{
	switch (section->type) {
	case SHT_PROGBITS:
		/* text or data */
		if (section->flags & SHF_EXECINSTR) {
			/* text */
			module->text_start = 0;
			module->text_end += section->size;

			fprintf(stdout, "\tTEXT\t");
		} else {
			/* initialized data, also calc the writable sections */
			module->data_start = 0;
			module->data_end += section->size;

			fprintf(stdout, "\tDATA\t");
		}
		break;
	case SHT_NOBITS:
		/* bss */
		if (index == module->bss_index) {
			/* updated the .bss segment */
			module->bss_start = section->vaddr;
			module->bss_end = section->vaddr + section->size;
			fprintf(stdout, "\tBSS\t");
		} else {
			fprintf(stdout, "\tHEAP\t");
		}
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

	module->text_start = module->data_start = 0xffffffff;
	module->bss_start = 0;
	module->text_end = module->data_end = module->bss_end = 0;

	fprintf(stdout, "  Found %d sections, listing valid sections......\n",
		module->hdr.shnum);

	fprintf(stdout, "\tNo\tStart\t\tEnd\t\tSize\tType\tName\n");

	/* iterate all sections and get size of segments */
	for (i = 0; i < module->hdr.shnum; i++) {
		section = &module->section[i];

		/* module bss can sometimes be missed */
		if (i != module->bss_index && i != module->logs_index &&
		    i != module->fw_ready_index) {
			/* only check valid sections */
			if (!(section->flags & valid))
				continue;

			if (section->size == 0)
				continue;

			if (elf_is_rom(image, section))
				continue;
		}

		fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%x", i,
			section->vaddr, section->vaddr + section->size,
			section->size);

		/* text or data section */
		if (image->reloc)
			elf_module_size_reloc(image, module, section, i);
		else
			elf_module_size(image, module, section, i);

		/* section name */
		fprintf(stdout, "%s\n", module->strings + section->name);
	}

	fprintf(stdout, "\n");
}

/* make sure no section overlap from any modules */
int elf_validate_section(struct image *image, struct module *module,
			 Elf32_Shdr *section, int index)
{
	struct module *m;
	Elf32_Shdr *s;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i, j;

	/* for each module */
	for (i = 0; i < image->num_modules; i++) {
		m = &image->module[i];

		/* for each section */
		for (j = 0; j < m->hdr.shnum; j++) {
			s = &m->section[j];

			if (s == section)
				continue;

			/* only check valid sections */
			if (!(s->flags & valid))
				continue;

			if (s->size == 0)
				continue;

			/* is section start non overlapping ? */
			if (section->vaddr >= s->vaddr &&
			    section->vaddr <
			    s->vaddr + s->size) {
				goto err;
			}

			/* is section end non overlapping ? */
			if (section->vaddr + section->size > s->vaddr &&
			    section->vaddr + section->size <=
			    s->vaddr + s->size) {
				goto err;
			}
		}
	}

	return 0;

err:
	fprintf(stderr, "error: section overlap between %s:%d and %s:%d\n",
		module->elf_file, index, m->elf_file, j);
	fprintf(stderr, "     [0x%x : 0x%x] overlaps with [0x%x :0x%x]\n",
		section->vaddr, section->vaddr + section->size,
		s->vaddr, s->vaddr + s->size);
	return -EINVAL;
}

/* make sure no section overlaps from any modules */
int elf_validate_modules(struct image *image)
{
	struct module *module;
	Elf32_Shdr *section;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i, j, ret;

	/* relocatable modules have no physical addresses until runtime */
	if (image->reloc)
		return 0;

	/* for each module */
	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];

		/* for each section */
		for (j = 0; j < module->hdr.shnum; j++) {
			section = &module->section[j];

			/* only check valid sections */
			if (!(section->flags & valid))
				continue;

			if (section->size == 0)
				continue;

			/* is section non overlapping ? */
			ret = elf_validate_section(image, module, section, j);
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

	section = &module->section[hdr->shstrndx];

	/* alloc data data */
	buffer = calloc(1, section->size);
	if (!buffer)
		return -ENOMEM;

	/* read in section string data */
	ret = fseek(module->fd, section->off, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to string section %d\n", ret);
		goto out;
	}

	count = fread(buffer, 1, section->size, module->fd);
	if (count != section->size) {
		fprintf(stderr, "error: can't read string section %d\n",
			-errno);
		ret = -errno;
		goto out;
	}

	/* find section with name */
	for (i = 0; i < hdr->shnum; i++) {
		s = &module->section[i];
		if (!strcmp(name, buffer + s->name)) {
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
	module->fd = fopen(name, "rb");
	if (!module->fd) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
			name, errno);
		return -EINVAL;
	}
	module->elf_file = name;

	/* get file size */
	ret = fseek(module->fd, 0, SEEK_END);
	if (ret < 0)
		goto hdr_err;
	module->file_size = ftell(module->fd);
	ret = fseek(module->fd, 0, SEEK_SET);
	if (ret < 0)
		goto hdr_err;

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
	free(module->strings);
	fclose(module->fd);
}
