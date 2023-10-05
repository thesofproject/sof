// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2015 Intel Corporation. All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <rimage/rimage.h>
#include <rimage/manifest.h>
#include <rimage/file_utils.h>

static int get_mem_zone_type(const struct memory_config *memory,
			     const struct module_section *section)
{
	uint32_t start, end, base, size;
	int i;

	start = section->load_address;
	end = start + section->size;

	for (i = SOF_FW_BLK_TYPE_START; i < SOF_FW_BLK_TYPE_NUM; i++) {
		base = memory->zones[i].base;
		size = memory->zones[i].size;

		if (start < base)
			continue;
		if (start >= base + size)
			continue;
		if (end > base + size)
			continue;
		return i;
	}
	return SOF_FW_BLK_TYPE_INVALID;
}

static int block_idx;

static int write_block(struct image *image, struct manifest_module *module,
		       const struct module_section *section)
{
	const struct adsp *adsp = image->adsp;
	struct snd_sof_blk_hdr block;
	uint32_t padding = 0;
	size_t count;
	int ret;

	block.size = section->size;
	if (block.size % 4) {
		/* make block.size divisible by 4 to avoid unaligned accesses */
		padding = 4 - (block.size % 4);
		block.size += padding;
	}

	ret = get_mem_zone_type(&adsp->mem, section);
	if (ret != SOF_FW_BLK_TYPE_INVALID) {
		block.type = ret;
		block.offset = section->load_address - adsp->mem.zones[ret].base
			+ adsp->mem.zones[ret].host_offset;
	} else {
		fprintf(stderr, "error: invalid block address/size 0x%x/0x%zx\n",
			section->load_address, section->size);
		return -EINVAL;
	}

	/* write header */
	count = fwrite(&block, sizeof(block), 1, image->out_fd);
	if (count != 1)
		return file_error("Write header failed", image->out_file);

	/* write out section data */
	ret = module_write_section(&module->file, section, padding, image->out_fd, image->out_file);
	if (ret) {
		fprintf(stderr, "error: cant write section data. foffset %d size 0x%zx mem addr 0x%x\n",
			section->header->data.off, section->size, section->load_address);
		return ret;
	}

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8zx\t0x%8.8lx\t%s\t%s\n", block_idx++,
		section->load_address, section->size, ftell(image->out_fd),
		block.type == SOF_FW_BLK_TYPE_IRAM ? "TEXT" : "DATA",
		section->header->name);

	/* return padding size */
	if (ret >= 0)
		return padding;

	return ret;
}

/**
 * Write all linked sections
 * 
 * @param image program global structure
 * @param module modules manifest description
 * @param section module section descriptor
 * @return size of used padding, error code on error
 */
static int write_blocks(struct image *image, struct manifest_module *module,
			const struct module_section *section)
{
	int ret, padding = 0;

	while (section) {
		ret = write_block(image, module, section);
		if (ret < 0) {
			fprintf(stderr, "error: failed to write section %s\n",
				section->header->name);
			return ret;
		}

		padding += ret;
		section = section->next_section;
	}

	return padding;
}

static int simple_write_module(struct image *image, struct manifest_module *module)
{
	struct snd_sof_mod_hdr hdr;
	size_t count;
	int err;
	int ptr_hdr, ptr_cur;
	uint32_t padding;

	hdr.num_blocks = module->file.text.count + module->file.data.count;
	hdr.size = module->file.text.size + module->file.data.size +
		sizeof(struct snd_sof_blk_hdr) * hdr.num_blocks;
	hdr.type = SOF_FW_BASE;

	/* Get the pointer of writing hdr */
	ptr_hdr = ftell(image->out_fd);
	if (ptr_hdr < 0)
		return file_error("cant get file position", image->out_file);

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return file_error("failed to write section header", image->out_file);

	module_print_zones(&module->file);

	fprintf(stdout, "\tNo\tAddress\t\tSize\t\tFile\t\tType\tName\n");

	/* Write text sections */
	err = write_blocks(image, module, module->file.text.first_section);
	if (err < 0)
		return err;
	padding = err;

	/* Write data sections */
	err = write_blocks(image, module, module->file.data.first_section);
	if (err < 0)
			return err;
		padding += err;

	hdr.size += padding;
	/* Record current pointer, will set it back after overwriting hdr */
	ptr_cur = ftell(image->out_fd);
	if (ptr_cur < 0)
		return file_error("cant get file position", image->out_file);

	/* overwrite hdr */
	err = fseek(image->out_fd, ptr_hdr, SEEK_SET);
	if (err)
		return file_error("cant seek to header", image->out_file);

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return file_error("failed to write section header", image->out_file);

	err = fseek(image->out_fd, ptr_cur, SEEK_SET);
	if (err)
		return file_error("cant seek", image->out_file);

	fprintf(stdout, "\n");
	/* return padding size */
	return padding;
}

static int write_block_reloc(struct image *image, struct manifest_module *module)
{
	struct snd_sof_blk_hdr block;
	size_t count;
	int ret;

	block.size = module->file.elf.file_size;
	block.type = SOF_FW_BLK_TYPE_DRAM;
	block.offset = 0;

	/* write header */
	count = fwrite(&block, sizeof(block), 1, image->out_fd);
	if (count != 1)
		return file_error("cant write header", image->out_file);

	ret = module_write_whole_elf(&module->file, image->out_fd, image->out_file);
	if (ret)
		return ret;

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8zx\t0x%8.8lx\t%s\n", block_idx++,
		0, module->file.elf.file_size, ftell(image->out_fd),
		block.type == SOF_FW_BLK_TYPE_IRAM ? "TEXT" : "DATA");

	return ret;
}

static int simple_write_module_reloc(struct image *image, struct manifest_module *module)
{
	struct snd_sof_mod_hdr hdr;
	size_t count;
	int err;

	hdr.num_blocks = 1;
	hdr.size = module->file.text.size + module->file.data.size;
	hdr.type = SOF_FW_BASE; // module

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return file_error("failed to write section header", image->out_file);

	module_print_zones(&module->file);

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
int simple_write_firmware(struct image *image)
{
	struct snd_sof_fw_header hdr;
	struct manifest_module *module;
	size_t count;
	int i, ret;

	memcpy(hdr.sig, SND_SOF_FW_SIG, SND_SOF_FW_SIG_SIZE);

	hdr.num_modules = image->num_modules;
	hdr.abi = SND_SOF_FW_ABI;
	hdr.file_size = 0;

	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];
		module->output_size = module->file.data.size + module->file.text.size;
		module->output_size += sizeof(struct snd_sof_blk_hdr) *
				(module->file.data.count + module->file.text.count);
		module->output_size += sizeof(struct snd_sof_mod_hdr) *
				hdr.num_modules;
		hdr.file_size += module->output_size;
	}

	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return file_error("failed to write header", image->out_file);

	for (i = 0; i < image->num_modules; i++) {
		module = &image->module[i];

		fprintf(stdout, "writing module %d %s\n", i, module->file.elf.filename);

		if (image->reloc)
			ret = simple_write_module_reloc(image, module);
		else
			ret = simple_write_module(image, module);
		if (ret < 0) {
			fprintf(stderr, "error: failed to write module %d\n",
				i);
			return ret;
		}
		/* add padding size */
		hdr.file_size += ret;
	}
	/* overwrite hdr */
	ret = fseek(image->out_fd, 0, SEEK_SET);
	if (ret)
		return file_error("can't seek set", image->out_file);


	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return file_error("failed to write header", image->out_file);

	fprintf(stdout, "firmware: image size %ld (0x%lx) bytes %d modules\n\n",
		(long)(hdr.file_size + sizeof(hdr)),
		(long)(hdr.file_size + sizeof(hdr)),
		hdr.num_modules);

	return 0;
}
