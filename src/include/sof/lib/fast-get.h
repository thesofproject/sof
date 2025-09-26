/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@linux.intel.com>
 */

#ifndef __SOF_LIB_FAST_GET_H__
#define __SOF_LIB_FAST_GET_H__

#include <stddef.h>

struct k_heap;
const void *fast_get(struct k_heap *heap, const void * const dram_ptr, size_t size);
void fast_put(struct k_heap *heap, const void *sram_ptr);

#endif /* __SOF_LIB_FAST_GET_H__ */
