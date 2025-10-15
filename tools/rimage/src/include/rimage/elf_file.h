/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 * 
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __ELF_FILE_H__
#define __ELF_FILE_H__

#include <stddef.h>

#include "elf.h"

struct elf_section_header {
	Elf32_Shdr data;
	char *name;
};

struct elf_section {
	struct elf_section_header header;
	void *data;
};

struct elf_strings {
	struct elf_section section;
};

struct elf_file {
	FILE *file;
	char *filename;
	size_t file_size;
	Elf32_Ehdr header;
	struct elf_section_header *sections;
	Elf32_Phdr *programs;
	uint16_t sections_count;
	uint16_t programs_count;
};

/**
 * Open elf file
 * 
 * @param [out]elf elf file structure
 * @param [in]filename File name to open
 * @return error code, 0 when success
 */
int elf_open(struct elf_file *elf, const char *filename);

/**
 * Close elf file and release resources
 * 
 * @param elf elf file structure
 */
void elf_free(struct elf_file *elf);

/**
 * Print elf file header
 * 
 * @param elf elf file structure
 */
void elf_header_print(const struct elf_file *elf);

/**
 * Print program headers
 * 
 * @param elf elf file structure
 */
void elf_print_programs(const struct elf_file *elf);

/**
 * Print single program header
 * 
 * @param header program header structure
 */
void elf_program_header_print(const Elf32_Phdr *header);

/**
 * Print elf sections headers
 * 
 * @param elf elf file structure
 */
void elf_print_sections(const struct elf_file *elf);

/**
 * Return section header by index
 * 
 * @param [in]elf elf file structure
 * @param [in]index section index
 * @param [out]header section header data
 * @return error code, 0 when success
 */
int elf_section_header_get_by_index(const struct elf_file *elf, int index,
				    const struct elf_section_header **header);

/**
 * Return section header by index
 * 
 * @param [in]elf elf file structure
 * @param [in]index section index
 * @param [out]header section header data
 * @return error code, 0 when success
 */
int elf_section_header_get_by_name(const struct elf_file *elf, const char* name,
				   const struct elf_section_header **header);
/**
 * Print elf section header
 * 
 * @param header section header structure
 */
void elf_section_header_print(const struct elf_section_header *header);

/**
 * Read elf section using given header
 * 
 * @param [in]elf elf file structure
 * @param [in]header section header
 * @param [out]section section data
 * @return error code, 0 when success
 */
int elf_section_read(const struct elf_file *elf, const struct elf_section_header *header,
		     struct elf_section *section);

/**
* Read elf section using given header to specified buffer
* 
* @param [in]elf elf file structure
* @param [in]header section header
* @param [out]buffer buffer for a section data
* @param [in]size buffer size
* @return error code, 0 when success
*/
int elf_section_read_content(const struct elf_file *elf, const struct elf_section_header *header,
			     void *buffer, const size_t size);

/**
 * Read elf section with given name
 * 
 * @param [in]elf elf file structure
 * @param [in]name section name
 * @param [out]section section data
 * @return error code, 0 when success
 */
int elf_section_read_by_name(const struct elf_file *elf, const char *name, struct elf_section *section);

/**
 * Release section
 * 
 * @param [in]section section structure
 */
void elf_section_free(struct elf_section *section);

/**
 * Read elf strings section with given index
 * 
 * @param [in]elf elf file structure
 * @param [in]index strings section index
 * @param [out]strings strings section data
 * @return error code, 0 when success
 */
int elf_strings_read_by_index(const struct elf_file *elf, int index, struct elf_strings *strings);

/**
 * Get string value. Allocate new copy using strdup.
 * 
 * @param [in]strings strings section structure
 * @param [in]index string index
 * @param [out]str Pointer to the variable in which the pointer to the allocated string will be placed
 * @return error code, 0 when success
 */
int elf_strings_get(const struct elf_strings *strings, int index, char **str);

/**
 * Release string section
 * @param [in]strings strings section structure
 */
void elf_strings_free(struct elf_strings *strings);

#endif /* __ELF_FILE_H__ */
