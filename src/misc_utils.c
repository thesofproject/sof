// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2018-2023 Intel Corporation. All rights reserved.
 */

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
