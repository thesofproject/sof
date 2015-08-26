/*
 * state_asm.S - assembly language processor management routines
 */

/*
 * Copyright (c) 2005-2010 Tensilica Inc.
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

#include <xtensa/coreasm.h>

#define GBL(x)   .global x
#define TYP(x)   .type x,@function
#define ALN(x)   .align x
#define SIZ(x)   .size x, . - x
#define MKEND(x) .purgem endfunc ; .macro endfunc ; SIZ(x) ; .purgem endfunc ; .macro endfunc ; .endm ; .endm
#define SYMT(x)  GBL(x); MKEND(x); TYP(x); ALN(4); x:
#define SYM(x)   GBL(x); MKEND(x); ALN(4); x:
	.macro endfunc ; .endm

/* For Call0 ABI, the xthal... and xthal..._nw versions are identical,
 * so we define both labels for the same function body.  The Makefile
 * does not define any of the __SPLIT__..._nw macros if Call0 ABI.
 * Use SYM() when we don't want .type information. */

#if defined (__XTENSA_CALL0_ABI__)
# define SYMBOL(x)  SYMT(x); SYMT(x ## _nw)
#else
# define SYMBOL(x)  SYMT(x)
#endif

	.text

//----------------------------------------------------------------------
// 		save the extra processor state.
//----------------------------------------------------------------------

#if defined(__SPLIT__save_extra)

// void xthal_save_extra(void *base)

SYMBOL(xthal_save_extra)
	abi_entry
	xchal_extra_store_funcbody
	abi_return
	endfunc

#endif
#if defined(__SPLIT__save_extra_nw)

// void xthal_save_extra_nw(void *base)

SYM(xthal_save_extra_nw)
	//addi	sp, sp, 0
	xchal_extra_store_funcbody
	ret
	endfunc


//----------------------------------------------------------------------
// 		restore the extra processor state.
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__restore_extra)
	
// void xthal_restore_extra(void *base)

SYMBOL(xthal_restore_extra)
	abi_entry
	xchal_extra_load_funcbody
	abi_return
	endfunc

#endif
#if defined(__SPLIT__restore_extra_nw)

// void xthal_restore_extra_nw(void *base)

SYM(xthal_restore_extra_nw)
	//addi	sp, sp, 0
	xchal_extra_load_funcbody
	ret
	endfunc


//----------------------------------------------------------------------
// 		save the TIE COPROCESSORS state
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__save_cpregs)

// void xthal_save_cpregs(void *base, int)

SYMBOL(xthal_save_cpregs)
	abi_entry
	xchal_cpi_store_funcbody
	abi_return
	endfunc

#endif
#if defined(__SPLIT__save_cp0)
// void xthal_save_cp0(void *base)
SYMBOL(xthal_save_cp0)
	abi_entry
	xchal_cp0_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp1)
// void xthal_save_cp1(void *base)
SYMBOL(xthal_save_cp1)
	abi_entry
	xchal_cp1_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp2)
// void xthal_save_cp2(void *base)
SYMBOL(xthal_save_cp2)
	abi_entry
	xchal_cp2_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp3)
// void xthal_save_cp3(void *base)
SYMBOL(xthal_save_cp3)
	abi_entry
	xchal_cp3_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp4)
// void xthal_save_cp4(void *base)
SYMBOL(xthal_save_cp4)
	abi_entry
	xchal_cp4_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp5)
// void xthal_save_cp5(void *base)
SYMBOL(xthal_save_cp5)
	abi_entry
	xchal_cp5_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp6)
// void xthal_save_cp6(void *base)
SYMBOL(xthal_save_cp6)
	abi_entry
	xchal_cp6_store_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__save_cp7)
// void xthal_save_cp7(void *base)
SYMBOL(xthal_save_cp7)
	abi_entry
	xchal_cp7_store_a2
	abi_return
	endfunc

#endif
#if defined(__SPLIT__save_cpregs_nw)

// void xthal_save_cpregs_nw(void *base, int)

SYM(xthal_save_cpregs_nw)
	//addi	sp, sp, 0
	xchal_cpi_store_funcbody
	ret
	endfunc

#endif
#if defined(__SPLIT__save_cp0_nw)
// void xthal_save_cp0(void *base)
SYM(xthal_save_cp0_nw)
	//addi	sp, sp, 0
	xchal_cp0_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp1_nw)
// void xthal_save_cp1(void *base)
SYM(xthal_save_cp1_nw)
	//addi	sp, sp, 0
	xchal_cp1_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp2_nw)
// void xthal_save_cp2(void *base)
SYM(xthal_save_cp2_nw)
	//addi	sp, sp, 0
	xchal_cp2_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp3_nw)
// void xthal_save_cp3(void *base)
SYM(xthal_save_cp3_nw)
	//addi	sp, sp, 0
	xchal_cp3_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp4_nw)
// void xthal_save_cp4(void *base)
SYM(xthal_save_cp4_nw)
	//addi	sp, sp, 0
	xchal_cp4_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp5_nw)
// void xthal_save_cp5(void *base)
SYM(xthal_save_cp5_nw)
	//addi	sp, sp, 0
	xchal_cp5_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp6_nw)
// void xthal_save_cp6(void *base)
SYM(xthal_save_cp6_nw)
	//addi	sp, sp, 0
	xchal_cp6_store_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__save_cp7_nw)
// void xthal_save_cp7(void *base)
SYM(xthal_save_cp7_nw)
	//addi	sp, sp, 0
	xchal_cp7_store_a2
	ret
	endfunc


//----------------------------------------------------------------------
// 		restore the TIE coprocessor state
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__restore_cpregs)

// void xthal_restore_cpregs(void *base, int)

SYMBOL(xthal_restore_cpregs)
	abi_entry
	xchal_cpi_load_funcbody
	abi_return
	endfunc

#endif
#if defined(__SPLIT__restore_cp0)
// void xthal_restore_cp0(void *base)
SYMBOL(xthal_restore_cp0)
	abi_entry
	xchal_cp0_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp1)
// void xthal_restore_cp1(void *base)
SYMBOL(xthal_restore_cp1)
	abi_entry
	xchal_cp1_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp2)
// void xthal_restore_cp2(void *base)
SYMBOL(xthal_restore_cp2)
	abi_entry
	xchal_cp2_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp3)
// void xthal_restore_cp3(void *base)
SYMBOL(xthal_restore_cp3)
	abi_entry
	xchal_cp3_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp4)
// void xthal_restore_cp4(void *base)
SYMBOL(xthal_restore_cp4)
	abi_entry
	xchal_cp4_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp5)
// void xthal_restore_cp5(void *base)
SYMBOL(xthal_restore_cp5)
	abi_entry
	xchal_cp5_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp6)
// void xthal_restore_cp6(void *base)
SYMBOL(xthal_restore_cp6)
	abi_entry
	xchal_cp6_load_a2
	abi_return
	endfunc
#endif
#if defined(__SPLIT__restore_cp7)
// void xthal_restore_cp7(void *base)
SYMBOL(xthal_restore_cp7)
	abi_entry
	xchal_cp7_load_a2
	abi_return
	endfunc

#endif
#if defined(__SPLIT__restore_cpregs_nw)

// void xthal_restore_cpregs_nw(void *base, int)

SYM(xthal_restore_cpregs_nw)
	//addi	sp, sp, 0
	xchal_cpi_load_funcbody
	ret
	endfunc

#endif
#if defined(__SPLIT__restore_cp0_nw)
// void xthal_restore_cp0(void *base)
SYM(xthal_restore_cp0_nw)
	//addi	sp, sp, 0
	xchal_cp0_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp1_nw)
// void xthal_restore_cp1(void *base)
SYM(xthal_restore_cp1_nw)
	//addi	sp, sp, 0
	xchal_cp1_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp2_nw)
// void xthal_restore_cp2(void *base)
SYM(xthal_restore_cp2_nw)
	//addi	sp, sp, 0
	xchal_cp2_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp3_nw)
// void xthal_restore_cp3(void *base)
SYM(xthal_restore_cp3_nw)
	//addi	sp, sp, 0
	xchal_cp3_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp4_nw)
// void xthal_restore_cp4(void *base)
SYM(xthal_restore_cp4_nw)
	//addi	sp, sp, 0
	xchal_cp4_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp5_nw)
// void xthal_restore_cp5(void *base)
SYM(xthal_restore_cp5_nw)
	//addi	sp, sp, 0
	xchal_cp5_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp6_nw)
// void xthal_restore_cp6(void *base)
SYM(xthal_restore_cp6_nw)
	//addi	sp, sp, 0
	xchal_cp6_load_a2
	ret
	endfunc
#endif
#if defined(__SPLIT__restore_cp7_nw)
// void xthal_restore_cp7(void *base)
SYM(xthal_restore_cp7_nw)
	//addi	sp, sp, 0
	xchal_cp7_load_a2
	ret
	endfunc

#endif
#if defined(__SPLIT__cpregs_save_fn)
	.section .rodata, "a"
SYM(Xthal_cpregs_save_fn)
# ifdef __XTENSA_CALL0_ABI__
SYM(Xthal_cpregs_save_nw_fn)
# endif
	.long	xthal_save_cp0
	.long	xthal_save_cp1
	.long	xthal_save_cp2
	.long	xthal_save_cp3
	.long	xthal_save_cp4
	.long	xthal_save_cp5
	.long	xthal_save_cp6
	.long	xthal_save_cp7
	endfunc
	.text

#endif
#if defined(__SPLIT__cpregs_save_nw_fn)
# ifndef __XTENSA_CALL0_ABI__
	.section .rodata, "a"
SYM(Xthal_cpregs_save_nw_fn)
	.long	xthal_save_cp0_nw
	.long	xthal_save_cp1_nw
	.long	xthal_save_cp2_nw
	.long	xthal_save_cp3_nw
	.long	xthal_save_cp4_nw
	.long	xthal_save_cp5_nw
	.long	xthal_save_cp6_nw
	.long	xthal_save_cp7_nw
	endfunc
	.text
# endif

#endif
#if defined(__SPLIT__cpregs_restore_fn)
	.section .rodata, "a"
SYM(Xthal_cpregs_restore_fn)
# ifdef __XTENSA_CALL0_ABI__
SYM(Xthal_cpregs_restore_nw_fn)
# endif
	.long	xthal_restore_cp0
	.long	xthal_restore_cp1
	.long	xthal_restore_cp2
	.long	xthal_restore_cp3
	.long	xthal_restore_cp4
	.long	xthal_restore_cp5
	.long	xthal_restore_cp6
	.long	xthal_restore_cp7
	endfunc
	.text

#endif
#if defined(__SPLIT__cpregs_restore_nw_fn)
# ifndef __XTENSA_CALL0_ABI__
	.section .rodata, "a"
SYM(Xthal_cpregs_restore_nw_fn)
	.long	xthal_restore_cp0_nw
	.long	xthal_restore_cp1_nw
	.long	xthal_restore_cp2_nw
	.long	xthal_restore_cp3_nw
	.long	xthal_restore_cp4_nw
	.long	xthal_restore_cp5_nw
	.long	xthal_restore_cp6_nw
	.long	xthal_restore_cp7_nw
	endfunc
	.text
# endif


//----------------------------------------------------------------------
//		coprocessor enable/disable
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__validate_cp)

// validate the register file.
// void xthal_validate_cp(int)

SYMBOL(xthal_validate_cp)
	abi_entry
#if XCHAL_HAVE_CP
	rsr	a3, CPENABLE
	movi	a4, 1
	ssl	a2
	sll	a4, a4
	or	a3, a3, a4
	wsr	a3, CPENABLE
#endif
	abi_return
	endfunc

#endif
#if defined(__SPLIT__validate_cp_nw)

// validate the register file.
// void xthal_validate_cp_nw(int)

SYM(xthal_validate_cp_nw)
	//addi	sp, sp, 0
#if XCHAL_HAVE_CP
	rsr	a3, CPENABLE
	movi	a4, 1
	ssl	a2
	sll	a4, a4
	or	a3, a3, a4
	wsr	a3, CPENABLE
#endif
	ret
	endfunc

#endif
#if defined(__SPLIT__invalidate_cp)

// invalidate the register file.
// void xthal_invalidate_cp(int)

SYMBOL(xthal_invalidate_cp)
	abi_entry
#if XCHAL_HAVE_CP
	rsr	a3, CPENABLE
	movi	a4, 1
	ssl	a2
	sll	a4, a4
	and	a4, a3, a4
	xor	a3, a3, a4
	wsr	a3, CPENABLE
#endif
	abi_return
	endfunc

#endif
#if defined(__SPLIT__invalidate_cp_nw)

// invalidate the register file.
// void xthal_invalidate_cp_nw(int)

SYMBOL(xthal_invalidate_cp_nw)
	//addi	sp, sp, 0
#if XCHAL_HAVE_CP
	rsr	a3, CPENABLE
	movi	a4, 1
	ssl	a2
	sll	a4, a4
	and	a4, a3, a4
	xor	a3, a3, a4
	wsr	a3, CPENABLE
#endif
	ret
	endfunc


//----------------------------------------------------------------------
//  Access the CPENABLE register
//----------------------------------------------------------------------

#endif
#if defined(__SPLIT__get_cpenable)

// unsigned xthal_get_cpenable(void);

SYMBOL(xthal_get_cpenable)
	abi_entry
#if XCHAL_HAVE_CP
	rsr	a2, CPENABLE
#else
	movi	a2, 0	// if no CPENABLE (no coprocessors), none is ever enabled
#endif
	abi_return
	endfunc

#endif
#if defined(__SPLIT__get_cpenable_nw)

// unsigned xthal_get_cpenable_nw(void);

SYM(xthal_get_cpenable_nw)
	//addi	sp, sp, 0
#if XCHAL_HAVE_CP
	rsr	a2, CPENABLE
#else
	movi	a2, 0	// if no CPENABLE (no coprocessors), none is ever enabled
#endif
	ret
	endfunc

#endif
#if defined(__SPLIT__set_cpenable)

// void xthal_set_cpenable(unsigned);
//
// Note:  to help asm code performance (eg. OS task switch),
// this routine returns the previous value of CPENABLE in a3
// (not a2, because that could require an extra mov instruction).
// This return value is not shown in the prototype, because
// C code won't see it.
// [Perhaps this should go in an RTOS-specific Core HAL or BSP.  TBD.]

SYMBOL(xthal_set_cpenable)
	abi_entry
#if XCHAL_HAVE_CP
	//rsr	a3, CPENABLE	// return previous CPENABLE
	movi	a3, 0		// for now, always return 0 (VxWorks currently done that way)

	wsr	a2, CPENABLE
#else
	movi	a3, 0	// if no CPENABLE (no coprocessors), none is ever enabled
#endif
	abi_return
	endfunc

#endif
#if defined(__SPLIT__set_cpenable_nw)

// unsigned[a3] xthal_set_cpenable_nw(unsigned);
//
// Note:  to help asm code performance (eg. OS task switch),
// this routine returns the previous value of CPENABLE in a3
// (not a2, because that could require an extra mov instruction).

SYMBOL(xthal_set_cpenable_nw)
	//addi	sp, sp, 0
#if XCHAL_HAVE_CP
	//rsr	a3, CPENABLE	// return previous CPENABLE
	movi	a3, 0		// for now, always return 0 (note: no OS currently calls this)

	wsr	a2, CPENABLE
#else
	movi	a3, 0	// if no CPENABLE (no coprocessors), none is ever enabled
#endif
	ret
	endfunc

#endif


/*  Nothing implemented below this point.  */
/************************************************************************/

#if 0

//----------------------------------------------------------------------
// initialize the processor state
//----------------------------------------------------------------------

// void xthal_init_extra_nw()
	.global	xthal_init_extra_nw
	.align	4
xthal_init_extra_nw:
	//addi	sp, sp, 0
	... NOT IMPLEMENTED ...
	ret

//----------------------------------------------------------------------
// initialize the TIE coprocessor
//----------------------------------------------------------------------

// void xthal_init_cp_nw(int)
	.global	xthal_init_cp_nw
	.align	4
xthal_init_cp_nw:
	//addi	sp, sp, 0
	... NOT IMPLEMENTED ...
	ret

//----------------------------------------------------------------------
// 
//----------------------------------------------------------------------

// initialize the extra processor
// void xthal_init_mem_extra_nw()
	.global	xthal_init_mem_extra_nw
	.align	4
xthal_init_mem_extra_nw:
	//addi	sp, sp, 0
	... NOT IMPLEMENTED ...
	ret

//----------------------------------------------------------------------
// 
//----------------------------------------------------------------------

// initialize the TIE coprocessor
// void xthal_init_mem_cp_nw(int)
	.global	xthal_init_mem_cp_nw
	.align	4
xthal_init_mem_cp_nw:
	//addi	sp, sp, 0
	... NOT IMPLEMENTED ...
	ret

#endif /*0*/

