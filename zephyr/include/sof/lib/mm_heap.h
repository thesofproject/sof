/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

/**
 * @file sof/lib/mm_heap.h
 * @brief Legacy XTOS heap compatibility header.
 * @ingroup memory_api
 * @deprecated Legacy XTOS heap management header. On Zephyr platforms, heap operations
 *             are managed by RTOS allocators in `<rtos/alloc.h>` and Zephyr k_heap;
 *             this header is retained solely as an empty compatibility shim.
 */

#ifndef __SOF_LIB_MM_HEAP_H__
#define __SOF_LIB_MM_HEAP_H__

/* no-op on Zephyr */

#endif /* __SOF_LIB_MM_HEAP_H__ */
