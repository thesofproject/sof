/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

/* Dummy memory header for qemu_xtensa */

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

#define PLATFORM_DCACHE_ALIGN sizeof(void *)
#define HOST_PAGE_SIZE 4096
#define SHARED_DATA

#endif /* __PLATFORM_LIB_MEMORY_H__ */
