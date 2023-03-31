/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __MISC_UTILS_H__
#define __MISC_UTILS_H__

#include <stdint.h>

#define DIV_ROUND_UP(val, div) (((val) + (div) - 1) / (div))

/**
 * Reverses the order of bytes in the array
 * @param ptr pointer to a array
 * @param size of the array
 */
void bytes_swap(uint8_t *ptr, uint32_t size);

struct name_val {
	const char *name;
	unsigned long value;
};

#define NAME_VAL_ENTRY(x) { .name = #x, .value = x }
#define NAME_VAL_END { .name = 0, .value = 0 }

void print_enum(unsigned long value, const struct name_val *values);
void print_flags(unsigned long value, const struct name_val *flags);

#endif /* __MISC_UTILS_H__ */
