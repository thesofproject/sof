/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_SOF_MODULE__
#define __INCLUDE_SOF_MODULE__

#include <stdint.h>
#include <sof/sof.h>
#include <sof/list.h>
#include <arch/reloc.h>
#include <uapi/ipc.h>

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
};

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
};

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
};

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
};

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

/* module driver */
struct sof_module_drv {
	char magic[8];	/* to help loader */
	uint16_t abi[2];		/* to validate against basefw runtime */
	char isa[4];		/* ISA configuration */

	/* general purpose entry and exit - mandatory */
	int (*init)(struct sof_module *mod);
	int (*exit)(struct sof_module *mod);
};

struct sof_module {
	/* driver */
	struct sof *sof;
	struct sof_module_drv *drv;
	void *private;			/* not touched by core */

	/* size of eacch section after parsing */
	size_t bss_bytes;
	size_t text_bytes;
	size_t data_bytes;
	size_t rodata_bytes;

	/* pointer to buffers for each section */
	void *text;
	void *data;
	void *rodata;
	void *bss;

	/* ELF data */
	struct elf32_file_hdr *elf;

	struct list_item list;
};

#define SOF_MODULE(name, minit, mexit) \
	const struct sof_module_drv _module_##name \
	__attribute__((used)) \
	__attribute__((section(".module.drv"), used)) \
	= { MODULE_MAGIC, MODULE_ABI, PLATFORM_ISA, minit, mexit, }

#define SOF_MODULE_REF(name) \
	(uint32_t)( &_module_##name )

#define SOF_MODULE_DECL(name) \
	extern const struct sof_module_drv _module_##name

struct sof_module *module_init(struct sof *sof, struct ipc_module_new *mod);
int module_reloc(struct sof *sof, struct sof_module *module);
int module_probe(struct sof *sof, struct sof_module *module);
int module_remove(struct sof *sof, struct sof_module *module);

#endif
