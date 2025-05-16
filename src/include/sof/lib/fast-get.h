/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@linux.intel.com>
 */

#ifndef __SOF_LIB_FAST_GET_H__
#define __SOF_LIB_FAST_GET_H__

#include <stddef.h>

const void *fast_get(const void * const dram_ptr, size_t size);
void fast_put(const void *sram_ptr);

#endif /* __SOF_LIB_FAST_GET_H__ */
