/*
 * xtensa-types.h  --  General type definitions and macros.
 */

/*
 * Copyright (c) 2002-2018 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef XTENSA_TYPES_H
#define XTENSA_TYPES_H

/*  parasoft-begin-suppress MISRA2012-RULE-20_7 "Cannot parenthesize macro args here" */
#if defined (_ASMLANGUAGE) || defined (__ASSEMBLER__)

/* Redefine stdint.h macros for assembler. */
/*  parasoft-begin-suppress MISRA2012-RULE-21_1_c "It is necessary to redefine these to enable assembly code to compile." */
#define INT8_C(x)	x
#define UINT8_C(x)	x
#define INT16_C(x)	x
#define UINT16_C(x)	x
#define INT32_C(x)	x
#define UINT32_C(x)	x
#define INT64_C(x)	x
#define UINT64_C(x)	x
/*  parasoft-end-suppress MISRA2012-RULE-21_1_c "It is necessary to redefine these to enable assembly code to compile." */

#else

#include <stdint.h>

/* Adapt inline spec for older and newer versions of C standard. */
#if (__STDC_VERSION__ >= 199901L) || defined (__cplusplus)
#define XT_INLINE	__attribute__((always_inline)) static inline
#else
#define XT_INLINE	__attribute__((always_inline)) static __inline__
#endif

/* This macro is used to mark unused function parameters and suppress warnings
   from compiler / MISRA checker.
 */
#define UNUSED(x)       ((void)(x))

#endif
/*  parasoft-end-suppress MISRA2012-RULE-20_7 "Cannot parenthesize macro args here" */

#endif // XTENSA_TYPES_H
