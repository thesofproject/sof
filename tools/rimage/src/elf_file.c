// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rimage/elf_file.h>
#include <rimage/file_utils.h>
#include <rimage/misc_utils.h>

 /* Values for e_type. */
static struct name_val e_type[] = {
	NAME_VAL_ENTRY(ET_NONE),		/* Unknown type. */
	NAME_VAL_ENTRY(ET_REL),			/* Relocatable. */
	NAME_VAL_ENTRY(ET_EXEC),		/* Executable. */
	NAME_VAL_ENTRY(ET_DYN),			/* Shared object. */
	NAME_VAL_ENTRY(ET_CORE),		/* Core file. */
	NAME_VAL_END
};

static struct name_val sh_types[] = {
	NAME_VAL_ENTRY(SHT_NULL),		/* inactive */
	NAME_VAL_ENTRY(SHT_PROGBITS),		/* program defined information */
	NAME_VAL_ENTRY(SHT_SYMTAB),		/* symbol table section */
	NAME_VAL_ENTRY(SHT_STRTAB),		/* string table section */
	NAME_VAL_ENTRY(SHT_RELA),		/* relocation section with addends */
	NAME_VAL_ENTRY(SHT_HASH),		/* symbol hash table section */
	NAME_VAL_ENTRY(SHT_DYNAMIC),		/* dynamic section */
	NAME_VAL_ENTRY(SHT_NOTE),		/* note section */
	NAME_VAL_ENTRY(SHT_NOBITS),		/* no space section */
	NAME_VAL_ENTRY(SHT_REL),		/* relocation section - no addends */
	NAME_VAL_ENTRY(SHT_SHLIB),		/* reserved - purpose unknown */
	NAME_VAL_ENTRY(SHT_DYNSYM),		/* dynamic symbol table section */
	NAME_VAL_ENTRY(SHT_INIT_ARRAY),		/* Initialization function pointers. */
	NAME_VAL_ENTRY(SHT_FINI_ARRAY),		/* Termination function pointers. */
	NAME_VAL_ENTRY(SHT_PREINIT_ARRAY),	/* Pre-initialization function ptrs. */
	NAME_VAL_ENTRY(SHT_GROUP),		/* Section group. */
	NAME_VAL_ENTRY(SHT_SYMTAB_SHNDX),	/* Section indexes (see SHN_XINDEX). */
	NAME_VAL_ENTRY(SHT_LOOS),		/* First of OS specific semantics */
	NAME_VAL_ENTRY(SHT_HIOS),		/* Last of OS specific semantics */
	NAME_VAL_ENTRY(SHT_GNU_VERDEF),
	NAME_VAL_ENTRY(SHT_GNU_VERNEED),
	NAME_VAL_ENTRY(SHT_GNU_VERSYM),
	NAME_VAL_ENTRY(SHT_LOPROC),		/* reserved range for processor */
	NAME_VAL_ENTRY(SHT_HIPROC),		/* specific section header types */
	NAME_VAL_ENTRY(SHT_LOUSER),		/* reserved range for application */
	NAME_VAL_ENTRY(SHT_HIUSER),		/* specific indexes */
	NAME_VAL_END
};

/* Flags for sh_flags. */
static struct name_val sh_flags[] = {
	NAME_VAL_ENTRY(SHF_WRITE),		/* Section contains writable data. */
	NAME_VAL_ENTRY(SHF_ALLOC),		/* Section occupies memory. */
	NAME_VAL_ENTRY(SHF_EXECINSTR),		/* Section contains instructions. */
	NAME_VAL_ENTRY(SHF_MERGE),		/* Section may be merged. */
	NAME_VAL_ENTRY(SHF_STRINGS),		/* Section contains strings. */
	NAME_VAL_ENTRY(SHF_INFO_LINK),		/* sh_info holds section index. */
	NAME_VAL_ENTRY(SHF_LINK_ORDER),		/* Special ordering requirements. */
	NAME_VAL_ENTRY(SHF_OS_NONCONFORMING),	/* OS-specific processing required. */
	NAME_VAL_ENTRY(SHF_GROUP),		/* Member of section group. */
	NAME_VAL_ENTRY(SHF_TLS),		/* Section contains TLS data. */
	NAME_VAL_END
};

/* Values for p_type. */
static struct name_val p_type[] = {
	NAME_VAL_ENTRY(PT_NULL),		/* Unused entry. */
	NAME_VAL_ENTRY(PT_LOAD),		/* Loadable segment. */
	NAME_VAL_ENTRY(PT_DYNAMIC),		/* Dynamic linking information segment. */
	NAME_VAL_ENTRY(PT_INTERP),		/* Pathname of interpreter. */
	NAME_VAL_ENTRY(PT_NOTE),		/* Auxiliary information. */
	NAME_VAL_ENTRY(PT_SHLIB),		/* Reserved (not used). */
	NAME_VAL_ENTRY(PT_PHDR),		/* Location of program header itself. */
	NAME_VAL_ENTRY(PT_TLS),			/* Thread local storage segment */
	NAME_VAL_END
};

/* Values for p_flags. */
static struct name_val p_flags[] = {
	NAME_VAL_ENTRY(PF_X),			/* Executable. */
	NAME_VAL_ENTRY(PF_W),			/* Writable. */
	NAME_VAL_ENTRY(PF_R),			/* Readable. */
	NAME_VAL_END
};

/**
 * Print elf related error message
 * 
 * @param elf elf file structure
 * @param msg error message
 * @param error error code to return
 * @return error code
 */
static int elf_error(const struct elf_file *elf, const char *msg, int error)
{
	fprintf(stderr, "Error: %s: %s\n", elf->filename, msg);
	return -error;
}

/**
 * Read elf header
 * 
 * @param elf elf file structure
 * @return error code, 0 when success
 */
static int elf_header_read(struct elf_file *elf)
{
	size_t count;

	/* read in elf header */
	count = fread(&elf->header, sizeof(elf->header), 1, elf->file);
	if (count != 1) {
		if (count < 0)
			return file_error("failed to read elf header", elf->filename);
		else
			return elf_error(elf, "Corrupted file.", ENODATA);
	}

	if (strncmp((char *)elf->header.ident, "\177ELF\001\001", 5))
		return elf_error(elf, "Not a 32 bits ELF-LE file", EILSEQ);

	if (elf->header.version != EV_CURRENT)
		return elf_error(elf, "Unsupported file version.", EINVAL);

	if (elf->header.ehsize < sizeof(Elf32_Ehdr))
		return elf_error(elf, "Invalid file header size.", EINVAL);

	if (elf->header.phoff >= elf->file_size)
		return elf_error(elf, "Invalid program header file offset.", EINVAL);

	if (elf->header.phentsize < sizeof(Elf32_Phdr))
		return elf_error(elf, "Invalid program header size.", EINVAL);

	if (elf->header.phoff + elf->header.phnum * sizeof(Elf32_Phdr) > elf->file_size)
		return elf_error(elf, "Invalid number of program header entries.", EINVAL);

	if (elf->header.shoff >= elf->file_size)
		return elf_error(elf, "Invalid section header file offset.", EINVAL);

	if (elf->header.shentsize < sizeof(Elf32_Shdr))
		return elf_error(elf, "Invalid section header size.", EINVAL);

	if (elf->header.shoff + elf->header.shnum * sizeof(Elf32_Shdr) > elf->file_size)
		return elf_error(elf, "Invalid number of section header entries.", EINVAL);

	if (elf->header.shstrndx >= elf->header.shnum)
		return elf_error(elf, "Invalid section name strings section index.", EINVAL);

	return 0;
}

void elf_header_print(const struct elf_file *elf)
{
	fprintf(stdout, "\tfile type\t 0x%8.8x ", elf->header.type);
	print_enum(elf->header.type, e_type);
	fprintf(stdout, "\tarchitecture\t 0x%8.8x\n", elf->header.machine);
	fprintf(stdout, "\tformat version\t 0x%8.8x\n", elf->header.version);
	fprintf(stdout, "\tarch flags\t 0x%8.8x\n", elf->header.flags);
	fprintf(stdout, "\theader size\t 0x%8.8x\n", elf->header.ehsize);
	fprintf(stdout, "\tentry point\t 0x%8.8x\n", elf->header.entry);
	fprintf(stdout, "\tprogram offset\t 0x%8.8x\n", elf->header.phoff);
	fprintf(stdout, "\tsection offset\t 0x%8.8x\n", elf->header.shoff);
	fprintf(stdout, "\tprogram size\t 0x%8.8x\n", elf->header.phentsize);
	fprintf(stdout, "\tprogram count\t 0x%8.8x\n", elf->header.phnum);
	fprintf(stdout, "\tsection size\t 0x%8.8x\n", elf->header.shentsize);
	fprintf(stdout, "\tsection count\t 0x%8.8x\n", elf->header.shnum);
	fprintf(stdout, "\tstring index\t 0x%8.8x\n\n", elf->header.shstrndx);
}

/**
 * Read sections headers from elf file
 * 
 * @param elf elf file structure
 * @return error code, 0 when success
 */
static int elf_section_headers_read(struct elf_file *elf)
{
	int i, ret;
	size_t offset, count;

	elf->sections = calloc(elf->header.shnum, sizeof(struct elf_section_header));
	if (!elf->sections)
		return elf_error(elf, "Cannot allocate section array.", ENOMEM);

	/* In case of error, sections memory are released in elf_open function. */

	offset = elf->header.shoff;
	for (i = 0; i < elf->header.shnum; i++, offset += elf->header.shentsize) {
		ret = fseek(elf->file, offset, SEEK_SET);
		if (ret)
			return file_error("unable to seek to section header", elf->filename);

		count = fread(&elf->sections[i].data, sizeof(Elf32_Shdr), 1, elf->file);
		if (count != 1) {
			if (count < 0)
				return file_error("failed to read section header", elf->filename);
			else
				return elf_error(elf, "Corrupted file.", ENODATA);
		}
	}

	elf->sections_count = elf->header.shnum;
	return 0;
}

/**
 * Update name of a section in the section headers
 * 
 * @param elf elf file structure
 * @return error code, 0 when success
 */
static int elf_set_sections_names(struct elf_file *elf, const struct elf_strings *strings)
{
	int ret, i;

	for (i = 0; i < elf->sections_count; i++) {
		ret = elf_strings_get(strings, elf->sections[i].data.name, &elf->sections[i].name);
		if (ret)
			return ret;
	}

	return 0;
}

void elf_print_sections(const struct elf_file *elf)
{
	int i;

	for (i = 0; i < elf->sections_count; i++) {
		fprintf(stdout, "Section %d:\n", i);
		elf_section_header_print(&elf->sections[i]);
	}
}

/**
 * Read program headers from elf file
 * 
 * @param elf elf file structure
 * @return error code, 0 when success
 */
static int elf_program_headers_read(struct elf_file *elf)
{
	int i, ret;
	size_t offset, count;

	elf->programs = calloc(elf->header.phnum, sizeof(Elf32_Phdr));
	if (!elf->programs)
		return elf_error(elf, "Cannot allocate program array.", ENOMEM);

	/* In case of error, programs memory are released in elf_open function. */

	offset = elf->header.phoff;
	for (i = 0; i < elf->header.phnum; i++, offset += elf->header.phentsize) {
		ret = fseek(elf->file, offset, SEEK_SET);
		if (ret)
			return file_error("unable to seek to program header", elf->filename);

		count = fread(&elf->programs[i], sizeof(Elf32_Phdr), 1, elf->file);
		if (count != 1) {
			if (count < 0)
				return file_error("failed to read program header", elf->filename);
			else
				return elf_error(elf, "Corrupted file.", ENODATA);
		}
	}

	elf->programs_count = elf->header.phnum;
	return 0;
}

void elf_print_programs(const struct elf_file *elf)
{
	int i;

	for (i = 0; i < elf->programs_count; i++) {
		fprintf(stdout, "\nProgram %d:\n", i);
		elf_program_header_print(&elf->programs[i]);
	}
}

/**
 * Copy elf_header structure. Allocates a new copy of the name string.
 * 
 * @param [in]src Source section header structure
 * @param [out]dst Destination section header structure
 * @return error code, 0 when success
 */
static int elf_section_header_copy(const struct elf_section_header *src,
				   struct elf_section_header *dst)
{
	if (src->name) {
		dst->name = strdup(src->name);
		if (!dst->name)
			return -ENOMEM;
	} else {
		dst->name = NULL;
	}

	memcpy(&dst->data, &src->data, sizeof(dst->data));
	return 0;
}

int elf_section_header_get_by_index(const struct elf_file *elf, int index,
				     const struct elf_section_header **header)
{
	if (index >= elf->sections_count)
		return elf_error(elf, "Invalid section index.", EINVAL);

	*header = &elf->sections[index];

	return 0;
}

int elf_section_header_get_by_name(const struct elf_file *elf, const char* name,
				   const struct elf_section_header **header)
{
	int i;

	*header = NULL;

	for (i = 0; i < elf->sections_count; i++)
		if (strcmp(elf->sections[i].name, name) == 0) {
			*header = &elf->sections[i];
			return 0;
		}

	return -ENOENT;
}

void elf_section_header_print(const struct elf_section_header *header)
{
	fprintf(stdout, "\tname\t\t0x%8.8x\n", header->data.name);
	fprintf(stdout, "\tname\t\t%s\n", header->name);
	fprintf(stdout, "\ttype\t\t0x%8.8x ", header->data.type);
	print_enum(header->data.type, sh_types);
	fprintf(stdout, "\tflags\t\t0x%8.8x ", header->data.flags);
	print_flags(header->data.flags, sh_flags);
	fprintf(stdout, "\taddr\t\t0x%8.8x\n", header->data.vaddr);
	fprintf(stdout, "\toffset\t\t0x%8.8x\n", header->data.off);
	fprintf(stdout, "\tsize\t\t0x%8.8x\n", header->data.size);
	fprintf(stdout, "\tlink\t\t0x%8.8x\n", header->data.link);
	fprintf(stdout, "\tinfo\t\t0x%8.8x\n", header->data.info);
	fprintf(stdout, "\taddralign\t0x%8.8x\n", header->data.addralign);
	fprintf(stdout, "\tentsize\t\t0x%8.8x\n\n", header->data.entsize);
}

/**
 * Release section header structure
 * 
 * @param sec_hdr Section header structure
 */
static void elf_section_header_free(struct elf_section_header *sec_hdr)
{
	free(sec_hdr->name);
	sec_hdr->name = NULL;
}


int elf_open(struct elf_file *elf, const char *filename)
{
	struct elf_strings names;
	int ret = -ENOMEM;

	memset(elf, 0, sizeof(*elf));
	elf->filename = strdup(filename);
	if (!elf->filename) {
		ret = -ENOMEM;
		goto err;
	}

	elf->file = fopen(filename, "rb");
	if (!elf->file) {
		ret = file_error("Unable to open elf file", elf->filename);
		goto err;
	}

	ret = get_file_size(elf->file, elf->filename, &elf->file_size);
	if (ret)
		goto err;

	ret = elf_header_read(elf);
	if (ret)
		goto err;

	ret = elf_program_headers_read(elf);
	if (ret)
		goto err;

	ret = elf_section_headers_read(elf);
	if (ret)
		goto err;

	ret = elf_strings_read_by_index(elf, elf->header.shstrndx, &names);
	if (ret)
		goto err;

	ret = elf_set_sections_names(elf, &names);
	if (ret) {
		elf_strings_free(&names);
		goto err;
	}
	elf_strings_free(&names);

	return 0;

err:
	free(elf->filename);
	free(elf->programs);

	if (elf->file)
		fclose(elf->file);

	if (elf->sections) {
		for (int i = 0; i < elf->sections_count; i++)
			elf_section_header_free(&elf->sections[i]);

		free(elf->sections);
	}

	return ret;
}

/**
* Close elf file and release resources
* @param elf elf file structure
*/
void elf_free(struct elf_file *elf)
{
	int i;

	free(elf->filename);
	fclose(elf->file);

	for (i = 0; i < elf->sections_count; i++)
		elf_section_header_free(&elf->sections[i]);

	free(elf->sections);
	free(elf->programs);
}

int elf_section_read_content(const struct elf_file *elf, const struct elf_section_header *header,
			     void *buffer, const size_t size)
{
	int ret;
	size_t count;

	if ((header->data.type == SHT_NOBITS) || (header->data.type == SHT_NULL) ||
	    !header->data.size)
		return elf_error(elf, "Can't read section without data.", ENODATA);

	if (!header->data.off || (header->data.off + header->data.size) > elf->file_size)
		return elf_error(elf, "Invalid section position in file.", ENFILE);
	
	if (header->data.size > size)
		return elf_error(elf, "Output buffer too small.", ENOSPC);

	ret = fseek(elf->file, header->data.off, SEEK_SET);
	if (ret)
		return file_error("unable to seek to section data", elf->filename);

	count = fread(buffer, header->data.size, 1, elf->file);
	if (count != 1) {
		if (count < 0)
			return file_error("failed to read section data", elf->filename);
		else
			return elf_error(elf, "Corrupted file.", ENODATA);
	}

	return 0;
}

int elf_section_read(const struct elf_file *elf, const struct elf_section_header *header,
		     struct elf_section *section)
{
	int ret;

	section->data = malloc(header->data.size);
	if (!section->data)
		return elf_error(elf, "No memory for section buffer.", ENOMEM);

	ret = elf_section_header_copy(header, &section->header);
	if (ret)
		goto err;

	ret = elf_section_read_content(elf, header, section->data, header->data.size);
	if (ret)
		goto err;

	return 0;

err:
	elf_section_header_free(&section->header);
	free(section->data);
	return ret;
}

int elf_section_read_by_name(const struct elf_file *elf, const char *name,
			     struct elf_section *section)
{
	const struct elf_section_header *header;
	int ret;

	ret = elf_section_header_get_by_name(elf, name, &header);

	if (ret)
		return ret;

	return elf_section_read(elf, header, section);
}

void elf_section_free(struct elf_section *section)
{
	free(section->data);
}

int elf_strings_read_by_index(const struct elf_file *elf, int index, struct elf_strings *strings)
{
	const struct elf_section_header *header;
	int ret;

	ret = elf_section_header_get_by_index(elf, index, &header);
	if (ret)
		return ret;

	if (header->data.type != SHT_STRTAB)
		return elf_error(elf, "Invalid section type.", EINVAL);

	ret = elf_section_read(elf, header, &strings->section);
	if (ret)
		return elf_error(elf, "Unable to read section names section.", ret);

	return 0;
}

int elf_strings_get(const struct elf_strings *strings, int index, char **str)
{
	if (index >= strings->section.header.data.size)
		return -EINVAL;

	*str = strdup((const char *)strings->section.data + index);
	if (!*str)
		return -ENOMEM;

	return 0;
}

void elf_strings_free(struct elf_strings *strings)
{
	elf_section_free(&strings->section);
}

void elf_program_header_print(const Elf32_Phdr *header)
{
	fprintf(stdout, "\ttype\t 0x%8.8x ", header->type);
	print_enum(header->type, p_type);
	fprintf(stdout, "\tflags\t 0x%8.8x ", header->flags);
	print_flags(header->flags, p_flags);
	fprintf(stdout, "\toffset\t 0x%8.8x\n", header->off);
	fprintf(stdout, "\tvaddr\t 0x%8.8x\n", header->vaddr);
	fprintf(stdout, "\tpaddr\t 0x%8.8x\n", header->paddr);
	fprintf(stdout, "\tfilesz\t 0x%8.8x\n", header->filesz);
	fprintf(stdout, "\tmemsz\t 0x%8.8x\n", header->memsz);
	fprintf(stdout, "\talign\t 0x%8.8x\n\n", header->align);
}
