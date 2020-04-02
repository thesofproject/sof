// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "elf.h"

/*
 * Firmware image context.
 */
struct image {
	const char *ldc_out_file;
	FILE *ldc_out_fd;

	bool verbose;
	struct elf_module module;
};
