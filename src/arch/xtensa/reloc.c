/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */
#include <stdlib.h>
#include <sof/module.h>
#include <sof/lib/alloc.h>
#include <platform/platform.h>
#include <sof/lib/memory.h>
#include <sof/module.h>

/* TODO: just a copy today 4f9c3ec7-7b55-400c-86b3-502b4420e625 */
DECLARE_SOF_UUID("module", module_uuid, 0x4f9c3ec7, 0x7b55, 0x400c,
		 0x86, 0xb3, 0x50, 0x2b, 0x44, 0x20, 0xe6, 0x25);

DECLARE_TR_CTX(mod_tr, SOF_UUID(module_uuid), LOG_LEVEL_INFO);

#define XCC_MOD_OFFSET		0x8

#define XTENSA_OPCODE_CALLN	0x5
#define XTENSA_OPMASK_CALLN	0xf

#define XTENSA_OPCODE_L32R	0x1
#define XTENSA_OPMASK_L32R	0xf

struct sym_tab {
	struct sof_symbol *symbol;
	int num_symbols;
};

static inline int is_calln_opcode(uint8_t opcode)
{
	return (opcode & XTENSA_OPMASK_CALLN) == XTENSA_OPCODE_CALLN;
}

static inline int is_l32r_opcode(uint8_t opcode)
{
	return (opcode & XTENSA_OPMASK_L32R) == XTENSA_OPCODE_L32R;
}

static inline const char *sect_name(struct sof_module *m, int section)
{
	return m->str_buf + m->sect_hdr[section].sh_name;
}

static inline uint8_t *elf_get_sect_data(struct sof_module *m, int section,
					 uint32_t offset)
{
	return (uint8_t *)m->hdr + m->sect_hdr[section].sh_offset + offset;
}

static uint32_t lookup_symbol(struct sym_tab *s, const char *symbol)
{
	int i;

	for (i = 0; i < s->num_symbols; i++) {
		if (!strcmp(s->symbol[i].name, symbol))
			return s->symbol[i].value;
	}

	/* not found so give a NULL address */
	return 0;
}

static int elf_read_hdr(struct sof_module *m)
{
	struct elf32_file_hdr *hdr = m->hdr;

	if (strncmp((char *)hdr->e_ident, "\177ELF\001\001", 5)) {
		tr_err(&mod_tr, "Not a 32 bits ELF-LE file\n");
		return -EINVAL;
	}

	return 0;
}

static uint32_t reloc_get_reloc_add(struct sym_tab *sym_tab, struct sof_module *m,
				    struct elf32_relocation *rela,
				    uint32_t rela_idx, const char **symbol_)
{
	struct elf32_symbol *sym;
	uint32_t reloc_add;
	const char *symbol;

	sym = (struct elf32_symbol *)(m->elf + m->symtab->sh_offset);
	sym += ELF32_R_SYM(rela[rela_idx].r_info);
	symbol = m->symtab_strings + sym->st_name;

	/* lookup the symbol */
	if (*symbol != 0) {
		reloc_add = lookup_symbol(sym_tab, symbol);
		if (reloc_add == 0) {
			/* TODO re-enable when building with Zephyr for %s support */
			//tr_err(&mod_tr, "error can't find symbol %s\n", symbol);
			return 0;
		}
	} else
		reloc_add = m->sect_hdr[sym->st_shndx].sh_addr +
			rela[rela_idx].r_addend +  m->addr;

	*symbol_ = symbol;
	return reloc_add;
}

static int reloc_calln(struct sof_module *m, uint32_t reloc_add, uint32_t addr,
		      unsigned char *data)
{
	/* calc the call address from the caller offset and align to words  */
	reloc_add -= (addr & -4) + 4;

	/* must be word aligned */
	if (reloc_add & 3) {
		tr_err(&mod_tr, "error: call reloc 0x%x at 0x%x is not word aligned\n",
			reloc_add, addr);
		return -ENOEXEC;
	}

	/* reloc is signed 18 bits */
	if ((int32_t)reloc_add >= (1 << 19) ||
		(int32_t)(reloc_add) < -(1 << 19)) {
		tr_err(&mod_tr, "error: call reloc 0x%x at 0x%x is out of range\n",
			reloc_add, addr);
		return -ENOEXEC;
	}

	/* convert reloc to signed words */
	reloc_add = (int32_t)reloc_add >> 2;

	/* copy reloc add bits 17 .. 0 to opcode 23 .. 6 */
	data[0] = ((data[0] & ~0xc0) |
		    ((reloc_add << 6) & 0xc0));
	data[1] = (reloc_add >> 2) & 0xff;
	data[2] = (reloc_add >> 10) & 0xff;

	return 0;
}

static int reloc_l32r(struct sof_module *m, uint32_t reloc_add, uint32_t addr,
		      unsigned char *data)
{
	/* calc the load address from loader offset and align to words */
	reloc_add -= (addr + 3) & -4;

	if (reloc_add & 3) {
		tr_err(&mod_tr, "error: l32r reloc 0x%x at 0x%x is not word aligned\n",
			reloc_add, addr);
		return -ENOEXEC;
	}

	/* reloc is signed 16 bits */
	if ((int32_t)reloc_add >= (1 << 17) ||
		(int32_t)(reloc_add) < -(1 << 17)) {
		tr_err(&mod_tr, "error: l32r reloc 0x%x at 0x%x is out of range\n",
			reloc_add, addr);
		return -ENOEXEC;
	}

	/* convert reloc to signed words */
	reloc_add = (int32_t)reloc_add >> 2;

	/* copy reloc_add bits 16..0 to opcode 23 ..8 */
	data[1] = reloc_add & 0xff;
	data[2] = (reloc_add >> 8) & 0xff;

	return 0;
}


static int relocate_section(struct sym_tab *sym_tab, struct sof_module *m, unsigned int relsec)
{
	struct elf32_section_hdr *sechdrs = m->sect_hdr;
	struct elf32_relocation *rela =
		(struct elf32_relocation *)(m->elf + sechdrs[relsec].sh_offset);
	unsigned int i;
	unsigned char *data;
	const char *symbol = NULL;
	uint32_t reloc_add;
	uint32_t addr;
	int err;

	tr_dbg(&mod_tr, "relocate section %s to %s\n",
		sect_name(m, relsec),
		sect_name(m, sechdrs[relsec].sh_info));

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rela); i++) {

		data = elf_get_sect_data(m, sechdrs[relsec].sh_info,
					    rela[i].r_offset);
		reloc_add = reloc_get_reloc_add(sym_tab, m, rela, i, &symbol);
		addr = sechdrs[sechdrs[relsec].sh_info].sh_addr +
				rela[i].r_offset + m->addr;

		tr_dbg(&mod_tr, "item %d addr 0x%x add 0x%x symbol %s rela 0x%x 0x%x",
			i, addr, reloc_add, symbol ? symbol : "none",
			ELF32_R_SYM(rela[i].r_info), rela[i].r_addend);

		switch (ELF32_R_TYPE(rela[i].r_info)) {
		case R_XTENSA_NONE:
		case R_XTENSA_DIFF8:
		case R_XTENSA_DIFF16:
		case R_XTENSA_DIFF32:
		case R_XTENSA_ASM_EXPAND:
			/* nothing to do here */
			break;
		case R_XTENSA_32:
		case R_XTENSA_PLT:
			/* perform a 32bit relocation addition */
			*(uint32_t *)data += reloc_add;

			tr_dbg(&mod_tr, "R_XTENSA_PLT | R_XTENSA_32: addr 0x%8.8x = 0x%8.8x\n",
				addr, *(uint32_t *)data);
			break;
		case R_XTENSA_SLOT0_OP:
			if (is_calln_opcode(data[0])) {
				tr_dbg(&mod_tr, "R_XTENSA_SLOT0_OP call at 0x%x with %s\n",
					addr, symbol);

				err = reloc_calln(m, reloc_add, addr, data);
				if (err < 0)
					return err;
				break;
			} else if (is_l32r_opcode(data[0])) {
				tr_dbg(&mod_tr, "R_XTENSA_SLOT0_OP l32r at 0x%x with 0x%x\n",
					addr, reloc_add);

				err = reloc_l32r(m, reloc_add, addr, data);
				if (err < 0)
					return err;
				break;
			} else {
				/* probably safe - used by branching which is relative */
				tr_dbg(&mod_tr, "R_XTENSA_SLOT0_OP unhandled at 0x%8.8x\n", addr);
				break;
			}
		default:
			/* dont know about this so abort */
			tr_err(&mod_tr, "error: unexpected relocation at 0x%x type 0x%x\n",
			       addr,  ELF32_R_TYPE(rela[i].r_info));
			return -ENOEXEC;
		}
	}

	return 0;
}

static int elf_relocate(struct sof_module *m, struct sym_tab *sym_tab)
{
	unsigned int i;
	int err = 0;

	/* find the symbol table */
	for (i = 1; i < m->hdr->e_shnum; i++) {

		if (m->sect_hdr[i].sh_type != SHT_SYMTAB)
			continue;

		/* found */
		m->symtab = &m->sect_hdr[i];
		m->symtab_strings = (char *)m->hdr
			+ m->sect_hdr[m->symtab->sh_link].sh_offset;
		goto reloc;
	}

	/* not found */
	tr_err(&mod_tr, "error: cant find symbol table in ELF data\n");
	return -ENOEXEC;

reloc:
	/* do the relocations section by section (0 is empty) */
	for (i = 1; i < m->hdr->e_shnum; i++) {

		/* sections need to be allocated for reldata */
		if (!(m->sect_hdr[m->sect_hdr[i].sh_info].sh_flags & SHF_ALLOC))
			continue;

		/* is this section in the header ? */
		if (m->sect_hdr[i].sh_info >= m->hdr->e_shnum)
			continue;

		/* relocate if its a RELA section */
		if (m->sect_hdr[i].sh_type == SHT_RELA) {
			err = relocate_section(sym_tab, m, i);
			if (err < 0) {
				tr_dbg(&mod_tr, "error: failed to relocate section %i\n",i);
				return err;
			}
		}
	}

	return 0;
}

int arch_elf_reloc_sections(struct elf32_file_hdr *hdr, struct sym_tab *sym_tab,
			   size_t size)
{

	struct sof_module *mod;
	int ret;

	mod = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*mod));
	if (!mod)
		return -ENOMEM;

	/* setup and validate ELF module */
	mod->hdr = hdr;
	mod->elf = (uint8_t *)hdr;
	mod->size = size;
	ret = elf_read_hdr(mod);
	if (!ret) {
		rfree(mod);
		return ret;
	}

	/* get section headers and validate */
	mod->sect_hdr = (struct elf32_section_hdr *)(mod->elf + mod->hdr->e_shoff);
	if ((uint8_t *)mod->sect_hdr >=
	    mod->elf + hdr->e_shnum * sizeof(struct elf32_section_hdr)) {
		tr_err(&mod_tr, "section headers outside of ELF data");
		rfree(mod);
		return -ENOEXEC;
	}

	/* get string section header */
	if (hdr->e_shstrndx >= hdr->e_shnum) {
		tr_err(&mod_tr, "invalid section header index for strings %d max %d",
		       hdr->e_shstrndx, hdr->e_shnum);
		rfree(mod);
		return -ENOEXEC;
	}
	mod->str_section = &mod->sect_hdr[hdr->e_shstrndx];

	/* get strings */
	mod->str_buf = (const char *)(mod->elf + mod->sect_hdr[hdr->e_shstrndx].sh_offset);

	/* TODO: allocate text, data and bss region and copy ELF data */

	/* relocate to new region */
	ret = elf_relocate(mod, sym_tab);

#if 0
	icache_invalidate_region(smod->text, smod->text_bytes);
	dcache_invalidate_region(smod->data, smod->data_bytes);


	/* zero the BSS and invalidate it */
	bzero(smod->bss, smod->bss_bytes);
	dcache_invalidate_region(smod->bss, smod->bss_bytes);
#endif

	return ret;
}

int arch_reloc_init(struct sof *sof)
{
	struct sym_tab *s;

	s = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*s));

	s->symbol = (struct sof_symbol *)_symbol_table_start;
	s->num_symbols = (_symbol_table_end - _symbol_table_start) /
		sizeof(struct sof_symbol);
	list_init(&sof->module_list);

	tr_info(&mod_tr, "symbol table at %p with %d symbols", s->symbol,
			s->num_symbols);
	sof->symbol_table = s;

	return 0;
}

