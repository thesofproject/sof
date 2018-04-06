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

#include <sof/module.h>
#include <sof/alloc.h>
#include <platform/platform.h>

#define trace_module(__e)	trace_event_atomic(TRACE_CLASS_MODULE, __e)
#define trace_module_value(__e)	trace_value_atomic(__e)
#define trace_module_error(__e)	trace_error(TRACE_CLASS_MODULE, __e)

#define XCC_MOD_OFFSET		0x8

struct section {
	struct elf32_section_hdr *shdr;
	void *data;
};

struct rela {
	struct elf32_section_hdr *shdr;
	struct elf32_relocation *item;
};

struct reloc {

	/* header */
	struct elf32_file_hdr *hdr;
	void *elf;

	/* section headers */
	struct elf32_section_hdr *sect_hdr;

	/* strings */
	struct elf32_section_hdr *str_section;
	void *str_buf;

	/* symbols */
	struct elf32_section_hdr *sym_section;
	struct elf32_symbol *symbol;

	struct section section[3];
	struct rela rela[3];

	struct sof_symbol *symbols;
	int num_symbols;
};

static struct reloc *reloc;

static inline const char *elf_get_string(struct reloc *reloc, int offset)
{
	return (const char *)(reloc->str_buf + offset);
}

static inline uint32_t elf_get_symbol_addr(struct reloc *reloc,
	const char *symbol)
{
	int i;

	/* search symbol table for symbol */
	for (i = 0; i < reloc->num_symbols; i++) {

		if (!rstrcmp(symbol, reloc->symbols[i].name))
			return reloc->symbols[i].value;
	}

	return 0;
}

static int elf_reloc_section(struct reloc *reloc, struct section *section,
	struct rela *rela)
{
	struct elf32_symbol *sym;
	uint32_t addr;
	int i;
	int num_relocs;
	const char *symbol_name;

	/* make sure section and relocation tables exist */
	if (section == NULL || section->shdr == NULL)
		return 0;
	if (rela == NULL || rela->shdr == NULL)
		return 0;

	num_relocs = section->shdr->sh_size / sizeof(struct elf32_relocation);

	/* for each relocation */
	for (i = 0; i < num_relocs; i++) {

		/* make sure relocation is valid type */
		if (ELF32_R_TYPE(rela->item[i].r_info) == R_XTENSA_SLOT0_OP)
			continue;
		if (ELF32_R_TYPE(rela->item[i].r_info) == R_XTENSA_ASM_EXPAND)
			continue;

		/* get symbol table entry for relocaton */
		sym = &reloc->symbol[ELF32_R_SYM(rela->item[i].r_info)];

		/* symbol must have a name TODO: misses data */
		if (sym->st_name == 0)
			continue;

		/* symbol must be global */
		if (ELF32_ST_BIND(sym->st_info) != STB_GLOBAL)
			continue;

		/* get symbol name */
		symbol_name = elf_get_string(reloc, sym->st_name);
		if (!symbol_name)
			return -EINVAL;

		/* get symbol address */
		addr = elf_get_symbol_addr(reloc, symbol_name);
		if (addr == 0)
			return -EINVAL;

		/* add text value to address at offset */
		*((uint32_t *)(section->data + rela->item[i].r_offset)) =
			addr + rela->item[i].r_addend;
	}

	return 0;
}

static inline void reloc_new_section(struct reloc *reloc, int type,
				struct elf32_section_hdr *hdr)
{
	reloc->section[type].shdr = hdr;
	reloc->section[type].data = reloc->elf + hdr->sh_offset;
}

/* find our sof_module_drv in the ELF data */
static inline void reloc_driver(struct sof_module *smod, struct reloc *reloc,
				struct elf32_section_hdr *hdr)
{
	smod->drv = reloc->elf + hdr->sh_offset;

	/* try offset at offset 0 */
	if (!rstrcmp(smod->drv->magic, MODULE_MAGIC))
		return;

	/* xcc places driver at offset */
	smod->drv = reloc->elf + hdr->sh_offset + XCC_MOD_OFFSET;
	if (!rstrcmp(smod->drv->magic, MODULE_MAGIC))
		return;

	/* not found */
	trace_module_error("ed0");
	smod->drv = NULL;
}

/* get data for text and data sections */
static inline void reloc_parse_progs(struct sof_module *smod,
				     struct reloc *reloc,
				     struct elf32_section_hdr *hdr,
				     const char *name)
{
	if (hdr->sh_flags & SHF_EXECINSTR) {
		/* text sections */
		if (!rstrcmp(".text", name)) {
			reloc_new_section(reloc, SOF_TEXT_SECTION, hdr);
			smod->text_bytes = hdr->sh_size;
		}
	} else {
		/* data sections */
		if (!rstrcmp(".data", name)) {
			reloc_new_section(reloc, SOF_DATA_SECTION, hdr);
			smod->data_bytes = hdr->sh_size;
		} else if (!rstrcmp(".rodata", name)) {
			reloc_new_section(reloc, SOF_RODATA_SECTION, hdr);
			smod->rodata_bytes = hdr->sh_size;
		} else if (!rstrcmp(".module.drv", name)) {
			reloc_driver(smod, reloc, hdr);
		}
	}
}

static inline void reloc_new_rela(struct reloc *reloc, int type,
				struct elf32_section_hdr *hdr)
{
	reloc->rela[type].shdr = hdr;
	reloc->rela[type].item = reloc->elf + hdr->sh_offset;
}

/* get data for relocation sections */
static inline void reloc_parse_relas(struct reloc *reloc,
	struct elf32_section_hdr *hdr, const char *name)
{
	if (!rstrcmp(".rela.data", name))
		reloc_new_rela(reloc, SOF_DATA_SECTION, hdr);

	if (!rstrcmp(".rela.rodata", name))
		reloc_new_rela(reloc, SOF_RODATA_SECTION, hdr);

	if (!rstrcmp(".rela.text", name))
		reloc_new_rela(reloc, SOF_TEXT_SECTION, hdr);
}

int arch_elf_parse_sections(struct sof_module *smod)
{
	struct elf32_section_hdr *sect_hdr;
	struct elf32_file_hdr *hdr;
	int i;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	const char *sect_name;

	/* ELF header */
	reloc->hdr = smod->elf;
	hdr = reloc->hdr;

	/* section header */
	reloc->sect_hdr = reloc->elf + hdr->e_shoff;
	sect_hdr = reloc->sect_hdr;

	/* string header */
	reloc->str_section = &sect_hdr[hdr->e_shstrndx];

	/* parse each section for needed data */
	for (i = 0; i < hdr->e_shnum; i++) {

		/* is section empty ? */
		if (sect_hdr[i].sh_size == 0)
			continue;

		/* get the section name */
		sect_name = (char *)reloc->elf +
			reloc->str_section->sh_offset + sect_hdr[i].sh_name;

		switch (sect_hdr[i].sh_type) {
		case SHT_NOBITS:
			/* bss */
			smod->bss_bytes = sect_hdr[i].sh_size;
			break;
		case SHT_PROGBITS:
			/* text or data */

			/* only relocate valid sections */
			if (!(sect_hdr[i].sh_flags & valid))
				continue;

			reloc_parse_progs(smod, reloc, &sect_hdr[i], sect_name);
			break;
		case SHT_RELA:
			reloc_parse_relas(reloc, &sect_hdr[i], sect_name);
			break;
		case SHT_SYMTAB:
			if (!rstrcmp(".symtab", sect_name)) {
				reloc->sym_section = &sect_hdr[i];
				reloc->symbol =
					reloc->elf + sect_hdr[i].sh_offset;
			}
			break;
		case SHT_STRTAB:
			if (!rstrcmp(".strtab", sect_name)) {
				reloc->str_section = &sect_hdr[i];
				reloc->str_buf =
					reloc->elf + sect_hdr[i].sh_offset;
			}
			break;
		default:
			continue;
		}
	}

	/* validate ELF file - check for text section */
	if (!smod->text_bytes) {
		trace_module_error("ep0");
		return -EINVAL;
	}
	/* check for strings */
	if (!reloc->str_buf) {
		trace_module_error("ep1");
		return -EINVAL;
	}
	/* check for symbols */
	if (!reloc->sym_section) {
		trace_module_error("ep2");
		return -EINVAL;
	}
	/* check for relocation tables */
	if (!reloc->rela[SOF_TEXT_SECTION].shdr) {
		trace_module_error("ep3");
		return -EINVAL;
	}
	/* check for module driver */
	if (!smod->drv) {
		trace_module_error("ep4");
		return -EINVAL;
	}

	return 0;
}

int arch_elf_reloc_sections(struct sof_module *smod)
{
	int i;
	int ret;

	/* relocate each section */
	for (i = SOF_DATA_SECTION; i <= SOF_TEXT_SECTION; i++) {
		ret = elf_reloc_section(reloc, &reloc->section[i],
					&reloc->rela[i]);
		if (ret < 0)
			goto err;
	}

	/* write and invalidate text section */
	rmemcpy(smod->text, reloc->section[SOF_TEXT_SECTION].data,
		smod->text_bytes);
	icache_invalidate_region(smod->text, smod->text_bytes);

	/* write and invalidate data sections */
	rmemcpy(smod->data, reloc->section[SOF_DATA_SECTION].data,
		smod->data_bytes);
	dcache_invalidate_region(smod->data, smod->data_bytes);
	rmemcpy(smod->rodata, reloc->section[SOF_RODATA_SECTION].data,
			smod->rodata_bytes);
	dcache_invalidate_region(smod->rodata, smod->rodata_bytes);

	bzero(smod->bss, smod->bss_bytes);
	dcache_invalidate_region(smod->bss, smod->bss_bytes);
	return ret;

err:
	return ret;
}

int arch_reloc_init(struct sof *sof)
{
	reloc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*reloc));
	reloc->symbols = (struct sof_symbol *)_symbol_table_start;
	reloc->num_symbols = (_symbol_table_end - _symbol_table_start) /
		sizeof(struct sof_symbol);
	sof->reloc = reloc;

	return 0;
}

