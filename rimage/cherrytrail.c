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

/* TODO: need to config CHT values */
static const struct section cht_sections[] = {
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

#define IRAM_OFFSET		0x0C0000
#define IRAM_SIZE		(80 * 1024)
#define DRAM_OFFSET		0x100000
#define DRAM_SIZE		(160 * 1024)

const struct adsp cht_machine = {
		.name = "cht",
		.iram_base = 0xff2c0000,
		.iram_size = 0x14000,
		.host_iram_offset = IRAM_OFFSET,
		.dram_base = 0xff300000,
		.dram_size = 0x28000,
		.host_dram_offset = DRAM_OFFSET,
		.machine_id = MACHINE_CHERRYTRAIL,
		.ops = {
				.write_header = byt_write_header,
				.write_modules = byt_write_modules,
		},
		.sections = cht_sections,
};

