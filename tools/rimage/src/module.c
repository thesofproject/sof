// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <rimage/module.h>
#include <rimage/elf_file.h>
#include <rimage/file_utils.h>
#include <rimage/rimage.h>


int module_read_section(const struct module *module, const struct module_section *section,
			void *buffer, const size_t size)
{
	return elf_section_read_content(&module->elf, section->header, buffer, size);
}

int module_write_section(const struct module *module, const struct module_section *section,
			 const int padding, FILE *out_file, const char *filename)
{
	int ret;
	struct elf_section section_data;
	size_t count;
	char padding_buf[4];

	ret = elf_section_read(&module->elf, section->header, &section_data);
	if (ret)
		return ret;

	/* write out section data */
	count = fwrite(section_data.data, section->size, 1, out_file);
	if (count != 1) {
		ret = file_error("cant write section", filename);
		goto out;
	}

	/* write padding data */
	if (padding) {
		assert(padding <= sizeof(padding_buf));

		memset(padding_buf, 0, padding);
		count = fwrite(padding_buf, padding, 1, out_file);
		if (count != 1) {
			ret = file_error("cant write padding", filename);
			goto out;
		}
	}

out:
	elf_section_free(&section_data);
	return ret;
}

int module_read_whole_elf(const struct module *module, void *buffer, size_t size)
{
	int ret;
	size_t count;

	if (module->elf.file_size > size) {
		fprintf(stderr, "error: Output buffer too small.\n");
		return -ENOSPC;
	}

	/* read in file data */
	ret = fseek(module->elf.file, 0, SEEK_SET);
	if (ret)
		return file_error("can't seek set", module->elf.filename);

	count = fread(buffer, module->elf.file_size, 1, module->elf.file);
	if (count != 1)
		return file_error("can't read data", module->elf.filename);

	return ret;
}

int module_write_whole_elf(const struct module *module, FILE *out_file, const char *filename)
{
	int ret;
	char *buffer;
	size_t count;

	/* alloc data data */
	buffer = calloc(1, module->elf.file_size);
	if (!buffer)
		return -ENOMEM;

	ret = module_read_whole_elf(module, buffer, module->elf.file_size);
	if (ret)
		goto out;

	/* write out section data */
	count = fwrite(buffer, module->elf.file_size, 1, out_file);
	if (count != 1) {
		ret = file_error("can't write data", "");// TODO: image->out_file);
		goto out;
	}

out:
	free(buffer);
	return ret;
}

void module_print_zones(const struct module *module)
{
	fprintf(stdout, "\n\tTotals\tStart\t\tEnd\t\tSize");

	fprintf(stdout, "\n\tTEXT\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->text.start, module->text.end,
		module->text.end - module->text.start);
	fprintf(stdout, "\tDATA\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->data.start, module->data.end,
		module->data.end - module->data.start);
	fprintf(stdout, "\tBSS\t0x%8.8x\t0x%8.8x\t0x%x\n\n",
		module->bss.start, module->bss.end,
		module->bss.end - module->bss.start);
}

/**
 * Print a list of valid program headers
 * 
 * @param module pointer to a module structure
 */
static void module_print_programs(const struct module *module)
{
	const Elf32_Phdr *header;
	int i;

	/* check each program */
	for (i = 0; i < module->elf.header.phnum; i++) {
		header = &module->elf.programs[i];

		if (header->filesz == 0 || header->type != PT_LOAD)
			continue;

		fprintf(stdout, "%s program-%d:\n", module->elf.filename, i);
		elf_program_header_print(header);
	}
}

/**
 * Goes through program headers array to find the physical address based on the virtual address.
 * 
 * @param elf elf file structure
 * @param vaddr virtual address
 * @return physical address when success, virtual address on error
 */
static uint32_t find_physical_address(struct elf_file *file, size_t vaddr)
{
	uint16_t i;
	const Elf32_Phdr *prog;

	for (i = 0; i < file->programs_count; i++) {
		prog = &file->programs[i];

		if (prog->type != PT_LOAD)
			continue;

		if (vaddr >= prog->vaddr && vaddr < (prog->vaddr + file->programs[i].memsz))
			return file->programs[i].paddr + vaddr - prog->vaddr;
	}

	return vaddr;
}

unsigned long uncache_to_cache(const struct memory_alias *alias, unsigned long address)
{
	return (address & ~alias->mask) | alias->cached;
}

/**
 * Checks if the section is placed in the rom memory address space
 * 
 * @param config Memory configuration structure
 * @param section section to be checked
 * @return true if section is placed in rom memory address space
 */
static bool section_is_rom(const struct memory_config *config,
			   const struct elf_section_header *section)
{
	uint32_t sect_start, sect_end;
	uint32_t rom_start, rom_end;

	sect_start = section->data.vaddr;
	sect_end = sect_start + section->data.size;

	rom_start = config->zones[SOF_FW_BLK_TYPE_ROM].base;
	rom_end = rom_start + config->zones[SOF_FW_BLK_TYPE_ROM].size;

	if (sect_end <= rom_start || sect_start >= rom_end)
		return false;
	if (sect_start >= rom_start && sect_end <= rom_end)
		return true;

	fprintf(stderr, "Warning! Section %s partially overlaps with rom memory.\n", section->name);
	return false;
}

/**
 * Initialize module_sections_info structure
 * 
 * @param info Pointer to a module_sections_info structure
 */
static void sections_info_init(struct module_sections_info *info)
{
	memset(info, 0, sizeof(*info));

	info->start = UINT32_MAX;
}

/**
 * Adds section to module_sections_info structure
 * 
 * @param info Pointer to a module_sections_info structure
 * @param address section address
 * @param size section size
 */
static void sections_info_add(struct module_sections_info *info, const uint32_t address,
			      const size_t size)
{
	const uint32_t end = address + size;

	if (address < info->start)
		info->start = address;

	if (end > info->end)
		info->end = end;

	info->size += size;
	info->count++;
}

/**
 * Calculates file size after adding all sections
 * 
 * @param info Pointer to a module_sections_info structure
 */
static void sections_info_finalize(struct module_sections_info *info)
{
	info->file_size = info->end - info->start;

	/* file sizes round up to nearest page */
	info->file_size = (info->file_size + MAN_PAGE_SIZE - 1) & ~(MAN_PAGE_SIZE - 1);
}

/**
 * Checks the section header (type and flags) to determine the section type.
 * 
 * @param section section header
 * @return enum module_section_type
 */
static enum module_section_type get_section_type(const struct elf_section_header *section)
{
	switch (section->data.type) {
	case SHT_INIT_ARRAY:
		/* fall through */
	case SHT_PROGBITS:
		/* text or data */
		return (section->data.flags & SHF_EXECINSTR) ? MST_TEXT : MST_DATA;

	case SHT_NOBITS:
		/* bss or heap */
		return MST_BSS;

	case SHT_NOTE:
		return MST_NOTE;

	default:
		return MST_UNKNOWN;
	}
}

void module_parse_sections(struct module *module, const struct memory_config *mem_cfg, bool verbose)
{
	const uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	uint16_t i;

	struct module_section *out_section = module->sections;

	fprintf(stdout, "  Found %d sections, listing valid sections...\n",
		module->elf.sections_count);

	fprintf(stdout, "\tNo\tLMA\t\tVMA\t\tEnd\t\tSize\tType\tName\n");

	/* parse each section */
	for (i = 0; i < module->elf.sections_count; i++) {
		const struct elf_section_header *sect = &module->elf.sections[i];
		struct module_sections_info *info = NULL;

		/* only write valid sections */
		if (!(sect->data.flags & valid))
			continue;

		/* Comment from fix_elf_addrs.py:
		 * The sof-derived linker scripts currently emit some zero-length sections
		 * at address zero. This is benign, and the linker is happy
		 *
		 * So we gleefully skip them. */
		if (sect->data.size == 0)
			continue;

		out_section->header = sect;
		out_section->size = sect->data.size;
		out_section->type = get_section_type(sect);
		out_section->rom = section_is_rom(mem_cfg, sect);
		out_section->address = sect->data.vaddr;
		out_section->load_address = find_physical_address(&module->elf, sect->data.vaddr);

		/* Don't convert ROM addresses, ROM sections aren't included in the output image */
		if (!out_section->rom) {
			/* Walk the sections in the ELF file, changing the VMA/LMA of each uncached section
			 * to the equivalent address in the cached area of memory. */
			out_section->address = uncache_to_cache(&mem_cfg->alias,
								out_section->address);
			out_section->load_address = uncache_to_cache(&mem_cfg->alias,
								     out_section->load_address);
		}

		fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%8.8zx\t0x%zx", i,
			out_section->load_address, out_section->address,
			out_section->address + out_section->size, out_section->size);


		switch (out_section->type) {
		case MST_DATA:
			info = &module->data;
			fprintf(stdout, "\tDATA");
			break;

		case MST_TEXT:
			info = &module->text;
			fprintf(stdout, "\tTEXT");
			break;

		case MST_BSS:
			info = &module->bss;
			fprintf(stdout, "\tBSS");
			break;

		case MST_NOTE:
			fprintf(stdout, "\tNOTE");
			break;

		default:
			break;
		}

		if (out_section->rom) {
			/* ROM sections aren't included in the output image */
			fprintf(stdout, " ROM");
		} else {
			/* Add section to list */
			if (info) {
				sections_info_add(info, out_section->load_address, out_section->size);
				out_section->next_section = info->first_section;
				info->first_section = out_section;
			}
		}

		module->num_sections++;
		out_section++;

		/* section name */
		fprintf(stdout, "\t%s\n", sect->name);

		if (verbose) {
			fprintf(stdout, "%s section-%d:\n", module->elf.filename, i);
			elf_section_header_print(sect);
		}
	}

	sections_info_finalize(&module->text);
	sections_info_finalize(&module->data);
	sections_info_finalize(&module->bss);

	size_t fw_size = module->data.size + module->text.size;

	fprintf(stdout, " module: input size %zd (0x%zx) bytes %d sections\n",
		fw_size, fw_size, module->num_sections);
	fprintf(stdout, " module: text %zu (0x%zx) bytes\n"
		"\tdata %zu (0x%zx) bytes\n"
		"\tbss  %zu (0x%zx) bytes\n\n",
		module->text.size, module->text.size,
		module->data.size, module->data.size,
		module->bss.size, module->bss.size);
}

int module_open(struct module *module, const char *filename, const bool verbose)
{
	int ret;

	memset(module, 0, sizeof(*module));

	ret = elf_open(&module->elf, filename);
	if (ret)
		return ret;

	if (verbose) {
		fprintf(stdout, "%s elf header:\n", module->elf.filename);
		elf_header_print(&module->elf);
		module_print_programs(module);
	}

	module->sections = calloc(module->elf.sections_count, sizeof(struct module_section));
	if (!module->sections) {
		elf_free(&module->elf);
		return -ENOMEM;
	}

	sections_info_init(&module->data);
	sections_info_init(&module->bss);
	sections_info_init(&module->text);

	return 0;
}

void module_close(struct module *module)
{
	elf_free(&module->elf);
}

/**
 * Checks if the contents of the section overlaps
 * 
 * @param a first section to check
 * @param b second section to check
 * @return true if space of a sections overlap
 */
static bool section_check_overlap(const struct module_section *a, const struct module_section *b)
{
	uint32_t a_start = a->address;
	uint32_t a_end = a_start + a->size;

	uint32_t b_start = b->address;
	uint32_t b_end = b_start + b->size;

	/* is section start overlapping ? */
	return (a_start >= b_start && a_start < b_end) ||
	/* is section end overlapping ? */
	       (a_end > b_start && a_end <= b_end);
}

/**
 * Checks if the contents of the modules overlaps
 * 
 * @param mod first module to check
 * @param mod2 second module to check
 * @return error code
 */
static int module_check_overlap(const struct module *mod, const struct module *mod2)
{
	unsigned int i, j;

	/* for each section from first module */
	for (i = 0; i < mod->num_sections; i++) {
		/* and for each section from second module */
		for (j = 0; j < mod2->num_sections; j++) {
			const struct module_section *section = &mod->sections[i];
			const struct module_section *section2 = &mod2->sections[j];

			/* don't compare section with itself */
			if (section == section2)
				continue;

			/* check section overlapping */
			if (section_check_overlap(section, section2)) {

				fprintf(stderr, "error: Detected overlapping sections:\n");
				fprintf(stderr, "\t[0x%x : 0x%zx] %s from %s\n", section->address,
					section->address + section->size - 1,
					section->header->name, mod->elf.filename);
				fprintf(stderr, "\t[0x%x : 0x%zx] %s from %s\n", section2->address,
					section2->address + section2->size - 1,
					section2->header->name, mod2->elf.filename);

				return -EINVAL;
			}
		}
	}

	return 0;
}

int modules_validate(const struct image *image)
{
	int i, j, ret;

	for (i = 0; i < image->num_modules; i++) {
		for (j = 0; j < image->num_modules; j++) {
			ret = module_check_overlap(&image->module[i].file, &image->module[j].file);
			if (ret)
				return ret;
		}
	}

	return 0;
}
