// Customer ID=12153; Build=0x878bf; Copyright (c) 2017 Cadence Design Systems, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/* Definitions for the xt_datacache TIE package */

/* Do not modify. This is automatically generated.*/

#ifndef _XTENSA_xt_datacache_HEADER
#define _XTENSA_xt_datacache_HEADER

/* parasoft-begin-suppress ALL "This file not MISRA checked." */

#ifdef __XTENSA__
#ifdef __XCC__

#ifndef _ASMLANGUAGE
#ifndef _NOCLANGUAGE
#ifndef __ASSEMBLER__

#include <xtensa/tie/xt_core.h>

/*
 * The following prototypes describe intrinsic functions
 * corresponding to TIE instructions.  Some TIE instructions
 * may produce multiple results (designated as "out" operands
 * in the iclass section) or may have operands used as both
 * inputs and outputs (designated as "inout").  However, the C
 * and C++ languages do not provide syntax that can express
 * the in/out/inout constraints of TIE intrinsics.
 * Nevertheless, the compiler understands these constraints
 * and will check that the intrinsic functions are used
 * correctly.  To improve the readability of these prototypes,
 * the "out" and "inout" parameters are marked accordingly
 * with comments.
 */

extern void _TIE_xt_datacache_DHI(const int * s, immediate i);
extern void _TIE_xt_datacache_DHI_B(const int * s, int i);
extern void _TIE_xt_datacache_DHU(const int * s, immediate i);
extern void _TIE_xt_datacache_DHWB(const int * s, immediate i);
extern void _TIE_xt_datacache_DHWBI(const int * s, immediate i);
extern void _TIE_xt_datacache_DHWBI_B(const int * s, int i);
extern void _TIE_xt_datacache_DHWB_B(const int * s, int i);
extern void _TIE_xt_datacache_DII(const int * s, immediate i);
extern void _TIE_xt_datacache_DIU(const int * s, immediate i);
extern void _TIE_xt_datacache_DIWB(const int * s, immediate i);
extern void _TIE_xt_datacache_DIWBI(const int * s, immediate i);
extern void _TIE_xt_datacache_DIWBUI_P(const int * s /*inout*/);
extern void _TIE_xt_datacache_DPFL(const int * s, immediate i);
extern void _TIE_xt_datacache_DPFM_B(const int * s, int i);
extern void _TIE_xt_datacache_DPFM_BF(const int * s, int i);
extern void _TIE_xt_datacache_DPFR(const int * s, immediate i);
extern void _TIE_xt_datacache_DPFRO(const int * s, immediate i);
extern void _TIE_xt_datacache_DPFR_B(const int * s, int i);
extern void _TIE_xt_datacache_DPFR_BF(const int * s, int i);
extern void _TIE_xt_datacache_DPFW(const int * s, immediate i);
extern void _TIE_xt_datacache_DPFWO(const int * s, immediate i);
extern void _TIE_xt_datacache_DPFW_B(const int * s, int i);
extern void _TIE_xt_datacache_DPFW_BF(const int * s, int i);
extern int _TIE_xt_datacache_LDCT(const int * s);
extern int _TIE_xt_datacache_LDCW(const int * s);
extern void _TIE_xt_datacache_PFEND_A(void);
extern void _TIE_xt_datacache_PFEND_O(void);
extern void _TIE_xt_datacache_PFNXT_F(void);
extern void _TIE_xt_datacache_PFWAIT_A(void);
extern void _TIE_xt_datacache_PFWAIT_R(void);
extern void _TIE_xt_datacache_SDCT(int t, int * s);
extern void _TIE_xt_datacache_SDCW(int t, int * s);

#endif /*__ASSEMBLER__*/
#endif /*_NOCLANGUAGE*/
#endif /*_ASMLANGUAGE*/

#define XT_DHI _TIE_xt_datacache_DHI
#define XT_DHI_B _TIE_xt_datacache_DHI_B
#define XT_DHU _TIE_xt_datacache_DHU
#define XT_DHWB _TIE_xt_datacache_DHWB
#define XT_DHWBI _TIE_xt_datacache_DHWBI
#define XT_DHWBI_B _TIE_xt_datacache_DHWBI_B
#define XT_DHWB_B _TIE_xt_datacache_DHWB_B
#define XT_DII _TIE_xt_datacache_DII
#define XT_DIU _TIE_xt_datacache_DIU
#define XT_DIWB _TIE_xt_datacache_DIWB
#define XT_DIWBI _TIE_xt_datacache_DIWBI
#define XT_DIWBUI_P _TIE_xt_datacache_DIWBUI_P
#define XT_DPFL _TIE_xt_datacache_DPFL
#define XT_DPFM_B _TIE_xt_datacache_DPFM_B
#define XT_DPFM_BF _TIE_xt_datacache_DPFM_BF
#define XT_DPFR _TIE_xt_datacache_DPFR
#define XT_DPFRO _TIE_xt_datacache_DPFRO
#define XT_DPFR_B _TIE_xt_datacache_DPFR_B
#define XT_DPFR_BF _TIE_xt_datacache_DPFR_BF
#define XT_DPFW _TIE_xt_datacache_DPFW
#define XT_DPFWO _TIE_xt_datacache_DPFWO
#define XT_DPFW_B _TIE_xt_datacache_DPFW_B
#define XT_DPFW_BF _TIE_xt_datacache_DPFW_BF
#define XT_LDCT _TIE_xt_datacache_LDCT
#define XT_LDCW _TIE_xt_datacache_LDCW
#define XT_PFEND_A _TIE_xt_datacache_PFEND_A
#define XT_PFEND_O _TIE_xt_datacache_PFEND_O
#define XT_PFNXT_F _TIE_xt_datacache_PFNXT_F
#define XT_PFWAIT_A _TIE_xt_datacache_PFWAIT_A
#define XT_PFWAIT_R _TIE_xt_datacache_PFWAIT_R
#define XT_SDCT _TIE_xt_datacache_SDCT
#define XT_SDCW _TIE_xt_datacache_SDCW

#endif /* __XCC__ */

#endif /* __XTENSA__ */


/* parasoft-end-suppress ALL "This file not MISRA checked." */

#endif /* !_XTENSA_xt_datacache_HEADER */
