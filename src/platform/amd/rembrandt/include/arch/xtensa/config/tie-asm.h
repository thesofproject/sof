/* 
 * tie-asm.h -- compile-time HAL assembler definitions dependent on CORE & TIE
 *
 *  NOTE:  This header file is not meant to be included directly.
 */

/* This header file contains assembly-language definitions (assembly
   macros, etc.) for this specific Xtensa processor's TIE extensions
   and options.  It is customized to this Xtensa processor configuration.

   Customer ID=12153; Build=0x878bf; Copyright (c) 1999-2020 Cadence Design Systems Inc.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifndef _XTENSA_CORE_TIE_ASM_H
#define _XTENSA_CORE_TIE_ASM_H

#include <xtensa/coreasm.h>

/*  Selection parameter values for save-area save/restore macros:  */
/*  Option vs. TIE:  */
#define XTHAL_SAS_TIE	0x0001	/* custom extension or coprocessor */
#define XTHAL_SAS_OPT	0x0002	/* optional (and not a coprocessor) */
#define XTHAL_SAS_ANYOT	0x0003	/* both of the above */
/*  Whether used automatically by compiler:  */
#define XTHAL_SAS_NOCC	0x0004	/* not used by compiler w/o special opts/code */
#define XTHAL_SAS_CC	0x0008	/* used by compiler without special opts/code */
#define XTHAL_SAS_ANYCC	0x000C	/* both of the above */
/*  ABI handling across function calls:  */
#define XTHAL_SAS_CALR	0x0010	/* caller-saved */
#define XTHAL_SAS_CALE	0x0020	/* callee-saved */
#define XTHAL_SAS_GLOB	0x0040	/* global across function calls (in thread) */
#define XTHAL_SAS_ANYABI	0x0070	/* all of the above three */
/*  Misc  */
#define XTHAL_SAS_ALL	0xFFFF	/* include all default NCP contents */
#define XTHAL_SAS3(optie,ccuse,abi)	( ((optie) & XTHAL_SAS_ANYOT)  \
					| ((ccuse) & XTHAL_SAS_ANYCC)  \
					| ((abi)   & XTHAL_SAS_ANYABI) )


    /*
      *  Macro to store all non-coprocessor (extra) custom TIE and optional state
      *  (not including zero-overhead loop registers).
      *  Required parameters:
      *      ptr         Save area pointer address register (clobbered)
      *                  (register must contain a 4 byte aligned address).
      *      at1..at4    Four temporary address registers (first XCHAL_NCP_NUM_ATMPS
      *                  registers are clobbered, the remaining are unused).
      *  Optional parameters:
      *      continue    If macro invoked as part of a larger store sequence, set to 1
      *                  if this is not the first in the sequence.  Defaults to 0.
      *      ofs         Offset from start of larger sequence (from value of first ptr
      *                  in sequence) at which to store.  Defaults to next available space
      *                  (or 0 if <continue> is 0).
      *      select      Select what category(ies) of registers to store, as a bitmask
      *                  (see XTHAL_SAS_xxx constants).  Defaults to all registers.
      *      alloc       Select what category(ies) of registers to allocate; if any
      *                  category is selected here that is not in <select>, space for
      *                  the corresponding registers is skipped without doing any store.
      */
    .macro xchal_ncp_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start	\continue, \ofs
	// Optional global registers used by default by the compiler:
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\select)
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	rur.threadptr	\at1		// threadptr option
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	// Optional caller-saved registers not used by default by the compiler:
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 1012, 4, 4
	rsr.br	\at1		// boolean option
	s32i	\at1, \ptr, .Lxchal_ofs_+0
	rsr.scompare1	\at1		// conditional store option
	s32i	\at1, \ptr, .Lxchal_ofs_+4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1012, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.endif
    .endm	// xchal_ncp_store

    /*
      *  Macro to load all non-coprocessor (extra) custom TIE and optional state
      *  (not including zero-overhead loop registers).
      *  Required parameters:
      *      ptr         Save area pointer address register (clobbered)
      *                  (register must contain a 4 byte aligned address).
      *      at1..at4    Four temporary address registers (first XCHAL_NCP_NUM_ATMPS
      *                  registers are clobbered, the remaining are unused).
      *  Optional parameters:
      *      continue    If macro invoked as part of a larger load sequence, set to 1
      *                  if this is not the first in the sequence.  Defaults to 0.
      *      ofs         Offset from start of larger sequence (from value of first ptr
      *                  in sequence) at which to load.  Defaults to next available space
      *                  (or 0 if <continue> is 0).
      *      select      Select what category(ies) of registers to load, as a bitmask
      *                  (see XTHAL_SAS_xxx constants).  Defaults to all registers.
      *      alloc       Select what category(ies) of registers to allocate; if any
      *                  category is selected here that is not in <select>, space for
      *                  the corresponding registers is skipped without doing any load.
      */
    .macro xchal_ncp_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start	\continue, \ofs
	// Optional global registers used by default by the compiler:
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\select)
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wur.threadptr	\at1		// threadptr option
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_CC | XTHAL_SAS_GLOB) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1016, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 4
	.endif
	// Optional caller-saved registers not used by default by the compiler:
	.ifeq (XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 1012, 4, 4
	l32i	\at1, \ptr, .Lxchal_ofs_+0
	wsr.br	\at1		// boolean option
	l32i	\at1, \ptr, .Lxchal_ofs_+4
	wsr.scompare1	\at1		// conditional store option
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.elseif ((XTHAL_SAS_OPT | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 1012, 4, 4
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 8
	.endif
    .endm	// xchal_ncp_load


#define XCHAL_NCP_NUM_ATMPS	1

    /* 
     *  Macro to store the state of TIE coprocessor AudioEngineLX.
     *  Required parameters:
     *      ptr         Save area pointer address register (clobbered)
     *                  (register must contain a 16 byte aligned address).
     *      at1..at4    Four temporary address registers (first XCHAL_CP1_NUM_ATMPS
     *                  registers are clobbered, the remaining are unused).
     *  Optional parameters are the same as for xchal_ncp_store.
     */
#define xchal_cp_AudioEngineLX_store	xchal_cp1_store
    .macro	xchal_cp1_store  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start \continue, \ofs
	// Custom caller-saved registers not used by default by the compiler:
	.ifeq (XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 0, 16, 16
	ae_s64.i	aed0, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_s64.i	aed1, \ptr, .Lxchal_ofs_+0
	ae_s64.i	aed2, \ptr, .Lxchal_ofs_+8
	ae_s64.i	aed3, \ptr, .Lxchal_ofs_+16
	ae_s64.i	aed4, \ptr, .Lxchal_ofs_+24
	ae_s64.i	aed5, \ptr, .Lxchal_ofs_+32
	ae_s64.i	aed6, \ptr, .Lxchal_ofs_+40
	ae_s64.i	aed7, \ptr, .Lxchal_ofs_+48
	ae_s64.i	aed8, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_s64.i	aed9, \ptr, .Lxchal_ofs_+0
	ae_s64.i	aed10, \ptr, .Lxchal_ofs_+8
	ae_s64.i	aed11, \ptr, .Lxchal_ofs_+16
	ae_s64.i	aed12, \ptr, .Lxchal_ofs_+24
	ae_s64.i	aed13, \ptr, .Lxchal_ofs_+32
	ae_s64.i	aed14, \ptr, .Lxchal_ofs_+40
	ae_s64.i	aed15, \ptr, .Lxchal_ofs_+48
	ae_s64.i	aed16, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_s64.i	aed17, \ptr, .Lxchal_ofs_+0
	ae_s64.i	aed18, \ptr, .Lxchal_ofs_+8
	ae_s64.i	aed19, \ptr, .Lxchal_ofs_+16
	ae_s64.i	aed20, \ptr, .Lxchal_ofs_+24
	ae_s64.i	aed21, \ptr, .Lxchal_ofs_+32
	ae_s64.i	aed22, \ptr, .Lxchal_ofs_+40
	ae_s64.i	aed23, \ptr, .Lxchal_ofs_+48
	ae_s64.i	aed24, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_s64.i	aed25, \ptr, .Lxchal_ofs_+0
	ae_s64.i	aed26, \ptr, .Lxchal_ofs_+8
	ae_s64.i	aed27, \ptr, .Lxchal_ofs_+16
	ae_s64.i	aed28, \ptr, .Lxchal_ofs_+24
	ae_s64.i	aed29, \ptr, .Lxchal_ofs_+32
	ae_s64.i	aed30, \ptr, .Lxchal_ofs_+40
	ae_s64.i	aed31, \ptr, .Lxchal_ofs_+48
	ae_movae	\at1, aep0
	s8i	\at1, \ptr, .Lxchal_ofs_+56
	ae_movae	\at1, aep1
	s8i	\at1, \ptr, .Lxchal_ofs_+57
	ae_movae	\at1, aep2
	s8i	\at1, \ptr, .Lxchal_ofs_+58
	ae_movae	\at1, aep3
	s8i	\at1, \ptr, .Lxchal_ofs_+59
	addi.a	\ptr, \ptr, 64
	ae_salign128.i	u0, \ptr, .Lxchal_ofs_+0
	ae_salign128.i	u1, \ptr, .Lxchal_ofs_+16
	ae_salign128.i	u2, \ptr, .Lxchal_ofs_+32
	ae_salign128.i	u3, \ptr, .Lxchal_ofs_+48
	addi.a	\ptr, \ptr, -320
	ae_movdrzbvc	aed0		// ureg AE_ZBIASV8C
	ae_s64.i	aed0, \ptr, .Lxchal_ofs_+0 + 0
	ae_movvfcrfsr	aed0		// ureg FCR_FSR
	ae_s64.i	aed0, \ptr, .Lxchal_ofs_+8 + 0
	rur.ae_ovf_sar	\at1		// ureg 240
	s32i	\at1, \ptr, .Lxchal_ofs_+16
	rur.ae_bithead	\at1		// ureg 241
	s32i	\at1, \ptr, .Lxchal_ofs_+20
	rur.ae_ts_fts_bu_bp	\at1		// ureg 242
	s32i	\at1, \ptr, .Lxchal_ofs_+24
	rur.ae_cw_sd_no	\at1		// ureg 243
	s32i	\at1, \ptr, .Lxchal_ofs_+28
	rur.ae_cbegin0	\at1		// ureg 246
	s32i	\at1, \ptr, .Lxchal_ofs_+32
	rur.ae_cend0	\at1		// ureg 247
	s32i	\at1, \ptr, .Lxchal_ofs_+36
	rur.ae_cbegin1	\at1		// ureg 248
	s32i	\at1, \ptr, .Lxchal_ofs_+40
	rur.ae_cend1	\at1		// ureg 249
	s32i	\at1, \ptr, .Lxchal_ofs_+44
	rur.ae_cbegin2	\at1		// ureg 250
	s32i	\at1, \ptr, .Lxchal_ofs_+48
	rur.ae_cend2	\at1		// ureg 251
	s32i	\at1, \ptr, .Lxchal_ofs_+52
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 384
	.elseif ((XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 0, 16, 16
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 384
	.endif
    .endm	// xchal_cp1_store

    /* 
     *  Macro to load the state of TIE coprocessor AudioEngineLX.
     *  Required parameters:
     *      ptr         Save area pointer address register (clobbered)
     *                  (register must contain a 16 byte aligned address).
     *      at1..at4    Four temporary address registers (first XCHAL_CP1_NUM_ATMPS
     *                  registers are clobbered, the remaining are unused).
     *  Optional parameters are the same as for xchal_ncp_load.
     */
#define xchal_cp_AudioEngineLX_load	xchal_cp1_load
    .macro	xchal_cp1_load  ptr at1 at2 at3 at4  continue=0 ofs=-1 select=XTHAL_SAS_ALL alloc=0
	xchal_sa_start \continue, \ofs
	// Custom caller-saved registers not used by default by the compiler:
	.ifeq (XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\select)
	xchal_sa_align	\ptr, 0, 0, 16, 16
	ae_l64.i	aed0, \ptr, .Lxchal_ofs_+0 + 0		// ureg AE_ZBIASV8C
	ae_movzbvcdr	aed0
	ae_l64.i	aed0, \ptr, .Lxchal_ofs_+8 + 0		// ureg FCR_FSR
	ae_movfcrfsrv	aed0
	l32i	\at1, \ptr, .Lxchal_ofs_+16
	wur.ae_ovf_sar	\at1		// ureg 240
	l32i	\at1, \ptr, .Lxchal_ofs_+20
	wur.ae_bithead	\at1		// ureg 241
	l32i	\at1, \ptr, .Lxchal_ofs_+24
	wur.ae_ts_fts_bu_bp	\at1		// ureg 242
	l32i	\at1, \ptr, .Lxchal_ofs_+28
	wur.ae_cw_sd_no	\at1		// ureg 243
	l32i	\at1, \ptr, .Lxchal_ofs_+32
	wur.ae_cbegin0	\at1		// ureg 246
	l32i	\at1, \ptr, .Lxchal_ofs_+36
	wur.ae_cend0	\at1		// ureg 247
	l32i	\at1, \ptr, .Lxchal_ofs_+40
	wur.ae_cbegin1	\at1		// ureg 248
	l32i	\at1, \ptr, .Lxchal_ofs_+44
	wur.ae_cend1	\at1		// ureg 249
	l32i	\at1, \ptr, .Lxchal_ofs_+48
	wur.ae_cbegin2	\at1		// ureg 250
	l32i	\at1, \ptr, .Lxchal_ofs_+52
	wur.ae_cend2	\at1		// ureg 251
	ae_l64.i	aed0, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_l64.i	aed1, \ptr, .Lxchal_ofs_+0
	ae_l64.i	aed2, \ptr, .Lxchal_ofs_+8
	ae_l64.i	aed3, \ptr, .Lxchal_ofs_+16
	ae_l64.i	aed4, \ptr, .Lxchal_ofs_+24
	ae_l64.i	aed5, \ptr, .Lxchal_ofs_+32
	ae_l64.i	aed6, \ptr, .Lxchal_ofs_+40
	ae_l64.i	aed7, \ptr, .Lxchal_ofs_+48
	ae_l64.i	aed8, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_l64.i	aed9, \ptr, .Lxchal_ofs_+0
	ae_l64.i	aed10, \ptr, .Lxchal_ofs_+8
	ae_l64.i	aed11, \ptr, .Lxchal_ofs_+16
	ae_l64.i	aed12, \ptr, .Lxchal_ofs_+24
	ae_l64.i	aed13, \ptr, .Lxchal_ofs_+32
	ae_l64.i	aed14, \ptr, .Lxchal_ofs_+40
	ae_l64.i	aed15, \ptr, .Lxchal_ofs_+48
	ae_l64.i	aed16, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_l64.i	aed17, \ptr, .Lxchal_ofs_+0
	ae_l64.i	aed18, \ptr, .Lxchal_ofs_+8
	ae_l64.i	aed19, \ptr, .Lxchal_ofs_+16
	ae_l64.i	aed20, \ptr, .Lxchal_ofs_+24
	ae_l64.i	aed21, \ptr, .Lxchal_ofs_+32
	ae_l64.i	aed22, \ptr, .Lxchal_ofs_+40
	ae_l64.i	aed23, \ptr, .Lxchal_ofs_+48
	ae_l64.i	aed24, \ptr, .Lxchal_ofs_+56
	addi.a	\ptr, \ptr, 64
	ae_l64.i	aed25, \ptr, .Lxchal_ofs_+0
	ae_l64.i	aed26, \ptr, .Lxchal_ofs_+8
	ae_l64.i	aed27, \ptr, .Lxchal_ofs_+16
	ae_l64.i	aed28, \ptr, .Lxchal_ofs_+24
	ae_l64.i	aed29, \ptr, .Lxchal_ofs_+32
	ae_l64.i	aed30, \ptr, .Lxchal_ofs_+40
	ae_l64.i	aed31, \ptr, .Lxchal_ofs_+48
	addi.a	\ptr, \ptr, 56
	l8ui	\at1, \ptr, .Lxchal_ofs_+0
	ae_movea	aep0, \at1
	l8ui	\at1, \ptr, .Lxchal_ofs_+1
	ae_movea	aep1, \at1
	l8ui	\at1, \ptr, .Lxchal_ofs_+2
	ae_movea	aep2, \at1
	l8ui	\at1, \ptr, .Lxchal_ofs_+3
	ae_movea	aep3, \at1
	addi.a	\ptr, \ptr, 8
	ae_lalign128.i	u0, \ptr, .Lxchal_ofs_+0
	ae_lalign128.i	u1, \ptr, .Lxchal_ofs_+16
	ae_lalign128.i	u2, \ptr, .Lxchal_ofs_+32
	ae_lalign128.i	u3, \ptr, .Lxchal_ofs_+48
	.set	.Lxchal_pofs_, .Lxchal_pofs_ + 320
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 64
	.elseif ((XTHAL_SAS_TIE | XTHAL_SAS_NOCC | XTHAL_SAS_CALR) & ~(\alloc)) == 0
	xchal_sa_align	\ptr, 0, 0, 16, 16
	.set	.Lxchal_ofs_, .Lxchal_ofs_ + 384
	.endif
    .endm	// xchal_cp1_load

#define XCHAL_CP1_NUM_ATMPS	1
#define XCHAL_SA_NUM_ATMPS	1

	/*  Empty macros for unconfigured coprocessors:  */
	.macro xchal_cp0_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp0_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp2_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp2_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp3_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp3_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp4_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp4_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp5_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp5_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp6_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp6_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp7_store	p a b c d continue=0 ofs=-1 select=-1 ; .endm
	.macro xchal_cp7_load	p a b c d continue=0 ofs=-1 select=-1 ; .endm

#endif /*_XTENSA_CORE_TIE_ASM_H*/

