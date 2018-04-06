/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_SOF_MODULE__
#define __INCLUDE_SOF_MODULE__

#include <stdint.h>
#include <sof/sof.h>
#include <sof/list.h>
#include <arch/reloc.h>
#include <sof/ipc/topology.h>
#include <rimage/sof/user/manifest.h>

#define MODULE_MAGIC	"SOF_MOD"
#define MODULE_ABI	{0, 0}

/* ELF 32bit file header */
struct elf32_file_hdr {
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint32_t e_entry;
	uint32_t e_phoff;
	uint32_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
} __packed;

/* section types - sh_type */
#define SHT_PROGBITS		1
#define SHT_SYMTAB		2
#define SHT_STRTAB		3
#define SHT_RELA		4
#define SHT_NOBITS		8

/* section flags - sh_flags */
#define SHF_WRITE		(1 << 0)
#define SHF_ALLOC		(1 << 1)
#define SHF_EXECINSTR		(1 << 2)

/* ELF 32bit section header */
struct elf32_section_hdr {
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
} __packed;

/* relocation info data - r_info */
#define ELF32_R_SYM(val)		((val) >> 8)
#define ELF32_R_TYPE(val)		((val) & 0xff)

/* relocation types - r_info */
#define R_XTENSA_NONE           0
#define R_XTENSA_32          1
#define R_XTENSA_PLT            6
#define R_XTENSA_ASM_EXPAND	11
#define R_XTENSA_32_PCREL	14
#define R_XTENSA_DIFF8		17
#define R_XTENSA_DIFF16		18
#define R_XTENSA_DIFF32		19
#define R_XTENSA_SLOT0_OP	20

/* ELF 32bit relocation entry */
struct elf32_relocation {
	uint32_t r_offset;
	uint32_t r_info;
	int32_t	r_addend;
} __packed;

/* binding information - st_info */
#define ELF32_ST_BIND(val)	(((uint8_t) (val)) >> 4)
#define ELF32_ST_TYPE(val)	((val) & 0xf)

/* binding types  - st_info */
#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2

/* ELF 32bit symbol table entry */
struct elf32_symbol {
	uint32_t st_name;
	uint32_t st_value;
	uint32_t st_size;
	uint8_t st_info;
	uint8_t st_other;
	uint16_t st_shndx;
} __packed;

/*
 * ELF module data
 */

#define SOF_DATA_SECTION	0
#define SOF_RODATA_SECTION	1
#define SOF_TEXT_SECTION	2

/* linker script sets both point to start and end of symbol table */
extern unsigned long _symbol_table_start;
extern unsigned long _symbol_table_end;

struct sof_module;

struct sof_module {
	/* driver */
	struct sof *sof;
	struct sof_module_data *drv;

	uint32_t addr;

	/* header */
	struct elf32_file_hdr *hdr;
	uint8_t *elf;
	size_t size;

	/* section headers */
	struct elf32_section_hdr *sect_hdr;

	/* strings */
	struct elf32_section_hdr *str_section;
	const char *str_buf;

	/* symbol table */
	struct elf32_section_hdr *symtab;
	const char *symtab_strings;

	struct list_item list;
};

/* not part of runtime data - can be discarded after relocations */
#define SOF_MODULE(mname, minit, mexit,  		\
		   va, vb, vc,				\
		    vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7)	\
	const struct sof_module_data _module_##mname	\
	__attribute__((used))				\
	__attribute__((section(".module"), used))	\
	= { 						\
			.magic = MODULE_MAGIC,		\
			.abi = MODULE_ABI, 		\
			.isa = PLATFORM_ISA, 		\
			.name = #mname, 		\
			.init = minit, 			\
			.exit = mexit, 			\
			.uuid = {			\
				.a = va, .b = vb, .c = vc,	\
				.d = {vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7}, \
			},				\
	}

struct sof_module *module_init(struct sof *sof, struct ipc_module_new *mod);
int module_reloc(struct sof *sof, struct sof_module *module);
int module_probe(struct sof *sof, struct sof_module *module);
int module_remove(struct sof *sof, struct sof_module *module);

#endif
