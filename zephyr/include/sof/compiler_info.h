/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifndef __SOF_COMPILER_INFO_H__
#define __SOF_COMPILER_INFO_H__

#if defined(__XCC_CLANG__)
/* Upstream LLVM clang targeting Xtensa; build system also sets -D__XCC__ */

#define CC_MAJOR __clang_major__
#define CC_MINOR __clang_minor__
#define CC_MICRO __clang_patchlevel__
#define CC_NAME "CLANG"
#define CC_DESC ""

#elif defined(__XCC__)
/* Cadence Xtensa toolchain (xt-cc or xt-clang) */

#include <xtensa/hal.h>

#define CC_MAJOR (XTHAL_RELEASE_MAJOR / 1000)
#define CC_MINOR ((XTHAL_RELEASE_MAJOR % 1000) / 10)
#define CC_MICRO XTHAL_RELEASE_MINOR
#define CC_NAME "XCC"
#define CC_DESC " " XCC_TOOLS_VERSION

#elif defined(__clang__)

#define CC_MAJOR __clang_major__
#define CC_MINOR __clang_minor__
#define CC_MICRO __clang_patchlevel__
#define CC_NAME "CLANG"
#define CC_DESC ""

#elif defined(__GNUC__)

#define CC_MAJOR __GNUC__
#define CC_MINOR __GNUC_MINOR__
#define CC_MICRO __GNUC_PATCHLEVEL__
#define CC_NAME "GCC"
#define CC_DESC ""

#if CC_MAJOR >= 10
#define CC_USE_LIBC
#endif

#else

#error "Unsupported toolchain."

#endif

#endif /* __SOF_COMPILER_INFO_H__ */
