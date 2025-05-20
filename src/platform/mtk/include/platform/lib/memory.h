/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef _SOF_PLATFORM_MTK_LIB_MEMORY_H
#define _SOF_PLATFORM_MTK_LIB_MEMORY_H

#include <xtensa/config/core-isa.h>
#include <ipc/info.h>

#define PLATFORM_DCACHE_ALIGN 128

/* Sigh, too many ways to get this wrong... */
BUILD_ASSERT(PLATFORM_DCACHE_ALIGN == XCHAL_DCACHE_LINESIZE);

#define uncache_to_cache(addr) (addr)
#define cache_to_uncache(addr) (addr)

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

#define host_to_local(addr) (addr)

#define PLATFORM_HEAP_SYSTEM 1
#define PLATFORM_HEAP_SYSTEM_RUNTIME 1
#define PLATFORM_HEAP_RUNTIME 1
#define PLATFORM_HEAP_BUFFER 1

#define SHARED_DATA /* no special section attribute needed */

/* Mailbox window addresses for the rimage extended manifest.  The
 * struct is optimized out in generated code, it's just here to be a
 * little clearer than the pages of #defines used traditionally.
 *
 * 8195 puts the window region at 8M into the DRAM memory space,
 * everything else at 5M.  Note that these are linkable addresses!
 * There's nothing preventing a symbol from ending up here except the
 * fact that SOF isn't (remotely) that big.  Long term we should move
 * this stuff into regular .bss/.noinit symbols, but that requires
 * validation that the kernel driver interprets the manifest
 * correctly.  Right now we're using the historical addresses.
 */
#if defined(CONFIG_SOC_MT8195) || defined(CONFIG_SOC_MT8365)
#define MTK_IPC_BASE (DT_REG_ADDR(DT_NODELABEL(dram0)) + 0x800000)
#else
#define MTK_IPC_BASE (DT_REG_ADDR(DT_NODELABEL(dram0)) + 0x500000)
#endif

/* Beware: the first two buffers are variously labelled UP/DOWN OUT/IN
 * and DSP/HOST, and the correspondance isn't as clear as one would
 * want.
 */
#define _MTK_WIN_SZ_K_UPBOX     4
#define _MTK_WIN_SZ_K_DOWNBOX   4
#define _MTK_WIN_SZ_K_DEBUG     2
#define _MTK_WIN_SZ_K_EXCEPTION 2
#define _MTK_WIN_SZ_K_STREAM    4
#define _MTK_WIN_SZ_K_TRACE     4

#define _MTK_WIN_OFF_K_UPBOX     0
#define _MTK_WIN_OFF_K_DOWNBOX   (_MTK_WIN_SZ_K_UPBOX)
#define _MTK_WIN_OFF_K_DEBUG     (_MTK_WIN_SZ_K_DOWNBOX   + _MTK_WIN_OFF_K_DOWNBOX)
#define _MTK_WIN_OFF_K_EXCEPTION (_MTK_WIN_SZ_K_DEBUG     + _MTK_WIN_OFF_K_DEBUG)
#define _MTK_WIN_OFF_K_STREAM    (_MTK_WIN_SZ_K_EXCEPTION + _MTK_WIN_OFF_K_EXCEPTION)
#define _MTK_WIN_OFF_K_TRACE     (_MTK_WIN_SZ_K_STREAM    + _MTK_WIN_OFF_K_STREAM)

#define MTK_IPC_WIN_OFF(reg)  (1024 * _MTK_WIN_OFF_K_##reg)
#define MTK_IPC_WIN_SIZE(reg) (1024 * _MTK_WIN_SZ_K_##reg)
#define MTK_IPC_WIN_BASE(reg) (MTK_IPC_BASE + MTK_IPC_WIN_OFF(reg))

#endif /* _SOF_PLATFORM_MTK_LIB_MEMORY_H */
