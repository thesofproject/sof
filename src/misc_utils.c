// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2018-2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#include <stdio.h>
#include <rimage/misc_utils.h>

void bytes_swap(uint8_t *ptr, uint32_t size)
{
	uint8_t tmp;
	uint32_t index;

	for (index = 0; index < (size / 2); index++) {
		tmp = ptr[index];
		ptr[index] = ptr[size - 1 - index];
		ptr[size - 1 - index] = tmp;
	}
}

void print_enum(unsigned long value, const struct name_val *values)
{
	while (values->name) {
		if (values->value == value) {
			fprintf(stdout, "%s\n", values->name);
			return;
		}
		
		values++;
	}

	printf("Unknown: 0x%lx\n", value);
}

void print_flags(unsigned long value, const struct name_val *flags)
{
	while (flags->name) {
		if (value & flags->value) {
			fprintf(stdout, "%s ", flags->name);
			value &= ~flags->value;
		}

		flags++;
	}

	if (value)
		fprintf(stdout, "+ 0x%lx", value);
	printf("\n");
}
