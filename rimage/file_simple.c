// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2015 Intel Corporation. All rights reserved.

#include <kernel/abi.h>
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

#define IMX8_IRAM_BASE		0x596f8000
#define IMX8_IRAM_HOST_OFFSET	0x10000
#define IMX8_IRAM_SIZE		0x800
#define IMX8_DRAM_BASE		0x596e8000
#define IMX8_DRAM_SIZE		0x8000
#define IMX8_SRAM_BASE		0x92400000
#define IMX8_SRAM_SIZE		0x800000

#define IMX8M_IRAM_BASE		0x3b6f8000
#define IMX8M_IRAM_HOST_OFFSET	0x10000
#define IMX8M_IRAM_SIZE		0x800
#define IMX8M_DRAM_BASE		0x3b6e8000
#define IMX8M_DRAM_SIZE		0x8000
#define IMX8M_SRAM_BASE		0x92400000
#define IMX8M_SRAM_SIZE		0x800000

static int get_mem_zone_type(struct image *image, Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	uint32_t start, end, base, size;
	int i;

	start = section->vaddr;
	end = section->vaddr + section->size;

	for (i = SOF_FW_BLK_TYPE_START; i < SOF_FW_BLK_TYPE_NUM; i++) {
		base =  adsp->mem_zones[i].base;
		size =  adsp->mem_zones[i].size;

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

static int fw_version_copy(struct snd_sof_logs_header *header,
			   const struct module *module)
{
	Elf32_Shdr *section = NULL;
	struct sof_ipc_ext_data_hdr *ext_hdr = NULL;
	void *buffer = NULL;

	if (module->fw_ready_index <= 0)
		return 0;

	section = &module->section[module->fw_ready_index];

	buffer = calloc(1, section->size);
	if (!buffer)
		return -ENOMEM;

	fseek(module->fd, section->off, SEEK_SET);
	size_t count = fread(buffer, 1,
		section->size, module->fd);

	if (count != section->size) {
		fprintf(stderr, "error: can't read ready section %d\n", -errno);
		free(buffer);
		return -errno;
	}

	memcpy(&header->version,
	       &((struct sof_ipc_fw_ready *)buffer)->version,
	       sizeof(header->version));

	/* fw_ready structure contains main (primarily kernel)
	 * ABI version.
	 */

	fprintf(stdout, "fw abi main version: %d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(header->version.abi_version),
		SOF_ABI_VERSION_MINOR(header->version.abi_version),
		SOF_ABI_VERSION_PATCH(header->version.abi_version));

	/* let's find dbg abi version, which the log client
	 * is interested in and override the kernel's one.
	 *
	 * skip the base fw-ready record and begin from the first extension.
	 */
	ext_hdr = buffer + ((struct sof_ipc_fw_ready *)buffer)->hdr.size;
	while ((uintptr_t)ext_hdr < (uintptr_t)buffer + section->size) {
		if (ext_hdr->type == SOF_IPC_EXT_USER_ABI_INFO) {
			header->version.abi_version =
				((struct sof_ipc_user_abi_version *)
						ext_hdr)->abi_dbg_version;
			break;
		}
		//move to the next entry
		ext_hdr = (struct sof_ipc_ext_data_hdr *)
				((uint8_t *)ext_hdr + ext_hdr->hdr.size);
	}

	fprintf(stdout, "fw abi dbg version: %d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(header->version.abi_version),
		SOF_ABI_VERSION_MINOR(header->version.abi_version),
		SOF_ABI_VERSION_PATCH(header->version.abi_version));

	free(buffer);

	return 0;
}

static int block_idx;

static int write_block(struct image *image, struct module *module,
		       Elf32_Shdr *section)
{
	const struct adsp *adsp = image->adsp;
	struct snd_sof_blk_hdr block;
	uint32_t padding = 0;
	size_t count;
	void *buffer;
	int ret;

	block.size = section->size;
	if (block.size % 4) {
		/* make block.size divisible by 4 to avoid unaligned accesses */
		padding = 4 - (block.size % 4);
		block.size += padding;
	}

	ret = get_mem_zone_type(image, section);
	if (ret != SOF_FW_BLK_TYPE_INVALID) {
		block.type = ret;
		block.offset = section->vaddr - adsp->mem_zones[ret].base
			+ adsp->mem_zones[ret].host_offset;
	} else {
		fprintf(stderr, "error: invalid block address/size 0x%x/0x%x\n",
			section->vaddr, section->size);
		return -EINVAL;
	}

	/* write header */
	count = fwrite(&block, sizeof(block), 1, image->out_fd);
	if (count != 1)
		return -errno;

	/* alloc data data */
	buffer = calloc(1, section->size);
	if (!buffer)
		return -ENOMEM;

	/* read in section data */
	ret = fseek(module->fd, section->off, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: cant seek to section %d\n", ret);
		goto out;
	}
	count = fread(buffer, 1, section->size, module->fd);
	if (count != section->size) {
		fprintf(stderr, "error: cant read section %d\n", -errno);
		ret = -errno;
		goto out;
	}

	/* write out section data */
	count = fwrite(buffer, 1, block.size, image->out_fd);
	if (count != block.size) {
		fprintf(stderr, "error: cant write section %d\n", -errno);
		fprintf(stderr, " foffset %d size 0x%x mem addr 0x%x\n",
			section->off, section->size, section->vaddr);
		ret = -errno;
		goto out;
	}

	fprintf(stdout, "\t%d\t0x%8.8x\t0x%8.8x\t0x%8.8lx\t%s\n", block_idx++,
		section->vaddr, section->size, ftell(image->out_fd),
		block.type == SOF_FW_BLK_TYPE_IRAM ? "TEXT" : "DATA");

out:
	free(buffer);
	/* return padding size */
	if (ret >= 0)
		return padding;

	return ret;
}

static int simple_write_module(struct image *image, struct module *module)
{
	struct snd_sof_mod_hdr hdr;
	Elf32_Shdr *section;
	size_t count;
	int i, err;
	uint32_t valid = (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR);
	int ptr_hdr, ptr_cur;
	uint32_t padding = 0;

	hdr.num_blocks = module->num_sections - module->num_bss;
	hdr.size = module->text_size + module->data_size +
		sizeof(struct snd_sof_blk_hdr) * hdr.num_blocks;
	hdr.type = SOF_FW_BASE;

	/* Get the pointer of writing hdr */
	ptr_hdr = ftell(image->out_fd);
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

	for (i = 0; i < module->hdr.shnum; i++) {
		section = &module->section[i];

		/* only write valid sections */
		if (!(module->section[i].flags & valid))
			continue;

		/* dont write bss */
		if (section->type == SHT_NOBITS)
			continue;

		err = write_block(image, module, section);
		if (err < 0) {
			fprintf(stderr, "error: failed to write section #%d\n",
				i);
			return err;
		}
		/* write_block will return padding size */
		padding += err;
	}
	hdr.size += padding;
	/* Record current pointer, will set it back after overwriting hdr */
	ptr_cur = ftell(image->out_fd);
	/* overwrite hdr */
	fseek(image->out_fd, ptr_hdr, SEEK_SET);
	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1) {
		fprintf(stderr, "error: failed to write section header %d\n",
			-errno);
		return -errno;
	}
	fseek(image->out_fd, ptr_cur, SEEK_SET);

	fprintf(stdout, "\n");
	/* return padding size */
	return padding;
}

static int write_block_reloc(struct image *image, struct module *module)
{
	struct snd_sof_blk_hdr block;
	size_t count;
	void *buffer;
	int ret;

	block.size = module->file_size;
	block.type = SOF_FW_BLK_TYPE_DRAM;
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
		block.type == SOF_FW_BLK_TYPE_IRAM ? "TEXT" : "DATA");

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
		module->fw_size += sizeof(struct snd_sof_mod_hdr) *
				hdr.num_modules;
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
		/* add padding size */
		hdr.file_size += ret;
	}
	/* overwrite hdr */
	fseek(image->out_fd, 0, SEEK_SET);
	count = fwrite(&hdr, sizeof(hdr), 1, image->out_fd);
	if (count != 1)
		return -errno;

	fprintf(stdout, "firmware: image size %ld (0x%lx) bytes %d modules\n\n",
		(long)(hdr.file_size + sizeof(hdr)),
		(long)(hdr.file_size + sizeof(hdr)),
		hdr.num_modules);

	return 0;
}

static int write_logs_dictionary(struct image *image)
{
	struct snd_sof_logs_header header;
	int i, ret = 0;
	void *buffer = NULL;

	memcpy(header.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE);
	header.data_offset = sizeof(struct snd_sof_logs_header);

	for (i = 0; i < image->num_modules; i++) {
		struct module *module = &image->module[i];

		/* extract fw_version from fw_ready message located
		 * in .fw_ready section
		 */
		ret = fw_version_copy(&header, module);
		if (ret < 0)
			goto out;

		if (module->logs_index > 0) {
			Elf32_Shdr *section =
				&module->section[module->logs_index];

			header.base_address = section->vaddr;
			header.data_length = section->size;

			fwrite(&header, sizeof(struct snd_sof_logs_header), 1,
			       image->ldc_out_fd);

			buffer = calloc(1, section->size);
			if (!buffer)
				return -ENOMEM;

			fseek(module->fd, section->off, SEEK_SET);
			size_t count = fread(buffer, 1, section->size,
				module->fd);
			if (count != section->size) {
				fprintf(stderr,
					"error: can't read logs section %d\n",
					-errno);
				ret = -errno;
				goto out;
			}
			count = fwrite(buffer, 1, section->size,
				       image->ldc_out_fd);
			if (count != section->size) {
				fprintf(stderr,
					"error: can't write section %d\n",
					-errno);
				ret = -errno;
				goto out;
			}

			fprintf(stdout, "logs dictionary: size %u\n",
				header.data_length + header.data_offset);
			fprintf(stdout, "including fw version of size: %lu\n",
				(unsigned long)sizeof(header.version));
		}
	}
out:
	if (buffer)
		free(buffer);

	return ret;
}

static int write_uids_dictionary(struct image *image)
{
	struct snd_sof_uids_header header;
	Elf32_Shdr *section;
	int i, ret = 0;
	void *buffer = NULL;

	memcpy(header.sig, SND_SOF_UIDS_SIG, SND_SOF_UIDS_SIG_SIZE);
	header.data_offset = sizeof(struct snd_sof_uids_header);

	for (i = 0; i < image->num_modules; i++) {
		struct module *module = &image->module[i];

		if (module->uids_index <= 0)
			continue;
		section = &module->section[module->uids_index];

		header.base_address = section->vaddr;
		header.data_length = section->size;

		fwrite(&header, sizeof(struct snd_sof_uids_header), 1,
		       image->ldc_out_fd);

		buffer = calloc(1, section->size);
		if (!buffer)
			return -ENOMEM;
		fseek(module->fd, section->off, SEEK_SET);
		if (fread(buffer, 1, section->size, module->fd) !=
				section->size) {
			fprintf(stderr, "error: can't read uids section %d\n",
				-errno);
			ret = -errno;
			goto out;
		}
		if (fwrite(buffer, 1, section->size, image->ldc_out_fd) !=
				section->size) {
			fprintf(stderr, "error: cant't write section %d\n",
				-errno);
			ret = -errno;
			goto out;
		}
		fprintf(stdout, "uids dictionary: size %u\n",
			header.data_length + header.data_offset);
	}
out:
	free(buffer);
	return ret;
}

int write_dictionaries(struct image *image)
{
	int ret = 0;

	ret = write_logs_dictionary(image);
	if (ret)
		goto out;

	ret = write_uids_dictionary(image);

out:
	return ret;
}

const struct adsp machine_byt = {
	.name = "byt",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = BYT_IRAM_BASE,
			.size = BYT_IRAM_SIZE,
			.host_offset = BYT_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = BYT_DRAM_BASE,
			.size = BYT_DRAM_SIZE,
			.host_offset = BYT_DRAM_HOST_OFFSET,
		},
	},
	.machine_id = MACHINE_BAYTRAIL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_cht = {
	.name = "cht",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = BYT_IRAM_BASE,
			.size = BYT_IRAM_SIZE,
			.host_offset = BYT_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = BYT_DRAM_BASE,
			.size = BYT_DRAM_SIZE,
			.host_offset = BYT_DRAM_HOST_OFFSET,
		},
	},
	.machine_id = MACHINE_CHERRYTRAIL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_bsw = {
	.name = "bsw",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = BYT_IRAM_BASE,
			.size = BYT_IRAM_SIZE,
			.host_offset = BYT_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = BYT_DRAM_BASE,
			.size = BYT_DRAM_SIZE,
			.host_offset = BYT_DRAM_HOST_OFFSET,
		},
	},
	.machine_id = MACHINE_BRASWELL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_hsw = {
	.name = "hsw",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = HSW_IRAM_BASE,
			.size = HSW_IRAM_SIZE,
			.host_offset = HSW_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = HSW_DRAM_BASE,
			.size = HSW_DRAM_SIZE,
			.host_offset = HSW_DRAM_HOST_OFFSET,
		},
	},
	.machine_id = MACHINE_HASWELL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_bdw = {
	.name = "bdw",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = BDW_IRAM_BASE,
			.size = BDW_IRAM_SIZE,
			.host_offset = BDW_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = BDW_DRAM_BASE,
			.size = BDW_DRAM_SIZE,
			.host_offset = BDW_DRAM_HOST_OFFSET,
		},
	},
	.machine_id = MACHINE_BROADWELL,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_imx8 = {
	.name = "imx8",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = IMX8_IRAM_BASE,
			.size = IMX8_IRAM_SIZE,
			.host_offset = IMX8_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = IMX8_DRAM_BASE,
			.size = IMX8_DRAM_SIZE,
			.host_offset = 0,
		},
		[SOF_FW_BLK_TYPE_SRAM] = {
			.base = IMX8_SRAM_BASE,
			.size = IMX8_SRAM_SIZE,
			.host_offset = 0,
		},
	},
	.machine_id = MACHINE_IMX8,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_imx8x = {
	.name = "imx8x",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = IMX8_IRAM_BASE,
			.size = IMX8_IRAM_SIZE,
			.host_offset = IMX8_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = IMX8_DRAM_BASE,
			.size = IMX8_DRAM_SIZE,
			.host_offset = 0,
		},
		[SOF_FW_BLK_TYPE_SRAM] = {
			.base = IMX8_SRAM_BASE,
			.size = IMX8_SRAM_SIZE,
			.host_offset = 0,
		},
	},
	.machine_id = MACHINE_IMX8X,
	.write_firmware = simple_write_firmware,
};

const struct adsp machine_imx8m = {
	.name = "imx8m",
	.mem_zones = {
		[SOF_FW_BLK_TYPE_IRAM] = {
			.base = IMX8M_IRAM_BASE,
			.size = IMX8M_IRAM_SIZE,
			.host_offset = IMX8M_IRAM_HOST_OFFSET,
		},
		[SOF_FW_BLK_TYPE_DRAM] = {
			.base = IMX8M_DRAM_BASE,
			.size = IMX8M_DRAM_SIZE,
			.host_offset = 0,
		},
		[SOF_FW_BLK_TYPE_SRAM] = {
			.base = IMX8M_SRAM_BASE,
			.size = IMX8M_SRAM_SIZE,
			.host_offset = 0,
		},
	},
	.machine_id = MACHINE_IMX8M,
	.write_firmware = simple_write_firmware,
};
