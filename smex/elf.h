// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#ifndef __INCLUDE_ELF_H__
#define __INCLUDE_ELF_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "elf_defs.h"

/*
 * ELF module data
 */
struct elf_module {
	const char *elf_file;
	FILE *fd;

	Elf32_Ehdr hdr;
	Elf32_Shdr *section;
	Elf32_Phdr *prg;
	char *strings;

	uint32_t text_start;
	uint32_t text_end;
	uint32_t data_start;
	uint32_t data_end;
	uint32_t bss_start;
	uint32_t bss_end;
	uint32_t foffset;

	int num_sections;
	int num_bss;
	int fw_size;
	int bss_index;

	/* sizes do not include any gaps */
	int bss_size;
	int text_size;
	int data_size;

	/* sizes do include gaps to nearest page */
	int bss_file_size;
	int text_file_size;
	int text_fixup_size;
	int data_file_size;

	/* total file size */
	int file_size;

	/* executable header module */
	int exec_header;
};

int elf_read_module(struct elf_module *module, const char *name, bool verbose);
void elf_free_module(struct elf_module *module);
int elf_find_section(const struct elf_module *module, const char *name);
int elf_read_section(const struct elf_module *module, const char *section_name,
		     const Elf32_Shdr **dst_section, void **dst_buff);

#endif /* __INCLUDE_ELF_H__ */
