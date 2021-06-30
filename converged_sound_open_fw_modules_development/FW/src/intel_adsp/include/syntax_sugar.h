// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef SYNTAX_SUGAR_H
#define SYNTAX_SUGAR_H

/*
Various helper macros which have primary role of making code more readable.
Do not encode anything platform specific here
*/
//! Given IP is present in HW
#define PRESENT(feature)                        CONFIG_ADSP_##feature##_PRESENT
//! Given IP is NOT in HW
#define NOT_PRESENT(feature)                    CONFIG_ADSP_##feature##_PRESENT == 0
#define SUPPORTED(feature) CONFIG_ADSP_##feature##_SUPPORT
#define NOT_SUPPORTED(feature) CONFIG_ADSP_##feature##_SUPPORT == 0
#define REQUIRED(feature) CONFIG_ADSP_##feature##_REQUIRED
#define NOT_REQUIRED(feature) CONFIG_ADSP_##feature##_REQUIRED == 0
#define WORKAROUND(feature) CONFIG_ADSP_##feature##_WORKAROUND
#define NOT_WORKAROUND(feature) CONFIG_ADSP_##feature##_WORKAROUND == 0
#define GET_QUANTITY(feature) CONFIG_ADSP_##feature##_QUANTITY
#define GET_SIZE(feature) CONFIG_ADSP_##feature##_SIZE
#define GET_VALUE(feature) CONFIG_ADSP_##feature##_VALUE
#define ADDRESS(feature) CONFIG_ADSP_##feature##_ADDRESS
#define REGISTER(feature) CONFIG_ADSP_##feature##_REGISTER
#define REGISTER_CSI(base, feature) REGISTER(feature)(base)
#define LOCATION(feature) CONFIG_ADSP_##feature##_LOCATION
#define VERSION(feature) CONFIG_ADSP_##feature##_VERSION
#define REVISION(feature) CONFIG_ADSP_##feature##_REVISION
#define REGISTER_PRESENT(reg_name) CONFIG_ADSP_##reg_name##_REGISTER_PRESENT
#define GET_UUID(feature) CONFIG_ADSP_##feature##_UUID

/********************************************************************
 *                       CONSTANT DEFINES
 *********************************************************************/
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *) 0)
#endif
#endif

#define KBYTES *1024
#define MBYTES *1024 KBYTES
/********************************************************************
 *             GENERAL PURPOSE MACRO-FUNCTIONS
 *********************************************************************/
//! Macro to silence the compiler whining about unused parameter
#ifndef UNUSED
#define UNUSED(x) (void) (x)
#endif

//! Macro that returns mask with selected bit set
#define BIT(bit_index) ((1 << (bit_index)))
#define COUNT_TO_BITMASK(cnt) (((0x1 << (cnt)) - 1))
#define NELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))
#define IS_BIT_CLEAR(value, idx) ((((value)) & (1 << (idx))) == 0)
#define IS_BIT_SET(value, idx) ((value) & (1 << (idx)))

//! Generates 32 bit mask from range.
/*!
  \param from  MSB bit to start from.
  \param to    LSB to finish.
*/
#define BITMASK_FROM_RANGE_UINT32(from, to) ((0xFFFFFFFF << (from)) & (0xFFFFFFFF >> (sizeof(uint32_t) * 8 - 1 - (to))))

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef ABS
#define ABS(X) (((X) > 0) ? (X) : (-(X)))
#endif

#define UINT32x2_TO_UINT64(u, l)    ((((uint64_t)(u))<<32)|((uint32_t)(l)))
#define UINT64_TO_UINT32x2(u64, ptr_u, ptr_l)                                 \
                        do {                                                \
                            *(ptr_u) = (uint32_t)((u64)>>32);                 \
                            *(ptr_l) = (uint32_t)(u64);                       \
                        }while(0)

#define IS_ALIGNED(address, aligment) (((uintptr_t)(address)) % (aligment) == 0)
#define IS_ALIGNED_TO_OWORD(n) ((n & 0xf) ? false : true)
#define IS_ALIGNED_TO_QWORD(n) ((n & 0x7) ? false : true)
#define IS_ALIGNED_TO_DWORD(n) ((n & 0x3) ? false : true)

#define BYTE_SWAP16(x) (((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00))

#ifndef CONCAT
#define CAT2(x, y) x##y
#define CONCAT(x, y) CAT2(x, y)
#endif

#define QUOT(x) #x
#define QUOTE(x) QUOT(x)

#ifndef C_CAST
#define C_CAST(type, value) ((type)(value))
#endif // C_CAST

#define ANONYMOUS_NAME(name) CONCAT(name, __LINE__)

// TODO: Add commentary + usage for those macros:
// THIS IS BROKEN FOR 0 ARGS
#define __NARG__(...) __NARG_I_("dummy", ##__VA_ARGS__, __RSEQ_N())
#define __NARG_I_(...) __ARG_N(__VA_ARGS__)
#define __ARG_N(_0, _1, _2, _3, _4, _5, _6, _7, _8, N, ...) N

#define __RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0

/********************************************************************
 *             MEMORY MACRO-FUNCTIONS
 *********************************************************************/

#ifndef ROUND_UP
#define ROUND_UP(size, alignment)                                              \
    (((size) % (alignment) == 0)                                               \
         ? (size)                                                              \
         : ((size) - ((size) % (alignment)) + (alignment)))
#endif
#ifndef ROUND_DOWN
#define ROUND_DOWN(size, alignment) ((size) - ((size) % (alignment)))
#endif

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#define ___in_section(a, b, c)                                                 \
    __attribute__(                                                             \
        (section("." _STRINGIFY(a) "." _STRINGIFY(b) "." _STRINGIFY(c))))

#define __in_section(a, b, c) ___in_section(a, b, c)

#define __in_section_unique(seg) ___in_section(seg, __FILE__, __COUNTER__)

#ifndef __aligned
#define __aligned(x) __attribute__((__aligned__(x)))
#endif

#define SECTIONATT(sect_name) __attribute__((section(sect_name)))
#define BSS_SECTION SECTIONATT(".bss")

/********************************************************************
 *             COMPILER DIRECTIVE MACROS
 *********************************************************************/
#define LIKELY(condition) __builtin_expect((condition), 1)
#define UNLIKELY(condition) __builtin_expect((condition), 0)

#define _STRINGIFY(x) #x
#define STRINGIFY(s) _STRINGIFY(s)

#ifndef PURE
#define PURE __attribute__((pure))
#endif

#define FORCE_INLINE __attribute__((always_inline))
#define FORCE_NO_INLINE __attribute__((noinline))
#define NO_OPTIMIZE __attribute__((optimize("-O0")))

#ifndef C_ASSERT
#ifdef __cplusplus
#define C_ASSERT(e) typedef char __C_ASSERT__[(e) ? 1 : -1]
#else
#if __clang__
#define C_ASSERT(e) _Static_assert((e), #e)
#else
#define C_ASSERT(e) extern char(*ct_assert(void))[sizeof(char[1 - 2 * !(e)])]
#endif
#endif
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member)                                        \
    ((type *) ((char *) (ptr) - (char *) &((type *) 0)->member))
#endif

#if !defined(__cplusplus) && !defined(offsetof)
#define offsetof(st, m) __builtin_offsetof(st, m)
#endif

//Wrap 'static' in #define to allow override for symbol/function globalization
#if !defined(UT)
#define PRIVATE static
#define STATIC static
#define INLINE inline

#else
#define PRIVATE
#define STATIC
#define INLINE
#endif

#define C_CAST(type, value) ((type)(value))

#if defined(_MSC_VER)
#define PACKED
#else
#define PACKED __attribute__((__packed__))
#endif

//! Returns protected region size in kB.
/*!
  \param value protected region size is calculated form value (e.g. IMR_PROTECTION_128_KB_SIZE)
*/
#define PROTECTED_REGION_SIZE(value) (BIT(value) * 0x1000)

#define DEFINE_UUID_(name, l1, s1, s2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const uint32_t name[] = \
    { \
        l1, \
        s2 << 16 | s1, \
        b4 << 24 | b3 << 16 | b2 << 8 | b1, \
        b8 << 24 | b7 << 16 | b6 << 8 | b5, \
    }
#define DEFINE_UUID(name, ...) DEFINE_UUID_(name, __VA_ARGS__)
#define DEFINE_UUID_INLINE_(l1, s1, s2, b1, b2, b3, b4, b5, b6, b7, b8) \
    l1, \
    s2 << 16 | s1, \
    b4 << 24 | b3 << 16 | b2 << 8 | b1, \
    b8 << 24 | b7 << 16 | b6 << 8 | b5
#define DEFINE_UUID_INLINE(...) DEFINE_UUID_INLINE_(__VA_ARGS__)

#if defined(__KLOCWORK__)
#include "misc/klocwork_overrides.h"
#endif

#if !(defined(ULP_FW) && defined(UT)) //if not UT for ULP, where call mockups are used
#define __syscall(...) static inline
#else
#define __syscall(...)
#endif

#endif // SYNTAX_SUGAR_H
