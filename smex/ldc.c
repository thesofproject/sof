// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <kernel/abi.h>
#include <kernel/ext_manifest.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldc.h"
#include "smex.h"

static int fw_version_copy(const struct elf_module *src,
			   struct snd_sof_logs_header *header)
{
	struct ext_man_elem_header *ext_hdr = NULL;
	void *buffer = NULL;
	int section_size;

	section_size = elf_read_section(src, ".fw_ready", NULL, &buffer);

	if (section_size < 0)
		return section_size;

	memcpy(&header->version,
	       &((struct sof_ipc_fw_ready *)buffer)->version,
	       sizeof(header->version));
	free(buffer);

	/* fw_ready structure contains main (primarily kernel)
	 * ABI version.
	 */
	fprintf(stdout, "fw abi main version:\t%d.%d.%d\n",
		SOF_ABI_VERSION_MAJOR(header->version.abi_version),
		SOF_ABI_VERSION_MINOR(header->version.abi_version),
		SOF_ABI_VERSION_PATCH(header->version.abi_version));

	/* let's find dbg abi version, which the log client
	 * is interested in and override the kernel's one.
	 *
	 * skip the base fw-ready record and begin from the first extension.
	 */
	section_size = elf_read_section(src, ".fw_metadata", NULL, &buffer);

	if (section_size < 0)
		return section_size;

	ext_hdr = (struct ext_man_elem_header *)buffer;
	while ((uintptr_t)ext_hdr < (uintptr_t)buffer + section_size) {
		if (ext_hdr->type == EXT_MAN_ELEM_DBG_ABI) {
			header->version.abi_version =
				((struct ext_man_dbg_abi *)
						ext_hdr)->dbg_abi.abi_dbg_version;
			break;
		}
		//move to the next entry
		ext_hdr = (struct ext_man_elem_header *)
				((uint8_t *)ext_hdr + ext_hdr->elem_size);
	}
	free(buffer);

	fprintf(stdout, "fw abi dbg version:\t%d.%d.%d\n",
		SOF_ABI_VERSION_MAJOR(header->version.abi_version),
		SOF_ABI_VERSION_MINOR(header->version.abi_version),
		SOF_ABI_VERSION_PATCH(header->version.abi_version));

	return 0;
}

static int write_logs_dictionary(struct image *image,
				 const struct elf_module *src)
{
	struct snd_sof_logs_header header;
	const Elf32_Shdr *section;
	void *buffer = NULL;
	int count;
	int ret;

	memcpy(header.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE);
	header.data_offset = sizeof(struct snd_sof_logs_header);

	/* extract fw_version from fw_ready message located
	 * in .fw_ready section
	 */
	ret = fw_version_copy(src, &header);
	if (ret < 0)
		goto out;

	ret = elf_read_section(src, ".static_log_entries", &section, &buffer);
	if (ret < 0)
		goto out;

	ret = 0;
	header.base_address = section->vaddr;
	header.data_length = section->size;

	fwrite(&header, sizeof(struct snd_sof_logs_header), 1,
	       image->ldc_out_fd);

	count = fwrite(buffer, 1, section->size, image->ldc_out_fd);
	if (count != section->size) {
		fprintf(stderr,
			"error: can't write section %d\n",
			-errno);
		ret = -errno;
		goto out;
	}

	fprintf(stdout, "logs dictionary size:\t%u\n",
		header.data_length + header.data_offset);
	fprintf(stdout, "including fw version of size:\t%lu\n",
		(unsigned long)sizeof(header.version));
out:
	if (buffer)
		free(buffer);

	return ret;
}

static int write_uids_dictionary(struct image *image,
				 const struct elf_module *src)
{
	struct snd_sof_uids_header header;
	const Elf32_Shdr *section;
	void *buffer = NULL;
	int ret;

	memcpy(header.sig, SND_SOF_UIDS_SIG, SND_SOF_UIDS_SIG_SIZE);
	header.data_offset = sizeof(struct snd_sof_uids_header);

	ret = elf_read_section(src, ".static_uuid_entries", &section, &buffer);
	if (ret < 0)
		goto out;

	ret = 0;
	header.base_address = section->vaddr;
	header.data_length = section->size;

	fwrite(&header, sizeof(struct snd_sof_uids_header), 1,
	       image->ldc_out_fd);

	if (fwrite(buffer, 1, section->size, image->ldc_out_fd) !=
			section->size) {
		fprintf(stderr, "error: cant't write section %d\n",
			-errno);
		ret = -errno;
		goto out;
	}
	fprintf(stdout, "uids dictionary size:\t%u\n",
		header.data_length + header.data_offset);
out:
	if (buffer)
		free(buffer);
	return ret;
}

int write_dictionaries(struct image *image, const struct elf_module *src)
{
	int ret = 0;

	ret = write_logs_dictionary(image, src);
	if (ret)
		goto out;

	ret = write_uids_dictionary(image, src);

out:
	return ret;
}
