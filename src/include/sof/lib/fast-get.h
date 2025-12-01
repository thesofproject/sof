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

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
#include <zephyr/toolchain.h>

__syscall const void *fast_get(struct k_heap *heap, const void * const dram_ptr, size_t size);
__syscall void fast_put(struct k_heap *heap, const void *sram_ptr);
#include <zephyr/syscalls/fast-get.h>
#else
const void *z_impl_fast_get(struct k_heap *heap, const void * const dram_ptr, size_t size);
void z_impl_fast_put(struct k_heap *heap, const void *sram_ptr);
#define fast_get z_impl_fast_get
#define fast_put z_impl_fast_put
#endif /* __ZEPHYR__ */

#endif /* __SOF_LIB_FAST_GET_H__ */
