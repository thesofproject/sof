/*
 * ELF to firmware image creator.
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "rimage.h"
#include "file_format.h"

static const struct adsp *machine[] = {
	&byt_machine,
	&cht_machine,
};

static int read_elf_sections(struct image *image)
{
	size_t count;
	int i, ret;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);

	/* read in section header */
	ret = fseek(image->in_fd, image->hdr.e_shoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to section header %d\n", ret);
		return ret;
	}

	image->section = calloc(sizeof(Elf32_Shdr), image->hdr.e_shnum);
	if (image->section == NULL)
		return -ENOMEM;

	count = fread(image->section, sizeof(Elf32_Shdr),
				image->hdr.e_shnum, image->in_fd);
	if (count != image->hdr.e_shnum) {
		fprintf(stderr, "error: failed to read section header %d\n",
			-errno);
		return -errno;
	}

	for (i = 0; i < image->hdr.e_shnum; i++) {

		/* only write valid sections */
		if (!(image->section[i].sh_flags & valid))
			continue;

		switch (image->section[i].sh_type) {
		case SHT_NOBITS:
			/* bss */
			image->bss_size += image->section[i].sh_size;
			image->num_bss++;
			break;
		case SHT_PROGBITS:
			/* text or data */
			image->fw_size += image->section[i].sh_size;

			if (image->section[i].sh_flags & SHF_EXECINSTR)
				image->text_size += image->section[i].sh_size;
			else
				image->data_size += image->section[i].sh_size;
			break;
		default:
			continue;
		}
		
		image->num_sections++;

		if (!image->verbose)
			continue;

		fprintf(stdout, "section-%d: \ttype\t 0x%8.8x\n", i,
			image->section[i].sh_type);
		fprintf(stdout, "section-%d: \tflags\t 0x%8.8x\n", i,
			image->section[i].sh_flags);
		fprintf(stdout, "section-%d: \taddr\t 0x%8.8x\n", i,
			image->section[i].sh_addr);
		fprintf(stdout, "section-%d: \toffset\t 0x%8.8x\n", i,
			image->section[i].sh_offset);
		fprintf(stdout, "section-%d: \tsize\t 0x%8.8x\n", i,
			image->section[i].sh_size);
		fprintf(stdout, "section-%d: \tlink\t 0x%8.8x\n", i,
			image->section[i].sh_link);
		fprintf(stdout, "section-%d: \tinfo\t 0x%8.8x\n\n", i,
			image->section[i].sh_info);
	}

	return 0;
}

static int read_elf_programs(struct image *image)
{
	size_t count;
	int i, ret;

	/* read in program header */
	ret = fseek(image->in_fd, image->hdr.e_phoff, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to program header %d\n", ret);
		return ret;
	}

	image->prg = calloc(sizeof(Elf32_Phdr), image->hdr.e_phnum);
	if (image->prg == NULL)
		return -ENOMEM;

	count = fread(image->prg, sizeof(Elf32_Phdr),
			image->hdr.e_phnum, image->in_fd);
	if (count != image->hdr.e_phnum) {
		fprintf(stderr, "error: failed to read program header %d\n",
			-errno);
		return -errno;
	}

	for (i = 0; i < image->hdr.e_phnum; i++) {

		if (image->prg[i].p_filesz == 0)
			continue;

		if (!image->verbose)
			continue;

		fprintf(stdout, "program-%d: \ttype\t 0x%8.8x\n", i,
			image->prg[i].p_type);
		fprintf(stdout, "program-%d: \toffset\t 0x%8.8x\n", i,
			image->prg[i].p_offset);
		fprintf(stdout, "program-%d: \tvaddr\t 0x%8.8x\n", i,
			image->prg[i].p_vaddr);
		fprintf(stdout, "program-%d: \tpaddr\t 0x%8.8x\n", i,
			image->prg[i].p_paddr);
		fprintf(stdout, "program-%d: \tfsize\t 0x%8.8x\n", i,
			image->prg[i].p_filesz);
		fprintf(stdout, "program-%d: \tmsize\t 0x%8.8x\n", i,
			image->prg[i].p_memsz);
		fprintf(stdout, "program-%d: \tflags\t 0x%8.8x\n\n", i,
			image->prg[i].p_flags);
	}

	return 0;
}

static int write_elf_data(struct image *image)
{
	const struct adsp *adsp = image->adsp;
	int ret = 0;
	size_t count;

	/* read in elf header */
	count = fread(&image->hdr, sizeof(image->hdr), 1, image->in_fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to read elf header %d\n",
			-errno);
		return -errno;
	}

	fprintf(stdout, "elf: \tentry point\t 0x%8.8x\n", image->hdr.e_entry);
	fprintf(stdout, "elf: \tprogram offset\t 0x%8.8x\n", image->hdr.e_phoff);
	fprintf(stdout, "elf: \tsection offset\t 0x%8.8x\n", image->hdr.e_shoff);
	fprintf(stdout, "elf: \tprogram size\t 0x%8.8x\n", image->hdr.e_phentsize);
	fprintf(stdout, "elf: \tprogram count\t 0x%8.8x\n", image->hdr.e_phnum);
	fprintf(stdout, "elf: \tsection size\t 0x%8.8x\n", image->hdr.e_shentsize);
	fprintf(stdout, "elf: \tsection count\t 0x%8.8x\n", image->hdr.e_shnum);
	fprintf(stdout, "elf: \tstring index\t 0x%8.8x\n\n", image->hdr.e_shstrndx);

	ret = read_elf_programs(image);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read program sections %d\n", ret);
		goto out;
	}

	ret = read_elf_sections(image);
	if (ret < 0) {
		fprintf(stderr, "error: failed to read sections %d\n", ret);
		goto out;
	}
	
	fprintf(stdout, "fw: input size %d (0x%x) bytes %d sections\n",
		image->fw_size, image->fw_size, image->num_sections);
	fprintf(stdout, "fw: text %d (0x%x) bytes\n"
			"    data %d (0x%x) bytes\n"
			"    bss  %d (0x%x) bytes\n",
		image->text_size, image->text_size,
		image->data_size, image->data_size,
		image->bss_size, image->bss_size);

	ret = adsp->ops.write_header(image);
	if (ret < 0) {
		fprintf(stderr, "error: failed to write header %d\n", ret);
		goto out;
	}

	ret = adsp->ops.write_modules(image);
	if (ret < 0) {
		fprintf(stderr, "error: failed to write modules %d\n", ret);
		goto out;
	}

out:
	return ret;
}

static void usage(char *name)
{
	fprintf(stdout, "%s:\t -m machine -i infile -o outfile\n", name);
	fprintf(stdout, "\t -v enable verbose output\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	struct image image;
	const char *mach = NULL;
	int opt, ret, i;

	memset(&image, 0, sizeof(image));
	image.text_start = 0xffffffff;
	image.data_start = 0xffffffff;
	image.bss_start = 0xffffffff;

	while ((opt = getopt(argc, argv, "ho:i:m:vba:sk:")) != -1) {
		switch (opt) {
		case 'o':
			image.out_file = optarg;
			break;
		case 'i':
			image.in_file = optarg;
			break;
		case 'm':
			mach = optarg;
			break;
		case 'v':
			image.verbose = 1;
			break;
		case 's':
			image.dump_sections = 1;
			break;
		case 'a':
			image.abi = atoi(optarg);
			break;
		case 'h':
		default: /* '?' */
			usage(argv[0]);
		}
	}

	if (image.in_file == NULL || image.out_file == NULL || mach == NULL)
		usage(argv[0]);

	/* find machine */
	for (i = 0; i < ARRAY_SIZE(machine); i++) {
		if (!strcmp(mach, machine[i]->name)) {
			image.adsp = machine[i];
			goto found;
		}
	}
	fprintf(stderr, "error: machine %s not found\n", mach);
	fprintf(stderr, "error: available machines ");
	for (i = 0; i < ARRAY_SIZE(machine); i++)
		fprintf(stderr, "%s, ", machine[i]->name);
	fprintf(stderr, "/n");

	return -EINVAL;

found:
	/* open infile for reading */
	image.in_fd = fopen(image.in_file, "r");
	if (image.in_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
			image.in_file, errno);
	}

	/* open outfile for writing */
	unlink(image.out_file);
	image.out_fd = fopen(image.out_file, "w");
	if (image.out_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image.out_file, errno);
	}

	/* write data */
	ret = write_elf_data(&image);

	/* close files */
	fclose(image.out_fd);
	fclose(image.in_fd);

	return ret;
}

