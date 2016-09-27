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

static int dump_section(struct image *image, struct dma_block_info *block,
	void *data)
{
	const struct adsp *adsp = image->adsp;
	uint32_t addr;

	switch (block->type) {
	case REEF_IRAM:
		addr = block->ram_offset + adsp->iram_base;
		break;
	case REEF_DRAM:
		addr = block->ram_offset + adsp->dram_base;
		break;
	case REEF_CACHE:
		addr = block->ram_offset + adsp->dram_base;
		break;
	default:
		return -EINVAL;
	}
}

static int read_byt_bin_module(struct image *image,
	struct byt_module_header *module)
{
	const struct adsp *adsp = image->adsp;
	struct dma_block_info *block;
	int count, offset;
	void *data;

	block = (void *)module + sizeof(*module);

	for (count = 0; count < module->blocks; count++) {

		if (block->size <= 0) {
			fprintf(stderr, "block %d size invalid\n", count);
			return -EINVAL;
		}

		switch (block->type) {
		case REEF_IRAM:
			offset = block->ram_offset + 0;
			break;
		case REEF_DRAM:
			offset = block->ram_offset +
				adsp->dram_offset;
			break;
		case REEF_CACHE:
			offset = block->ram_offset +
				adsp->dram_offset;
			break;
		default:
			fprintf(stderr, "wrong ram type 0x%x in block0x%x\n",
				block->type, count);
			return -EINVAL;
		}

		fprintf(stdout, "bin:  block %d type %d offset 0x%x size 0x%x\n",
			count, block->type, offset, block->size);

		data = (void *)block + sizeof(*block);
		memcpy(image->out_buffer + offset, data, block->size);

		if (image->dump_sections)
			dump_section(image, block, data);

		block = (void *)block + sizeof(*block) + block->size;
	}

	return 0;
}

static int read_bdw_bin_module(struct image *image,
	struct hsw_module_header *module)
{
	const struct adsp *adsp = image->adsp;
	struct dma_block_info *block;
	int count, offset;
	void *data;

	block = (void *)module + sizeof(*module);

	for (count = 0; count < module->blocks; count++) {

		if (block->size <= 0) {
			fprintf(stderr, "block %d size invalid\n", count);
			return -EINVAL;
		}

		switch (block->type) {
		case REEF_IRAM:
			offset = block->ram_offset + 0;
			break;
		case REEF_DRAM:
			offset = block->ram_offset +
				adsp->dram_offset;
			break;
		case REEF_CACHE:
			offset = block->ram_offset +
				adsp->dram_offset;
			break;
		default:
			fprintf(stderr, "wrong ram type 0x%x in block0x%x\n",
				block->type, count);
			return -EINVAL;
		}

		fprintf(stdout, "bin:  block %d type %d offset 0x%x size 0x%x\n",
			count, block->type, offset, block->size);

		data = (void *)block + sizeof(*block);
		memcpy(image->out_buffer + offset, data, block->size);

		if (image->dump_sections)
			dump_section(image, block, data);

		block = (void *)block + sizeof(*block) + block->size;
	}

	return 0;
}

static int read_bin_data(struct image *image)
{
	const struct adsp *adsp = image->adsp;
	struct fw_header hdr;
	size_t count;

	count = fread(&hdr, sizeof(hdr), 1, image->in_fd);
	if (count != 1)
		return -errno;

	if (strncmp(hdr.signature, REEF_FW_SIGN, REEF_FW_SIGNATURE_SIZE)) {
		fprintf(stderr, "invalid header signature: %c%c%c%c\n",
			hdr.signature[0], hdr.signature[1],
			hdr.signature[2], hdr.signature[3]);
		return -EINVAL;
	}

	image->fw_size = hdr.file_size;
	image->num_modules = hdr.modules;

	fprintf(stdout, "bin: input image size %ld (0x%lx) bytes %d modules\n",
		hdr.file_size + sizeof(hdr), hdr.file_size + sizeof(hdr),
		hdr.modules);
	fprintf(stdout, "bin: output image size %d (0x%x) bytes\n",
		adsp->image_size,
		adsp->image_size);

	image->in_buffer = calloc(image->fw_size, 1);
	if (image->in_buffer == NULL)
		return -ENOMEM;

	count = fread(image->in_buffer, image->fw_size, 1, image->in_fd);
	if (count != 1) {
		free(image->in_buffer);
		return -errno;
	}

	image->out_buffer = calloc(adsp->image_size, 1);
	if (image->out_buffer == NULL) {
		free(image->in_buffer);
		return -ENOMEM;
	}

	return 0;
}

int write_byt_binary_image(struct image *image)
{
	const struct adsp *adsp = image->adsp;
	struct byt_module_header *module;
	int ret, count;

	ret = read_bin_data(image);
	if (ret < 0)
		return ret;

	module = (void *)image->in_buffer;

	for (count = 0; count < image->num_modules; count++) {

		fprintf(stdout, "bin: module %d type %d blocks %d size 0x%x\n",
			count, module->type, module->blocks, module->mod_size);

		ret = read_byt_bin_module(image, module);
		if (ret < 0) {
			fprintf(stderr, "invalid module %d\n", count);
			return ret;
		}
		module = (void *)module + sizeof(*module) + module->mod_size;
	}

	count = fwrite(image->out_buffer, adsp->image_size, 1,
		image->out_fd);
	if (count != adsp->image_size)
		return -errno;

	return ret;
}

int write_bdw_binary_image(struct image *image)
{
	const struct adsp *adsp = image->adsp;
	struct hsw_module_header *module;
	int ret, count;

	ret = read_bin_data(image);
	if (ret < 0)
		return ret;

	module = (void *)image->in_buffer;

	for (count = 0; count < image->num_modules; count++) {

		fprintf(stdout, "bin: module %d type %d blocks %d size 0x%x\n",
			count, module->type, module->blocks, module->mod_size);

		ret = read_bdw_bin_module(image, module);
		if (ret < 0) {
			fprintf(stderr, "invalid module %d\n", count);
			return ret;
		}
		module = (void *)module + sizeof(*module) + module->mod_size;
	}

	count = fwrite(image->out_buffer, adsp->image_size, 1,
		image->out_fd);
	if (count != adsp->image_size)
		return -errno;

	return ret;
}
