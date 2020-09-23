// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rimage/rimage.h>
#include <rimage/cse.h>
#include <rimage/manifest.h>

static int elf_read_sections(struct image *image, struct module *module,
			     int module_index)
{
	Elf32_Ehdr *hdr = &module->hdr;
	Elf32_Shdr *section = module->section;
	size_t count;
	int i, ret;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);

	/* read in section header */
	ret = fseek(module->fd, hdr->shoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to %s section header %d\n",
			module->elf_file, ret);
		return ret;
	}

	/* allocate space for each section header */
	section = calloc(hdr->shnum, sizeof(Elf32_Shdr));
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
	assert(hdr->shstrndx < count);
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

	if (image->num_modules > 1 && module_index == 0) {
		/* In case of multiple modules first one should be bootloader,
		 * that should not have these sections.
		 */
		fprintf(stdout, "info: ignore .bss"
			" section for bootloader module\n");

		module->bss_start = 0;
		module->bss_end = 0;
	} else {
		/* find manifest module data */
		module->bss_index = elf_find_section(module, ".bss");
		if (module->bss_index < 0)
			return module->bss_index;
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
		case SHT_INIT_ARRAY:
			/* fall through */
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

	if (strncmp((char *)hdr->ident, "\177ELF\001\001", 5)) {
		fprintf(stderr, "Not a 32 bits ELF-LE file\n");
		return -EINVAL;
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
			    Elf32_Shdr *section, uint32_t lma, int index)
{
	switch (section->type) {
	case SHT_INIT_ARRAY:
		/* fall through */
	case SHT_PROGBITS:
		/* text or data */
		if (section->flags & SHF_EXECINSTR) {
			/* text */
			if (module->text_start > lma)
				module->text_start = lma;
			if (module->text_end < lma + section->size)
				module->text_end = lma + section->size;
			fprintf(stdout, "\tTEXT\t");
		} else {
			/* initialized data, also calc the writable sections */
			if (module->data_start > lma)
				module->data_start = lma;
			if (module->data_end < lma + section->size)
				module->data_end = lma + section->size;

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
	uint32_t section_lma;
	int i, j;

	module->text_start = 0xffffffff;
	module->data_start = 0xffffffff;
	module->bss_start = 0;
	module->text_end = 0;
	module->data_end = 0;
	module->bss_end = 0;

	fprintf(stdout, "  Found %d sections, listing valid sections......\n",
		module->hdr.shnum);

	fprintf(stdout, "\tNo\tLMA\t\tVMA\t\tEnd\t\tSize\tType\tName\n");

	/* iterate all sections and get size of segments */
	for (i = 0; i < module->hdr.shnum; i++) {
		section = &module->section[i];

		/* module bss can sometimes be missed */
		if (i != module->bss_index) {
			/* only check valid sections */
			if (!(section->flags & valid))
				continue;

			if (section->size == 0)
				continue;

			if (elf_is_rom(image, section))
				continue;
		}
		/* check programs to get LMA */
		section_lma = section->vaddr;
		for (j = 0; j < module->hdr.phnum; j++) {
			if (section->vaddr == module->prg[j].vaddr) {
				section_lma = module->prg[j].paddr;
				break;
			}
		}

		fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%8.8x\t0x%x", i,
			section_lma, section->vaddr,
			section->vaddr + section->size, section->size);

		/* text or data section */
		if (image->reloc)
			elf_module_size_reloc(image, module, section, i);
		else
			elf_module_size(image, module, section, section_lma, i);

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

int elf_find_section(const struct module *module, const char *name)
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

	fprintf(stderr, "warning: can't find section named '%s' in module %s\n",
		name, module->elf_file);
	ret = -EINVAL;

out:
	free(buffer);
	return ret;
}

int elf_read_section(const struct module *module, const char *section_name,
		     const Elf32_Shdr **dst_section, void **dst_buff)
{
	const Elf32_Shdr *section;
	int section_index = -1;
	int read;

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
	fseek(module->fd, section->off, SEEK_SET);
	read = fread(*dst_buff, 1, section->size, module->fd);
	if (read != section->size) {
		fprintf(stderr,
			"error: can't read %s section %d\n", section_name,
			-errno);
		free(*dst_buff);
		return -errno;
	}

	return section->size;
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
	ret = elf_read_sections(image, module, module_index);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read base sections %d\n",
			ret);
		goto sec_err;
	}

	/* check limits */
	elf_module_limits(image, module);

	elf_find_section(module, "");

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
