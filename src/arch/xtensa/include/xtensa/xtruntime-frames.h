/*  xtruntime-frames.h  -  exception stack frames for single-threaded run-time  */
/* $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/include/xtensa/xtruntime-frames.h#1 $ */

/*
 * Copyright (c) 2002-2012 Tensilica Inc.
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

#ifndef _XTRUNTIME_FRAMES_H_
#define _XTRUNTIME_FRAMES_H_

#include <xtensa/config/core.h>

/*  Macros that help define structures for both C and assembler:  */
#if defined(_ASMLANGUAGE) || defined(__ASSEMBLER__)
#define STRUCT_BEGIN		.pushsection .text; .struct 0
#define STRUCT_FIELD(ctype,size,pre,name)	pre##name:	.space	size
#define STRUCT_AFIELD(ctype,size,pre,name,n)	pre##name:	.if n ; .space	(size)*(n) ; .endif
#define STRUCT_AFIELD_A(ctype,size,align,pre,name,n)	.balign align ; pre##name:	.if n ; .space (size)*(n) ; .endif
#define STRUCT_END(sname)	sname##Size:; .popsection
#else /*_ASMLANGUAGE||__ASSEMBLER__*/
#define STRUCT_BEGIN		typedef struct {
#define STRUCT_FIELD(ctype,size,pre,name)	ctype	name;
#define STRUCT_AFIELD(ctype,size,pre,name,n)	ctype	name[n];
#define STRUCT_AFIELD_A(ctype,size,align,pre,name,n)	ctype	name[n] __attribute__((aligned(align)));
#define STRUCT_END(sname)	} sname;
#endif /*_ASMLANGUAGE||__ASSEMBLER__*/

/* Coprocessors masks.
 * NOTE: currently only 2 supported.
 */
#define CP0_MASK	(1 << 0)
#define CP1_MASK	(1 << 1)

/*
 *  Kernel vector mode exception stack frame.
 *
 *  NOTE:  due to the limited range of addi used in the current
 *  kernel exception vector, and the fact that historically
 *  the vector is limited to 12 bytes, the size of this
 *  stack frame is limited to 128 bytes (currently at 64).
 */
STRUCT_BEGIN
STRUCT_FIELD (long,4,KEXC_,pc)		/* "parm" */
STRUCT_FIELD (long,4,KEXC_,ps)
STRUCT_AFIELD(long,4,KEXC_,areg, 4)	/* a12 .. a15 */
STRUCT_FIELD (long,4,KEXC_,sar)		/* "save" */
#if XCHAL_HAVE_LOOPS
STRUCT_FIELD (long,4,KEXC_,lcount)
STRUCT_FIELD (long,4,KEXC_,lbeg)
STRUCT_FIELD (long,4,KEXC_,lend)
#endif
#if XCHAL_HAVE_MAC16
STRUCT_FIELD (long,4,KEXC_,acclo)
STRUCT_FIELD (long,4,KEXC_,acchi)
STRUCT_AFIELD(long,4,KEXC_,mr, 4)
#endif
STRUCT_END(KernelFrame)


/*
 *  User vector mode exception stack frame:
 *
 *  WARNING:  if you modify this structure, you MUST modify the
 *  computation of the pad size (ALIGNPAD) accordingly.
 */
STRUCT_BEGIN
STRUCT_FIELD (long,4,UEXC_,pc)
STRUCT_FIELD (long,4,UEXC_,ps)
STRUCT_FIELD (long,4,UEXC_,sar)
STRUCT_FIELD (long,4,UEXC_,vpri)
STRUCT_FIELD (long,4,UEXC_,a0)
STRUCT_FIELD (long,4,UEXC_,a1)
STRUCT_FIELD (long,4,UEXC_,a2)
STRUCT_FIELD (long,4,UEXC_,a3)
STRUCT_FIELD (long,4,UEXC_,a4)
STRUCT_FIELD (long,4,UEXC_,a5)
STRUCT_FIELD (long,4,UEXC_,a6)
STRUCT_FIELD (long,4,UEXC_,a7)
STRUCT_FIELD (long,4,UEXC_,a8)
STRUCT_FIELD (long,4,UEXC_,a9)
STRUCT_FIELD (long,4,UEXC_,a10)
STRUCT_FIELD (long,4,UEXC_,a11)
STRUCT_FIELD (long,4,UEXC_,a12)
STRUCT_FIELD (long,4,UEXC_,a13)
STRUCT_FIELD (long,4,UEXC_,a14)
STRUCT_FIELD (long,4,UEXC_,a15)
STRUCT_FIELD (long,4,UEXC_,exccause)	/* NOTE: can probably rid of this one (pass direct) */
STRUCT_FIELD (long,4,UEXC_,align1)	/* alignment to 8 bytes */
#if XCHAL_HAVE_LOOPS
STRUCT_FIELD (long,4,UEXC_,lcount)
STRUCT_FIELD (long,4,UEXC_,lbeg)
STRUCT_FIELD (long,4,UEXC_,lend)
STRUCT_FIELD (long,4,UEXC_,align2)	/* alignment to 8 bytes */
#endif
#if XCHAL_HAVE_MAC16
STRUCT_FIELD (long,4,UEXC_,acclo)
STRUCT_FIELD (long,4,UEXC_,acchi)
STRUCT_AFIELD(long,4,UEXC_,mr, 4)
#endif
#if (XCHAL_CP_MASK & CP0_MASK)
STRUCT_AFIELD_A (long,4,XCHAL_TOTAL_SA_ALIGN,UEXC_,cp0, XCHAL_CP0_SA_SIZE / 4)
#endif
#if (XCHAL_CP_MASK & CP1_MASK)
STRUCT_AFIELD_A (long,4,XCHAL_TOTAL_SA_ALIGN,UEXC_,cp1, XCHAL_CP1_SA_SIZE / 4)
#endif
/* ALIGNPAD is the 16-byte alignment padding. */
#define ALIGNPAD  ((2 + XCHAL_HAVE_MAC16*2 + ((XCHAL_CP0_SA_SIZE%16)/4) + ((XCHAL_CP1_SA_SIZE%16)/4)) & 3)
#if ALIGNPAD
STRUCT_AFIELD(long,4,UEXC_,pad, ALIGNPAD)	/* 16-byte alignment padding */
#endif
/*STRUCT_AFIELD_A(char,1,XCHAL_CPEXTRA_SA_ALIGN,UEXC_,ureg, (XCHAL_CPEXTRA_SA_SIZE+3)&-4)*/	/* not used */
STRUCT_END(UserFrame)

/*
 * xtos_structures_pointers contains ptrs to all structures created for
 * each processor individually.
 *
 * To access the core specific structure from ASM (after threadptr is set):
 * xtos_addr_percore a13, xtos_interrupt_table
 */
STRUCT_BEGIN
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_enabled)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_intstruct)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_interrupt_table)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_interrupt_mask_table)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_stack_for_interrupt_1)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_stack_for_interrupt_2)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_stack_for_interrupt_3)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_stack_for_interrupt_4)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_stack_for_interrupt_5)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_interrupt_ctx)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_saved_ctx)
STRUCT_FIELD(void*,4,XTOS_PTR_TO_,xtos_saved_sp)
STRUCT_END(xtos_structures_pointers)

/*
 * xtos_task_context contains information about currently
 * executed task
 */

#define XTOS_TASK_CONTEXT_OWN_STACK	1

STRUCT_BEGIN
STRUCT_FIELD (UserFrame*,4,TC_,stack_pointer)
STRUCT_FIELD (void*,4,TC_,stack_base)
STRUCT_FIELD (long,4,TC_,stack_size)
STRUCT_FIELD (long,4,TC_,flags)
STRUCT_END(xtos_task_context)

#if defined(_ASMLANGUAGE) || defined(__ASSEMBLER__)


/*  Check for UserFrameSize small enough not to require rounding...:  */
	/*  Skip 16-byte save area, then 32-byte space for 8 regs of call12
	 *  (which overlaps with 16-byte GCC nested func chaining area),
	 *  then exception stack frame:  */
	.set	UserFrameTotalSize, 16+32+UserFrameSize
	/*  Greater than 112 bytes? (max range of ADDI, both signs, when aligned to 16 bytes):  */
	.ifgt	UserFrameTotalSize-112
	/*  Round up to 256-byte multiple to accelerate immediate adds:  */
	.set	UserFrameTotalSize, ((UserFrameTotalSize+255) & 0xFFFFFF00)
	.endif
# define ESF_TOTALSIZE	UserFrameTotalSize

#endif /* _ASMLANGUAGE || __ASSEMBLER__ */


#if XCHAL_NUM_CONTEXTS > 1
/*  Structure of info stored on new context's stack for setup:  */
STRUCT_BEGIN
STRUCT_FIELD (long,4,INFO_,sp)
STRUCT_FIELD (long,4,INFO_,arg1)
STRUCT_FIELD (long,4,INFO_,funcpc)
STRUCT_FIELD (long,4,INFO_,prevps)
STRUCT_END(SetupInfo)
#endif


#define KERNELSTACKSIZE	1024


#endif /* _XTRUNTIME_FRAMES_H_ */

