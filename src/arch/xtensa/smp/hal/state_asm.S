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

	.text

//----------------------------------------------------------------------
// 		save the extra processor state.
//----------------------------------------------------------------------

#if defined(__SPLIT__save_extra) ||\
    defined(__SPLIT__save_extra_nw)

// void xthal_save_extra(void *base)

DECLFUNC(xthal_save_extra)
	abi_entry
	xchal_extra_store_funcbody
	abi_return
	endfunc


//----------------------------------------------------------------------
// 		restore the extra processor state.
//----------------------------------------------------------------------

#elif 	defined(__SPLIT__restore_extra) ||\
	defined(__SPLIT__restore_extra_nw)
	
// void xthal_restore_extra(void *base)

DECLFUNC(xthal_restore_extra)
	abi_entry
	xchal_extra_load_funcbody
	abi_return
	endfunc

//----------------------------------------------------------------------
// 		save the TIE COPROCESSORS state
//----------------------------------------------------------------------

#elif 	defined(__SPLIT__save_cpregs) ||\
	defined(__SPLIT__save_cpregs_nw)

// void xthal_save_cpregs(void *base, int)
DECLFUNC(xthal_save_cpregs)
	abi_entry
	xchal_cpi_store_funcbody
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp0) ||\
	defined(__SPLIT__save_cp0_nw)
// void xthal_save_cp0(void *base)
DECLFUNC(xthal_save_cp0)
	abi_entry
	xchal_cp0_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp1) ||\
	defined(__SPLIT__save_cp1_nw)
// void xthal_save_cp1(void *base)
DECLFUNC(xthal_save_cp1)
	abi_entry
	xchal_cp1_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp2) ||\
	defined(__SPLIT__save_cp2_nw)
// void xthal_save_cp2(void *base)
DECLFUNC(xthal_save_cp2)
	abi_entry
	xchal_cp2_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp3) ||\
	defined(__SPLIT__save_cp3_nw)
// void xthal_save_cp3(void *base)
DECLFUNC(xthal_save_cp3)
	abi_entry
	xchal_cp3_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp4) ||\
	defined(__SPLIT__save_cp4_nw)
// void xthal_save_cp4(void *base)
DECLFUNC(xthal_save_cp4)
	abi_entry
	xchal_cp4_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp5) ||\
	defined(__SPLIT__save_cp5_nw)
// void xthal_save_cp5(void *base)
DECLFUNC(xthal_save_cp5)
	abi_entry
	xchal_cp5_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp6) || \
	defined(__SPLIT__save_cp6_nw)
// void xthal_save_cp6(void *base)
DECLFUNC(xthal_save_cp6)
	abi_entry
	xchal_cp6_store_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__save_cp7) ||\
	defined(__SPLIT__save_cp7_nw)
// void xthal_save_cp7(void *base)
DECLFUNC(xthal_save_cp7)
	abi_entry
	xchal_cp7_store_a2
	abi_return
	endfunc

//----------------------------------------------------------------------
// 		restore the TIE coprocessor state
//----------------------------------------------------------------------

#elif 	defined(__SPLIT__restore_cpregs) ||\
	defined(__SPLIT__restore_cpregs_nw)

// void xthal_restore_cpregs(void *base, int)

DECLFUNC(xthal_restore_cpregs)
	abi_entry
	xchal_cpi_load_funcbody
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp0) ||\
	defined(__SPLIT__restore_cp0_nw)
// void xthal_restore_cp0(void *base)
DECLFUNC(xthal_restore_cp0)
	abi_entry
	xchal_cp0_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp1) ||\
	defined(__SPLIT__restore_cp1_nw)
// void xthal_restore_cp1(void *base)
DECLFUNC(xthal_restore_cp1)
	abi_entry
	xchal_cp1_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp2) ||\
	defined(__SPLIT__restore_cp2_nw)
// void xthal_restore_cp2(void *base)
DECLFUNC(xthal_restore_cp2)
	abi_entry
	xchal_cp2_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp3) || \
	defined(__SPLIT__restore_cp3_nw)
// void xthal_restore_cp3(void *base)
DECLFUNC(xthal_restore_cp3)
	abi_entry
	xchal_cp3_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp4) || \
	defined(__SPLIT__restore_cp4_nw)
// void xthal_restore_cp4(void *base)
DECLFUNC(xthal_restore_cp4)
	abi_entry
	xchal_cp4_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp5) || \
	defined(__SPLIT__restore_cp5_nw)
// void xthal_restore_cp5(void *base)
DECLFUNC(xthal_restore_cp5)
	abi_entry
	xchal_cp5_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp6) || \
	defined(__SPLIT__restore_cp6_nw)
// void xthal_restore_cp6(void *base)
DECLFUNC(xthal_restore_cp6)
	abi_entry
	xchal_cp6_load_a2
	abi_return
	endfunc
#elif 	defined(__SPLIT__restore_cp7) || \
	defined(__SPLIT__restore_cp7_nw)
// void xthal_restore_cp7(void *base)
DECLFUNC(xthal_restore_cp7)
	abi_entry
	xchal_cp7_load_a2
	abi_return
	endfunc

#elif defined(__SPLIT__cpregs_save_fn)
	.section .rodata, "a"
_SYM(Xthal_cpregs_save_fn)
# ifdef __XTENSA_CALL0_ABI__
_SYM(Xthal_cpregs_save_nw_fn)
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

#elif defined(__SPLIT__cpregs_save_nw_fn)
# ifndef __XTENSA_CALL0_ABI__
	.section .rodata, "a"
_SYM(Xthal_cpregs_save_nw_fn)
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

#elif defined(__SPLIT__cpregs_restore_fn)
	.section .rodata, "a"
_SYM(Xthal_cpregs_restore_fn)
# ifdef __XTENSA_CALL0_ABI__
_SYM(Xthal_cpregs_restore_nw_fn)
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

#elif defined(__SPLIT__cpregs_restore_nw_fn)
# ifndef __XTENSA_CALL0_ABI__
	.section .rodata, "a"
_SYM(Xthal_cpregs_restore_nw_fn)
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

#elif 	defined(__SPLIT__validate_cp) ||\
	defined(__SPLIT__validate_cp_nw)

// validate the register file.
// void xthal_validate_cp(int)

DECLFUNC(xthal_validate_cp)
	abi_entry
#if XCHAL_HAVE_CP
	rsr.cpenable	a3
	movi	a4, 1
	ssl	a2
	sll	a4, a4
	or	a3, a3, a4
	wsr.cpenable	a3
#endif
	abi_return
	endfunc

#elif 	defined(__SPLIT__invalidate_cp) || \
	defined(__SPLIT__invalidate_cp_nw)

// invalidate the register file.
// void xthal_invalidate_cp(int)

DECLFUNC(xthal_invalidate_cp)
	abi_entry
#if XCHAL_HAVE_CP
	rsr.cpenable	a3
	movi	a4, 1
	ssl	a2
	sll	a4, a4
	and	a4, a3, a4
	xor	a3, a3, a4
	wsr.cpenable	a3
#endif
	abi_return
	endfunc


//----------------------------------------------------------------------
//  Access the CPENABLE register
//----------------------------------------------------------------------

#elif 	defined(__SPLIT__get_cpenable) || \
	defined(__SPLIT__get_cpenable_nw)

// unsigned xthal_get_cpenable(void);

DECLFUNC(xthal_get_cpenable)
	abi_entry
#if XCHAL_HAVE_CP
	rsr.cpenable	a2
#else
	movi	a2, 0	// if no CPENABLE (no coprocessors), none is ever enabled
#endif
	abi_return
	endfunc

#elif 	defined(__SPLIT__set_cpenable) ||\
	defined(__SPLIT__set_cpenable_nw)

// void xthal_set_cpenable(unsigned);
//
// Note:  to help asm code performance (eg. OS task switch),
// this routine returns the previous value of CPENABLE in a3
// (not a2, because that could require an extra mov instruction).
// This return value is not shown in the prototype, because
// C code won't see it.
// [Perhaps this should go in an RTOS-specific Core HAL or BSP.  TBD.]

DECLFUNC(xthal_set_cpenable)
	abi_entry
#if XCHAL_HAVE_CP
	//rsr.cpenable	a3		// return previous CPENABLE
	movi	a3, 0		// for now, always return 0 (VxWorks currently done that way)

	wsr.cpenable	a2
#else
	movi	a3, 0	// if no CPENABLE (no coprocessors), none is ever enabled
#endif
	abi_return
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

