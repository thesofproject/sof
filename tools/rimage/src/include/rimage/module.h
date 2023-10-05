/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 * 
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_H__
#define __MODULE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <rimage/elf_file.h>

enum module_section_type {
	MST_UNKNOWN, MST_DATA, MST_TEXT, MST_BSS, MST_NOTE
};

struct module_section {
	const struct elf_section_header *header;
	enum module_section_type type;

	/* The contents of the section lie in the rom memory space */
	bool rom;

	/* ADSP devices have their RAM regions mapped twice. The first mapping is set in the CPU
	 * to bypass the L1 cache, and so access through pointers in that region is coherent between
	 * CPUs (but slow). The second region accesses the same memory through the L1 cache and
	 * requires careful flushing when used with shared data.
	 *
	 * This distinction is exposed in the linker script, where some symbols (e.g. stack regions)
	 * are linked into cached memory, but others (general kernel memory) are not.
	 * 
	 * Addresses of sections belonging to a rom memory are not converted. */
	 
	/* section virtual address, converted to cached address space */
	uint32_t address;

	/* section physical load address, converted to cached address space */
	uint32_t load_address;

	size_t size;

	/* next section of this type */
	struct module_section *next_section;
};

struct module_sections_info {
	/* start address */
	uint32_t start;

	/* end address */
	uint32_t end;

	/* size without any gaps */
	size_t size;

	/* size include gap to nearest page */
	size_t file_size;

	/* sections count */
	unsigned int count;

	/* First section */
	struct module_section *first_section;
};

/*
 * ELF module data
 */
struct module {
	struct elf_file elf;

	/* Array of valid sections */
	struct module_section *sections;

	/* Number of valid sections */
	unsigned int num_sections;

	struct module_sections_info text;
	struct module_sections_info data;
	struct module_sections_info bss;
};

struct image;
struct memory_alias;
struct memory_config;

/**
 * Convert uncached memory address to cached
 * 
 * @param alias alias memory configration
 * @param address address to be converted
 * @return cached address
 */
unsigned long uncache_to_cache(const struct memory_alias *alias, unsigned long address);


/**
 * Load module file
 * 
 * @param module module structure
 * @param filename module file name
 * @param verbose verbose logging selection
 * @return error code
 */
int module_open(struct module *module, const char *filename, const bool verbose);

/**
 * Unloads module
 * 
 * @param module module structure
 */
void module_close(struct module *module);

/**
 * Parse module sections
 * 
 * @param module module structure
 * @param mem_cfg memory configration structure
 * @param verbose verbose logging selection
 * @return error code
 */
void module_parse_sections(struct module *module, const struct memory_config *mem_cfg,
			   bool verbose);

/**
 * Read module section to memory buffer
 * 
 * @param [in]module module structure
 * @param [in]section module section structure
 * @param [out]buffer destination buffer
 * @param [in]size destination buffer size
 * @return error code
 */
int module_read_section(const struct module *module, const struct module_section *section,
			void *buffer, const size_t size);

/**
 * Read module section and write it to a file
 * 
 * @param module module structure
 * @param section module section structure
 * @param padding count of padding bytes to write after section content
 * @param out_file destination file handle
 * @param filename output file name used to print error message
 * @return error code
 */
int module_write_section(const struct module *module, const struct module_section *section,
			 const int padding, FILE *out_file, const char *filename);

/**
 * Read whole module elf file to a memory buffer
 * 
 * @param [in]module module structure
 * @param [out]buffer destination buffer
 * @param [in]size destination buffer size
 * @return error code
 */
int module_read_whole_elf(const struct module *module, void *buffer, size_t size);

/**
 * Read whore module elf file and write it to a file
 * 
 * @param module module structure
 * @param out_file destination file handle
 * @param filename output file name used to print error message
 * @return error code
 */
int module_write_whole_elf(const struct module *module, FILE *out_file, const char *filename);

/**
 * Displays information about the occupancy of each memory zone
 * 
 * @param module module structure
 */
void module_print_zones(const struct module *module);

/**
 * Checks all modules to make sure their sections do not overlap with each other
 * 
 * @param module module structure
 * @param image program global structure
 * @return error code
 */
int modules_validate(const struct image *image);

#endif /* __MODULE_H__ */
