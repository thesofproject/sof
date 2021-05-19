// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef CDECL_H
#define CDECL_H

#ifdef __cplusplus

#define EXTERN_C_BEGIN                                                         \
    extern "C"                                                                 \
    {
#define EXTERN_C_END }

#else

#define EXTERN_C_BEGIN
#define EXTERN_C_END

#endif //__cplusplus

//! Check if assembler is running
#define ASM defined(_ASMLANGUAGE) || defined(__ASSEMBLER__)
//! Check if C/C++ compiler is running (not assembler)
#define NOT_ASM !defined(_ASMLANGUAGE) && !defined(__ASSEMBLER__)

#endif // CDECL_H
