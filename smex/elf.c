/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf_defs.h"
#include "elf.h"

static int elf_read_sections(struct elf_module *module, bool verbose)
{
	Elf32_Ehdr *hdr = &module->hdr;
	Elf32_Shdr *section;
	size_t count;
	int i, ret;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);

	/* read in section header */
	ret = fseek(module->fd, hdr->shoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to %s section header %d\n",
			module->elf_file, ret);
		return -errno;
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
		return count < 0 ? -errno : -ENODATA;
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
		return -errno;
	}

	count = fread(module->strings, 1, section[hdr->shstrndx].size,
		      module->fd);
	if (count != section[hdr->shstrndx].size) {
		fprintf(stderr, "error: failed to read %s strings %d\n",
			module->elf_file, -errno);
		return count < 0 ? -errno : -ENODATA;
	}

	module->bss_index = elf_find_section(module, ".bss");
	if (module->bss_index < 0) {
		fprintf(stderr, "Can't find .bss section in %s",
		module->elf_file);
		return -EINVAL;
	}

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

		if (!verbose)
			continue;

		fprintf(stdout, " %s section-%d: \tname\t %s\n",
			module->elf_file, i, module->strings + section[i].name);
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

static int elf_read_programs(struct elf_module *module, bool verbose)
{
	Elf32_Ehdr *hdr = &module->hdr;
	Elf32_Phdr *prg;
	size_t count;
	int i, ret;

	/* read in program header */
	ret = fseek(module->fd, hdr->phoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to %s program header %d\n",
			module->elf_file, ret);
		return -errno;
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
		return count < 0 ? -errno : -ENODATA;
	}

	/* check each program */
	for (i = 0; i < hdr->phnum; i++) {
		if (prg[i].filesz == 0)
			continue;

		if (!verbose)
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

static int elf_read_hdr(struct elf_module *module, bool verbose)
{
	Elf32_Ehdr *hdr = &module->hdr;
	size_t count;

	/* read in elf header */
	count = fread(hdr, sizeof(*hdr), 1, module->fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to read %s elf header %d\n",
			module->elf_file, -errno);
		return count < 0 ? -errno : -ENODATA;
	}

	if (!verbose)
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

static void elf_module_size(struct elf_module *module, Elf32_Shdr *section,
			    int index)
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

static void elf_module_limits(struct elf_module *module)
{
	Elf32_Shdr *section;
	int i;

	module->text_start = 0xffffffff;
	module->data_start = 0xffffffff;
	module->bss_start = 0;
	module->text_end = 0;
	module->data_end = 0;
	module->bss_end = 0;

	fprintf(stdout, "  Found %d sections, listing valid sections......\n",
		module->hdr.shnum);

	fprintf(stdout, "\tNo\tStart\t\tEnd\t\tSize\tType\tName\n");

	/* iterate all sections and get size of segments */
	for (i = 0; i < module->hdr.shnum; i++) {
		section = &module->section[i];

		/* module bss can sometimes be missed */
		if (i != module->bss_index) {
			if (section->vaddr == 0)
				continue;

			if (section->size == 0)
				continue;
		}

		fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%x", i,
			section->vaddr, section->vaddr + section->size,
			section->size);

		/* text or data section */
		elf_module_size(module, section, i);

		/* section name */
		fprintf(stdout, "%s\n", module->strings + section->name);
	}

	fprintf(stdout, "\n");
}

/* make sure no section overlap */
static int elf_validate_section(struct elf_module *module,
				Elf32_Shdr *section, int index)
{
	Elf32_Shdr *s;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i;

	/* for each section */
	for (i = 0; i < module->hdr.shnum; i++) {
		s = &module->section[i];

		if (s == section)
			continue;

		/* only check valid sections */
		if (!(s->flags & valid))
			continue;

		if (s->size == 0)
			continue;

		/* is section start non overlapping ? */
		if (section->vaddr >= s->vaddr &&
		    section->vaddr < s->vaddr + s->size) {
			goto err;
		}

		/* is section end non overlapping ? */
		if (section->vaddr + section->size > s->vaddr &&
		    section->vaddr + section->size <= s->vaddr + s->size) {
			goto err;
		}
	}

	return 0;

err:
	fprintf(stderr, "error: section overlap between %s:%d and %s:%d\n",
		module->elf_file, index, module->elf_file, i);
	fprintf(stderr, "     [0x%x : 0x%x] overlaps with [0x%x :0x%x]\n",
		section->vaddr, section->vaddr + section->size,
		s->vaddr, s->vaddr + s->size);
	return -EINVAL;
}

/* make sure no section overlaps from any modules */
static int elf_validate_module(struct elf_module *module)
{
	Elf32_Shdr *section;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int i, ret;

	/* for each section */
	for (i = 0; i < module->hdr.shnum; i++) {
		section = &module->section[i];

		/* only check valid sections */
		if (!(section->flags & valid))
			continue;

		if (section->size == 0)
			continue;

		/* is section non overlapping ? */
		ret = elf_validate_section(module, section, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int elf_find_section(const struct elf_module *module, const char *name)
{
	const Elf32_Ehdr *hdr = &module->hdr;
	const Elf32_Shdr *section, *s;
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
		ret = -errno;
		goto out;
	}

	count = fread(buffer, 1, section->size, module->fd);
	if (count != section->size) {
		fprintf(stderr, "error: can't read string section %d\n",
			-errno);
		ret = count < 0 ? -errno : -ENODATA;
		goto out;
	}
	buffer[section->size - 1] = '\0';

	/* find section with name */
	for (i = 0; i < hdr->shnum; i++) {
		s = &module->section[i];
		if (s->name >= section->size) {
			fprintf(stderr, "error: invalid section name string index %d\n", s->name);
			ret = -EINVAL;
			goto out;
		}

		if (!strcmp(name, buffer + s->name)) {
			ret = i;
			goto out;
		}
	}

	fprintf(stderr, "warning: can't find section %s in module %s\n", name,
		module->elf_file);
	ret = -EINVAL;

out:
	free(buffer);
	return ret;
}

int elf_read_section(const struct elf_module *module, const char *section_name,
		     const Elf32_Shdr **dst_section, void **dst_buff)
{
	const Elf32_Shdr *section;
	int section_index;
	int ret;

	section_index = elf_find_section(module, section_name);
	if (section_index < 0) {
		fprintf(stderr, "error: section %s can't be found\n",
			section_name);
		return -EINVAL;
	}

	section = &module->section[section_index];
	if (dst_section)
		*dst_section = section;

	/* alloc buffer for section content */
	*dst_buff = calloc(1, section->size);
	if (!*dst_buff)
		return -ENOMEM;

	/* fill buffer with section content */
	ret = fseek(module->fd, section->off, SEEK_SET);
	if (ret) {
		fprintf(stderr, "error: can't seek to %s section %d\n", section_name, -errno);
		ret = -errno;
		goto error;
	}

	ret = fread(*dst_buff, 1, section->size, module->fd);
	if (ret != section->size) {
		fprintf(stderr, "error: can't read %s section %d\n", section_name, -errno);
		ret = ret < 0 ? -errno : -ENODATA;
		goto error;
	}

	return section->size;

error:
	free(*dst_buff);
	return ret;
}

int elf_read_module(struct elf_module *module, const char *name, bool verbose)
{
	int ret = 0;

	/* open the elf input file */
	module->fd = fopen(name, "rb");
	if (!module->fd) {
		fprintf(stderr, "error: unable to open %s for reading: %s\n",
			name, strerror(errno));
		return -EINVAL;
	}
	module->elf_file = name;

	/* get file size */
	ret = fseek(module->fd, 0, SEEK_END);
	if (ret < 0) {
		ret = -errno;
		goto hdr_err;
	}
	module->file_size = ftell(module->fd);
	ret = fseek(module->fd, 0, SEEK_SET);
	if (ret < 0) {
		ret = -errno;
		goto hdr_err;
	}

	/* read in elf header */
	ret = elf_read_hdr(module, verbose);
	if (ret < 0)
		goto hdr_err;

	/* read in programs */
	ret = elf_read_programs(module, verbose);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read program sections %d\n",
			ret);
		goto hdr_err;
	}

	/* read sections */
	ret = elf_read_sections(module, verbose);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read base sections %d\n",
			ret);
		goto sec_err;
	}

	elf_module_limits(module);
	elf_find_section(module, "");

	fprintf(stdout, " module: input size %d (0x%x) bytes %d sections\n",
		module->fw_size, module->fw_size, module->num_sections);
	fprintf(stdout, " module: text %d (0x%x) bytes\n"
			"    data %d (0x%x) bytes\n"
			"    bss  %d (0x%x) bytes\n\n",
		module->text_size, module->text_size,
		module->data_size, module->data_size,
		module->bss_size, module->bss_size);

	elf_validate_module(module);

	return 0;

sec_err:
	free(module->prg);
hdr_err:
	fclose(module->fd);

	return ret;
}

void elf_free_module(struct elf_module *module)
{
	free(module->prg);
	free(module->section);
	free(module->strings);
	if (module->fd)
		fclose(module->fd);
}
