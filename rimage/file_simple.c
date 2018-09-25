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

#define BYT_IRAM_BASE		0xff2c0000
#define BYT_IRAM_HOST_OFFSET	0x0C0000
#define BYT_IRAM_SIZE		(80 * 1024)
#define BYT_DRAM_BASE		0xff300000
#define BYT_DRAM_HOST_OFFSET	0x100000
#define BYT_DRAM_SIZE		(160 * 1024)

#define HSW_IRAM_BASE		0x00000000
#define HSW_IRAM_HOST_OFFSET	0x00080000
#define HSW_IRAM_SIZE		(384 * 1024)
#define HSW_DRAM_BASE		0x00400000
#define HSW_DRAM_HOST_OFFSET	0x00000000
#define HSW_DRAM_SIZE		(512 * 1024)

#define BDW_IRAM_BASE		0x00000000
#define BDW_IRAM_HOST_OFFSET	0x000A0000
#define BDW_IRAM_SIZE		(320 * 1024)
#define BDW_DRAM_BASE		0x00400000
#define BDW_DRAM_HOST_OFFSET	0x00000000
#define BDW_DRAM_SIZE		(640 * 1024)

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
	if (end > adsp->iram_base + adsp->iram_size)
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
	if (end > adsp->dram_base + adsp->dram_size)
		return 0;
	return 1;
}

static int block_idx;

static int write_block(struct image *image, struct module *module,
	Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	struct snd_sof_blk_hdr block;
	size_t count;
	void *buffer;
	int ret;

	block.size = section->sh_size;

	if (is_iram(image, section)) {
		block.type = SOF_BLK_TEXT;
		block.offset = section->sh_addr - adsp->iram_base
			+ adsp->host_iram_offset;
	} else if (is_dram(image, section)) {
		block.type = SOF_BLK_DATA;
		block.offset = section->sh_addr - adsp->dram_base
			+ adsp->host_dram_offset;
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
	ret = fseek(module->fd, section->sh_offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to section %d\n", ret);
		goto out;
	}
	count = fread(buffer, 1, section->sh_size, module->fd);
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

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%8.8lx\t%s\n", block_idx++,
		section->sh_addr, section->sh_size, ftell(image->out_fd),
		block.type == SOF_BLK_TEXT ? "TEXT" : "DATA");

out:
	free(buffer);
	return ret;
}

static int simple_write_module(struct image *image, struct module *module)
{
	struct snd_sof_mod_hdr hdr;
	Elf32_Shdr *section;
	size_t count;
	int i, err;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);

	hdr.num_blocks = module->num_sections - module->num_bss;
	hdr.size = module->text_size + module->data_size +
		sizeof(struct snd_sof_blk_hdr) * hdr.num_blocks;
	hdr.type = SOF_FW_BASE;

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to write section header %d\n",
			-errno);
		return -errno;
	}

	fprintf(stdout, "\n\tTotals\tStart\t\tEnd\t\tSize");

	fprintf(stdout, "\n\tTEXT\t0x%8.8x\t0x%8.8x\t0x%x\n",
			module->text_start, module->text_end,
			module->text_end - module->text_start);
	fprintf(stdout, "\tDATA\t0x%8.8x\t0x%8.8x\t0x%x\n",
			module->data_start, module->data_end,
			module->data_end - module->data_start);
	fprintf(stdout, "\tBSS\t0x%8.8x\t0x%8.8x\t0x%x\n\n ",
			module->bss_start, module->bss_end,
			module->bss_end - module->bss_start);

	fprintf(stdout, "\tNo\tAddress\t\tSize\t\tFile\t\tType\n");

	for (i = 0; i < module->hdr.e_shnum; i++) {

		section = &module->section[i];

		/* only write valid sections */
		if (!(module->section[i].sh_flags & valid))
			continue;

		/* dont write bss */
		if (section->sh_type == SHT_NOBITS)
			continue;

		err = write_block(image, module, section);
		if (err < 0) {
			fprintf(stderr, "error: failed to write section #%d\n", i);
			return err;
		}
	}

	fprintf(stdout, "\n");
	return 0;
}

static int write_block_reloc(struct image *image, struct module *module)
{
	struct snd_sof_blk_hdr block;
	size_t count;
	void *buffer;
	int ret;

	block.size = module->file_size;
	block.type = SOF_BLK_DATA;
	block.offset = 0;

	/* write header */
	count = fwrite(&block, sizeof(block), 1, image->out_fd);
	if (count != 1)
		return -errno;

	/* alloc data data */
	buffer = calloc(1, module->file_size);
	if (!buffer)
		return -ENOMEM;

	/* read in section data */
	ret = fseek(module->fd, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: can't seek to section %d\n", ret);
		goto out;
	}
	count = fread(buffer, 1, module->file_size, module->fd);
	if (count != module->file_size) {
		fprintf(stderr, "error: can't read section %d\n", -errno);
		ret = -errno;
		goto out;
	}

	/* write out section data */
	count = fwrite(buffer, 1, module->file_size, image->out_fd);
	if (count != module->file_size) {
		fprintf(stderr, "error: can't write section %d\n", -errno);
		ret = -errno;
		goto out;
	}

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%8.8lx\t%s\n", block_idx++,
		0, module->file_size, ftell(image->out_fd),
		block.type == SOF_BLK_TEXT ? "TEXT" : "DATA");

out:
	free(buffer);
	return ret;
}

static int simple_write_module_reloc(struct image *image, struct module *module)
{
	struct snd_sof_mod_hdr hdr;
	size_t count;
	int err;

	hdr.num_blocks = 1;
	hdr.size = module->text_size + module->data_size;
	hdr.type = SOF_FW_BASE; // module

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to write section header %d\n",
			-errno);
		return -errno;
	}

	fprintf(stdout, "\n\tTotals\tStart\t\tEnd\t\tSize");

	fprintf(stdout, "\n\tTEXT\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->text_start, module->text_end,
		module->text_end - module->text_start);
	fprintf(stdout, "\tDATA\t0x%8.8x\t0x%8.8x\t0x%x\n",
		module->data_start, module->data_end,
		module->data_end - module->data_start);
	fprintf(stdout, "\tBSS\t0x%8.8x\t0x%8.8x\t0x%x\n\n ",
		module->bss_start, module->bss_end,
		module->bss_end - module->bss_start);

	fprintf(stdout, "\tNo\tAddress\t\tSize\t\tFile\t\tType\n");

	err = write_block_reloc(image, module);
	if (err < 0) {
		fprintf(stderr, "error: failed to write section #%d\n", err);
		return err;
	}

	fprintf(stdout, "\n");
	return 0;
}

/* used by others */
static int simple_write_firmware(struct image *image)
{
	struct snd_sof_fw_header hdr;
	struct module *module;
	size_t count;
	int i, ret;

	memcpy(hdr.sig, SND_SOF_FW_SIG, SND_SOF_FW_SIG_SIZE);

	hdr.num_modules = image->num_modules;
	hdr.abi = SND_SOF_FW_ABI;
	hdr.file_size = 0;

	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];
		module->fw_size += sizeof(struct snd_sof_blk_hdr) *
				(module->num_sections - module->num_bss);
		module->fw_size += sizeof(struct snd_sof_mod_hdr) * hdr.num_modules;
		hdr.file_size += module->fw_size;
	}

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return -errno;

	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];

		fprintf(stdout, "writing module %d %s\n", i, module->elf_file);

		if (image->reloc)
			ret = simple_write_module_reloc(image, module);
		else
			ret = simple_write_module(image, module);
		if (ret < 0) {
			fprintf(stderr, "error: failed to write module %d\n",
				i);
			return ret;
		}
	}

	fprintf(stdout, "firmware: image size %ld (0x%lx) bytes %d modules\n\n",
			hdr.file_size + sizeof(hdr), hdr.file_size + sizeof(hdr),
			hdr.num_modules);

	return 0;
}

int write_logs_dictionary(struct image *image)
{
	struct snd_sof_logs_header header;
	int i, ret = 0;
	void *buffer = NULL;

	memcpy(header.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE);
	header.data_offset = sizeof(struct snd_sof_logs_header);

	for (i = 0; i < image->num_modules; i++) {
		struct module *module = &image->module[i];

		if (module->logs_index > 0) {
			Elf32_Shdr *section = &module->section[module->logs_index];

			header.base_address = section->sh_addr;
			header.data_length = section->sh_size;

			fwrite(&header, sizeof(struct snd_sof_logs_header), 1,
				image->ldc_out_fd);

			buffer = calloc(1, section->sh_size);
			if (!buffer)
				return -ENOMEM;

			fseek(module->fd, section->sh_offset, SEEK_SET);
			size_t count = fread(buffer, 1, section->sh_size,
				module->fd);
			if (count != section->sh_size) {
				fprintf(stderr, "error: can't read section %d\n",
					-errno);
				ret = -errno;
				goto out;
			}
			count = fwrite(buffer, 1, section->sh_size,
				image->ldc_out_fd);
			if (count != section->sh_size) {
				fprintf(stderr, "error: can't write section %d\n",
					-errno);
				ret = -errno;
				goto out;
			}

			fprintf(stdout, "logs dictionary: size %d\n\n",
				header.data_length + header.data_offset);
		}
	}
out:
	if (buffer)
		free(buffer);

	return ret;
}

const struct adsp machine_byt = {
	.name = "byt",
	.iram_base = BYT_IRAM_BASE,
	.iram_size = BYT_IRAM_SIZE,
	.host_iram_offset = BYT_IRAM_HOST_OFFSET,
	.dram_base = BYT_DRAM_BASE,
	.dram_size = BYT_DRAM_SIZE,
	.host_dram_offset = BYT_DRAM_HOST_OFFSET,
	.machine_id = MACHINE_BAYTRAIL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_cht = {
	.name = "cht",
	.iram_base = BYT_IRAM_BASE,
	.iram_size = BYT_IRAM_SIZE,
	.host_iram_offset = BYT_IRAM_HOST_OFFSET,
	.dram_base = BYT_DRAM_BASE,
	.dram_size = BYT_DRAM_SIZE,
	.host_dram_offset = BYT_DRAM_HOST_OFFSET,
	.machine_id = MACHINE_CHERRYTRAIL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_bsw = {
	.name = "bsw",
	.iram_base = BYT_IRAM_BASE,
	.iram_size = BYT_IRAM_SIZE,
	.host_iram_offset = BYT_IRAM_HOST_OFFSET,
	.dram_base = BYT_DRAM_BASE,
	.dram_size = BYT_DRAM_SIZE,
	.host_dram_offset = BYT_DRAM_HOST_OFFSET,
	.machine_id = MACHINE_BRASWELL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_hsw = {
	.name = "hsw",
	.iram_base = HSW_IRAM_BASE,
	.iram_size = HSW_IRAM_SIZE,
	.host_iram_offset = HSW_IRAM_HOST_OFFSET,
	.dram_base = HSW_DRAM_BASE,
	.dram_size = HSW_DRAM_SIZE,
	.host_dram_offset = HSW_DRAM_HOST_OFFSET,
	.machine_id = MACHINE_HASWELL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_bdw = {
	.name = "bdw",
	.iram_base = BDW_IRAM_BASE,
	.iram_size = BDW_IRAM_SIZE,
	.host_iram_offset = BDW_IRAM_HOST_OFFSET,
	.dram_base = BDW_DRAM_BASE,
	.dram_size = BDW_DRAM_SIZE,
	.host_dram_offset = BDW_DRAM_HOST_OFFSET,
	.machine_id = MACHINE_BROADWELL,
	.write_firmware = simple_write_firmware,
};
