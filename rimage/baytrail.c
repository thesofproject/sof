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

/* taken from the linker scripts */
static const struct section byt_sections[] = {
	{"ResetVector", 			0xff2c0000, 0x2e0},
	{"ResetVector.literal", 		0xff2c02e0, 0x120},
	{"WindowVectors", 			0xff2c0400, 0x178},
	{"Level2InterruptVector.literal", 	0xff2c0578, 0x4},
	{"Level2InterruptVector", 		0xff2c057c, 0x1c},
	{"Level3InterruptVector.literal", 	0xff2c0598, 0x4},
	{"Level3InterruptVector", 		0xff2c059c, 0x1c},
	{"Level4InterruptVector.literal", 	0xff2c05b8, 0x4},
	{"Level4InterruptVector", 		0xff2c05bc, 0x1c},
	{"Level5InterruptVector.literal", 	0xff2c05d8, 0x4},
	{"Level5InterruptVector", 		0xff2c05dc, 0x1c},
	{"DebugInterruptVector.literal", 	0xff2c05d8, 0x4},
	{"NMIExceptionVector", 			0xff2c061c},
};

static const enum reef_module_id modules[] = {
	REEF_MODULE_BASE_FW,
	REEF_MODULE_AAC_5_1,
	REEF_MODULE_PCM,
	REEF_MODULE_PCM_SYSTEM,
	REEF_MODULE_PCM_CAPTURE,
	REEF_MODULE_PCM_REFERENCE,
	REEF_MODULE_BLUETOOTH_RENDER_MODULE,
	REEF_MODULE_BLUETOOTH_CAPTURE_MODULE,
};

static int is_iram(struct image *image, Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	uint32_t start, end;

	start = section->sh_addr;
	end = section->sh_addr + section->sh_size;

	if (start < adsp->iram_base)
		return 0;
	if (start >= adsp->iram_base + adsp->iram_size)
		return 0;
	return 1;
}

static int is_dram(struct image *image, Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	uint32_t start, end;

	start = section->sh_addr;
	end = section->sh_addr + section->sh_size;

	if (start < adsp->dram_base)
		return 0;
	if (start >= adsp->dram_base + adsp->dram_size)
		return 0;
	return 1;
}

static int block_idx = 0;

static int write_block(struct image *image, Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	struct dma_block_info block;
	size_t count;
	void *buffer;
	int ret;

	block.size = section->sh_size;

	if (is_iram(image, section)) {
		block.type = REEF_IRAM;
		block.ram_offset = section->sh_addr - adsp->iram_base;
	} else if (is_dram(image, section)) {
		block.type = REEF_DRAM;
		block.ram_offset = section->sh_addr - adsp->dram_base;
	} else {
		fprintf(stderr, "error: invalid block address/size 0x%x/0x%x\n",
			section->sh_addr, section->sh_size);
		return -EINVAL;
	}

	/* write header */
	count = fwrite(&block, sizeof(block), 1, image->out_fd);
	if (count != 1)
		return -errno;

	/* alloc data data */
	buffer = calloc(1, section->sh_size);
	if (buffer == NULL)
		return -ENOMEM;

	/* read in section data */
	ret = fseek(image->in_fd, section->sh_offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to section %d\n", ret);
		goto out;
	}
	count = fread(buffer, 1, section->sh_size, image->in_fd);
	if (count != section->sh_size) {
		fprintf(stderr, "error: cant read section %d\n", -errno);
		ret = -errno;
		goto out;
	}

	/* write out section data */
	count = fwrite(buffer, 1, section->sh_size, image->out_fd);
	if (count != section->sh_size) {
		fprintf(stderr, "error: cant write section %d\n", -errno);
		fprintf(stderr, " foffset %d size 0x%x mem addr 0x%x\n",
			section->sh_offset, section->sh_size, section->sh_addr);
		ret = -errno;
		goto out;
	}

	if (image->verbose) {
		fprintf(stdout, "block: %d\n foffset %d\n size 0x%x\n mem addr 0x%x\n",
			block_idx++, section->sh_offset, section->sh_size,
			section->sh_addr);
	}

out:
	free(buffer);
	return ret;
}

/* used by others */
int byt_write_modules(struct image *image)
{
	const struct adsp *adsp = image->adsp;
	struct byt_module_header hdr;
	Elf32_Shdr *section;
	size_t count;
	int i, err;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);

	memcpy(hdr.signature, REEF_FW_SIGN, REEF_FW_SIGNATURE_SIZE);
	hdr.blocks = image->num_sections - image->num_bss;
	hdr.mod_size = image->text_size + image->data_size + \
				   sizeof(struct dma_block_info) * hdr.blocks;
	hdr.type = REEF_MODULE_BASE_FW;
	hdr.entry_point = 0;//section->sh_addr;

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to write section header %d\n", i);
		return -errno ;
	}

	for (i = 0; i < image->hdr.e_shnum; i++) {

		section = &image->section[i];

		/* only write valid sections */
		if (!(image->section[i].sh_flags & valid))
			continue;

		/* dont write bss */
		if (section->sh_type == SHT_NOBITS)
			continue;

		err = write_block(image, section);
		if (err < 0) {
			fprintf(stderr, "error: failed to write section #%d\n", i);
			return err;
		}
	}

	/* write the remaining 7 fake modules headers, to make the linux driver happy */
	hdr.mod_size = 0;
	hdr.blocks = 0;

	for (i = 1; i < sizeof(modules) / sizeof(modules[0]); i++) {
		hdr.type = modules[i];
		count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
		if (count != 1) {
			fprintf(stderr, "error: failed to write section header %d\n", i);
			return -errno ;
		}
	}

	return 0;
}

/* used by others */
int byt_write_header(struct image *image)
{
	struct fw_header hdr;
	size_t count;

	memcpy(hdr.signature, REEF_FW_SIGN, REEF_FW_SIGNATURE_SIZE);

	hdr.modules = sizeof(modules) / sizeof(modules[0]);
	hdr.file_format = 0;

	image->fw_size += sizeof(struct dma_block_info) *
		(image->num_sections - image->num_bss);
	image->fw_size += sizeof(struct byt_module_header) * hdr.modules;
	hdr.file_size = image->fw_size;

	fprintf(stdout, "fw: image size %ld (0x%lx) bytes %d modules\n\n",
		hdr.file_size + sizeof(hdr), hdr.file_size + sizeof(hdr),
		hdr.modules);
	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return -errno;

	return 0;
}

const struct adsp byt_machine = {
		.name = "byt",
		.iram_base = 0xff2c0000,
		.iram_size = 0x14000,
		.dram_base = 0xff300000,
		.dram_size = 0x28000,
		.image_size = 0xff300000 - 0xff2c0000 + 0x28000,
		.dram_offset = 0xff300000 - 0xff2c0000,
		.machine_id = MACHINE_BAYTRAIL,
		.ops = {
			.write_header = byt_write_header,
			.write_modules = byt_write_modules,
		},
		.sections = byt_sections,
};
