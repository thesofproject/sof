/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

#ifndef __INCLUDE_ARCH_RELOC_SOF__
#define __INCLUDE_ARCH_RELOC_SOF__

#include <stdint.h>

struct sof;
struct elf32_file_hdr;

int arch_reloc_init(struct sof *sof);
int arch_elf_reloc_sections(struct elf32_file_hdr *hdr, struct sym_tab *sym_tab,
			    size_t size);

#endif
