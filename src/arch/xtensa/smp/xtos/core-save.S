// core-save.S  --  core state save/restore routines (used by PSO)
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/xtos/core-save.S#1 $

// Copyright (c) 2012-2013 Tensilica Inc.
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

#include <xtensa/coreasm.h>
#include <xtensa/corebits.h>
#include <xtensa/cacheasm.h>
#include <xtensa/cacheattrasm.h>
#include <xtensa/xdm-regs.h>
#include <xtensa/xtruntime-core-state.h>
#include <xtensa/mpuasm.h>
#include "xtos-internal.h"

.weak	_idma_pso_save
//.type	xtos_C_core_save, @function


	.text


	//  (Place this alternate entry symbol *outside* the _xtos_core_save()
	//   function, to avoid confusing debugging / profiling / etc.)
	.align	4
	.global	_xtos_core_save_entry
	.type _xtos_core_save_entry,@function
_xtos_core_save_entry:
	j	.Lcore_save
	.size _xtos_core_save_entry, . - _xtos_core_save_entry


	//  int  _xtos_core_save(unsigned flags, XtosCoreState *savearea, void *code)
	//
	//  Generic processor state save routine.
	//
	//  On entry (after ENTRY if windowed):
	//	a0 = return PC
	//	a2 = flags argument
	//	a3 = ptr to save area
	//	a4 = ptr to code to jump to after save (just return if 0)
	//  Returns:
	//	0 when first returning from this call (if a4 == 0)
	//	non-zero (passed from restore call) when returning from restore
	//	(if a4 != 0, return behavior if any depends on code at a4)
	//
	.align	4
	.global	_xtos_core_save
	.type _xtos_core_save,@function
_xtos_core_save:
	abi_entry

.Lcore_save:
	s32i	a0, a3, CS_SA_areg + 0*4	// save a0 (clobbered below)
	s32i	a1, a3, CS_SA_areg + 1*4	// save a1
	s32i	a2, a3, CS_SA_areg + 2*4	// save a2 (flags arg, for debugging only)
	s32i	a4, a3, CS_SA_areg + 4*4	// save a4 (code to jump to after saving)
#ifdef __XTENSA_CALL0_ABI__
	//  Callee-saved regs:
	s32i	a12, a3, CS_SA_areg + 12*4	// save a12
	s32i	a13, a3, CS_SA_areg + 13*4	// save a13
	s32i	a14, a3, CS_SA_areg + 14*4	// save a14
	s32i	a15, a3, CS_SA_areg + 15*4	// save a15
#else
	call4	xthal_window_spill		// spill live caller windows to stack
#endif
	j	.Ls1

	.align	16
.Ls1:
#if XCHAL_HAVE_INTERRUPTS
	rsil	a4, 15				// disable interrupts before clobbering a0
#elif XCHAL_HAVE_EXCEPTIONS
	rsr.ps	a4
#endif

#if XCHAL_HAVE_EXCEPTIONS
	s32i	a4, a3, CS_SA_ps		// save PS
#endif
  
#if XCHAL_HAVE_IDMA
	movi	a4, _idma_pso_save
	beqz	a4, .LnoIDMA
#  ifdef __XTENSA_CALL0_ABI__
	mov a13, a3
	mov a12, a2
	addi	a3, a3, CS_SA_idmaregs	    // location for IDMA regs save
	call0	_idma_pso_save
	mov a3, a13
	mov a2, a12
#  else
	mov    a6, a2
	addi   a7, a3, CS_SA_idmaregs	    // location for IDMA regs save
	call4  _idma_pso_save
#  endif
.LnoIDMA:
#endif 

// not yet implemented
//#  ifdef __XTENSA_CALL0_ABI__
//	mov a13, a3
//	mov a12, a2
//	call0	xtos_C_core_save
//	mov a3, a13
//	mov a2, a12
//#  else
//	mov    a6, a2
//	mov    a7, a3
//	call4  xtos_C_core_save
//#  endif
//#endif 
  
#if XCHAL_HAVE_CCOUNT
	rsr.ccount	a5			// save CCOUNT restore value
#endif
#if XCHAL_HAVE_INTERRUPTS
	rsr.interrupt	a6			// save pending interrupts
	s32i	a6, a3, CS_SA_interrupt
#endif
#if XCHAL_HAVE_CCOUNT
	s32i	a5, a3, CS_SA_ccount
#endif

	call0	_xtos_core_save_common		// save and shutoff -- returns after wakeup

	//  a2 now contains return value.
	//  a3 still points to save area.
	//  Interrupts still disabled.

	//  Restore WINDOWSTART to single window.  Leave WINDOWBASE wherever it is.
	//rsr.windowbase	a6
	//movi	a5, 1
	//ssl	a6
	//sll	a5, a5
	//wsr.windowstart	a5
	//rsync

	l32i	a0, a3, CS_SA_areg + 0*4	// restore a0
	l32i	a1, a3, CS_SA_areg + 1*4	// restore a1
#ifdef __XTENSA_CALL0_ABI__
	//  Callee-saved regs:
	l32i	a12, a3, CS_SA_areg + 12*4	// restore a12
	l32i	a13, a3, CS_SA_areg + 13*4	// restore a13
	l32i	a14, a3, CS_SA_areg + 14*4	// restore a14
	l32i	a15, a3, CS_SA_areg + 15*4	// restore a15
#endif

#if XCHAL_HAVE_EXCEPTIONS
	//  Now that we've restored windowed state (a0,a1), we can restore interrupts.
	l32i	a4, a3, CS_SA_ps		// restore ps
	wsr.ps	a4
	rsync
#endif

	abi_return



	//  Generic processor state save routine, callable from assembly-level
	//  (Does not assume valid stack, saves all ARs, no window-spill etc.)
	//
	//  On entry:
	//	a0 = return PC
	//	a2 = flags argument
	//	a3 = ptr to save area
	//	a4 = ptr to code to jump to after save (just return if 0)
	//  All other registers are saved.
	//  Returns:
	//	0 when first returning from this call (if a4 == 0)
	//	non-zero (passed from restore call) when returning from restore
	//	(if a4 != 0, return behavior if any depends on code at a4)
	//
	.align	4
	.global	_xtos_core_save_nw
_xtos_core_save_nw:
	s32i	a0, a3, CS_SA_areg + 0*4	// save a0 (clobbered below)
	s32i	a1, a3, CS_SA_areg + 1*4	// save a1
	s32i	a2, a3, CS_SA_areg + 2*4	// save a2 (flags arg, for debugging only)
	s32i	a4, a3, CS_SA_areg + 4*4	// save a4 (code to jump to after saving)
	s32i	a5, a3, CS_SA_areg + 5*4	// save a5
	s32i	a6, a3, CS_SA_areg + 6*4	// save a6
	s32i	a7, a3, CS_SA_areg + 7*4	// save a7
	j	.Ls2

	.align 16
.Ls2:
#if XCHAL_HAVE_INTERRUPTS
	rsil	a4, 15				// disable interrupts before rotating etc
#elif XCHAL_HAVE_EXCEPTIONS
	rsr.ps	a4
#endif

#if XCHAL_HAVE_EXCEPTIONS
	s32i	a4, a3, CS_SA_ps		// save PS
#endif

#if XCHAL_HAVE_CCOUNT
	rsr.ccount	a5			// save CCOUNT restore value
#endif
#if XCHAL_HAVE_INTERRUPTS
	rsr.interrupt	a6			// save pending interrupts
        s32i    a6, a3, CS_SA_interrupt
#endif
#if XCHAL_HAVE_CCOUNT
        s32i    a5, a3, CS_SA_ccount
#endif

#if XCHAL_HAVE_WINDOWED
	movi	a5, XCHAL_NUM_AREGS / 8 - 1	// number of 8-reg chunks to save (a0-a7 already done)
#endif
1:	s32i	a8, a3, CS_SA_areg + 8*4	// save a8
	s32i	a9, a3, CS_SA_areg + 9*4	// save a9
	s32i	a10,a3, CS_SA_areg + 10*4	// save a10
	s32i	a11,a3, CS_SA_areg + 11*4	// save a11
	s32i	a12,a3, CS_SA_areg + 12*4	// save a12
	s32i	a13,a3, CS_SA_areg + 13*4	// save a13
	s32i	a14,a3, CS_SA_areg + 14*4	// save a14
	s32i	a15,a3, CS_SA_areg + 15*4	// save a15
#if XCHAL_HAVE_WINDOWED
	addi	a11, a3, 8*4			// next frame (a11 will become a3, a13 become a5)
	addi	a13, a5, -1
	rotw	2
	bnez	a5, 1b				// loop until all frames done
	rotw	2				// back to starting windowbase
#endif

	movi	a1, 0				// not to save any regs from stack
	call0	_xtos_core_save_common

	//  a2 now contains return value.
	//  a3 still points to save area.
	//  Interrupts still disabled.

#if XCHAL_HAVE_WINDOWED
	rotw	-2
	movi	a5, XCHAL_NUM_AREGS / 8 - 1	// 8-reg chunks to restore (a0-a7 already done)
	addi	a3, a11, XCHAL_NUM_AREGS * 4
1:	rotw	-2
	addi	a3, a11, -8*4
	addi	a5, a13, -1
#endif
	l32i	a8, a3, CS_SA_areg + 8*4	// restore a8
	l32i	a9, a3, CS_SA_areg + 9*4	// restore a9
	l32i	a10,a3, CS_SA_areg + 10*4	// restore a10
	l32i	a11,a3, CS_SA_areg + 11*4	// restore a11
	l32i	a12,a3, CS_SA_areg + 12*4	// restore a12
	l32i	a13,a3, CS_SA_areg + 13*4	// restore a13
	l32i	a14,a3, CS_SA_areg + 14*4	// restore a14
	l32i	a15,a3, CS_SA_areg + 15*4	// restore a15
#if XCHAL_HAVE_WINDOWED
	bnez	a5, 1b				// loop until all frames done
	//  We're now back to starting windowbase, and original a3.
#endif

	l32i	a0, a3, CS_SA_areg + 0*4	// restore a0
	l32i	a1, a3, CS_SA_areg + 1*4	// restore a1
	//  Don't clobber return value, so don't restore a2.
	l32i	a4, a3, CS_SA_areg + 4*4	// restore a4
	l32i	a5, a3, CS_SA_areg + 5*4	// restore a5
	l32i	a6, a3, CS_SA_areg + 6*4	// restore a6
#if XCHAL_HAVE_EXCEPTIONS
	//  Now that we've restored windowed state (a0,a1,done rotating), we can restore interrupts.
	l32i	a7, a3, CS_SA_ps		// restore ps
	wsr.ps	a7
	rsync
#endif
	l32i	a7, a3, CS_SA_areg + 7*4	// restore a7
	ret




	//  Common state save / shut-off code.
	//
	//	a0 = return PC within caller shut-off routine
	//	a1 = stack if != 0
	//	a2 = flags argument
	//	a3 = pointer to _xtos_pso_savearea
	//	a4 = PS to save/restore
	//	PS.INTLEVEL = 15  (interrupts disabled, except NMI)
	//	a5-a15 (and other ARs) are available.
	//	NOTE: CCOUNT and INTERRUPT have already been saved in save area.
	//
	.align	4
	//.global	_xtos_core_save_common
_xtos_core_save_common:
//#if XCHAL_HAVE_EXCEPTIONS
//	s32i	a4, a3, CS_SA_ps		// save PS
//#endif

#if XCHAL_HAVE_CACHE_BLOCKOPS
	pfend.o				// terminate non-essential block-prefetch ops
#endif

#if XCHAL_HAVE_WINDOWED
	// The following discussion is valid if we have a stack:
	// At this point, all non-live register windows have been spilled to the
	// stack. However, we cannot leave any spilled registers in our stack frame
	// or our caller's stack frame, since these frames could change after we
	// return and before restore() is called. So all spilled registers in the
	// current and previous stack frames must be saved to the save area. This
	// means a max of 16 registers: 4 base save registers for our caller, upto
	// 8 extra save registers for our caller, and 4 base save registers for the
	// next function up from our caller. The stack looks like this:
	//
	//	------------------------------- <---- stack ptr of function (i - 2)
	//	  Base save area i - 3
	//	-------------------------------
	//	  Extra save area i - 1
	//	  (0-8 registers depending on call type)
	//	-------------------------------
	//	  Locals i - 1
	//	------------------------------- <---- stack ptr of function (i - 1)
	//	  Base save area i - 2                (our caller)
	//
	//	------------------------------- <---- Our stack ptr (a1)
	//	  Base save area i - 1
	//	-------------------------------
	//
	// We don't have any extra save area or locals in our frame. See the
	// Xtensa Programmer's Guide for more details of the stack layout.
	//
	// NOTE that we are not counting the call0 to _xtos_core_save_common() since
	// that does not result in any register window rotation nor stack ptr change.

	s32i	a1, a3, CS_SA_caller_regs_saved	// save flag
	beqz	a1, .Lendcr			// skip if no stack

	// Save our caller's a0-a3 from the base save area (a1-16)

	addi	a4, a1, -16
	l32i	a5, a4, 0
	l32i	a6, a4, 4
	s32i	a5, a3, CS_SA_caller_regs	// caller a0
	s32i	a6, a3, CS_SA_caller_regs + 4	// caller a1
	l32i	a5, a4, 8
	l32i	a6, a4, 12
	s32i	a5, a3, CS_SA_caller_regs + 8	// caller a2
	s32i	a6, a3, CS_SA_caller_regs + 12	// caller a3

	// Save our callers caller's a0-a3 from its base save area (a1+0)

	l32i	a5, a1, 0
	l32i	a6, a1, 4
	s32i	a5, a3, CS_SA_caller_regs + 16  // caller caller a0
	s32i	a6, a3, CS_SA_caller_regs + 20  // caller caller a1
	l32i	a5, a1, 8
	l32i	a6, a1, 12
	s32i	a5, a3, CS_SA_caller_regs + 24  // caller caller a2
	s32i	a6, a3, CS_SA_caller_regs + 28  // caller caller a3

	// Now save 0-8 registers for our caller from its ext save area
	// NOTE we can't use a0 directly because we are one level down

	l32i	a4, a3, CS_SA_areg		// pull in the return address
	extui	a4, a4, 30, 2			// Top 2 bits of ret addr
	blti	a4, 2, .Lendcr			// No regs to save
	l32i	a5, a1, 4			// a5 <- caller caller a1
	slli	a4, a4, 4
	sub	a4, a5, a4			// a4 <- bottom of extra save area
	addi	a5, a5, -16			// a5 <- top of extra save area
	addi	a6, a3, CS_SA_caller_regs + 32	// location to start saving to
.Lcrloop:
	l32i	a7, a4, 0			// Save in groups of 4 registers
	l32i	a8, a4, 4
	s32i	a7, a6, 0
	s32i	a8, a6, 4
	l32i	a7, a4, 8
	l32i	a8, a4, 12
	s32i	a7, a6, 8
	s32i	a8, a6, 12
	addi	a4, a4, 16
	addi	a6, a6, 16
	blt	a4, a5, .Lcrloop
.Lendcr:
#endif

	// We want to save the CCOUNT value as soon as feasible after disabling
	// interrupts, so that the counter does not run past any CCOMPARE value
	// and miss a timer interrupt. The callers of this function have saved
	// the values of CCOUNT and INTERRUPT immediately after disabling interrupts.

#if XCHAL_HAVE_CCOUNT
	.set	_idx, 0
	.rept	XCHAL_NUM_TIMERS
	INDEX_SR rsr.ccompare a5
	s32i	a5, a3, CS_SA_ccompare + 4*_idx
	.set	_idx, _idx+1
        .endr
#endif

	s32i	a0, a3, CS_SA_restore_label	// where to return to, to return from function
#if XCHAL_HAVE_INTERRUPTS || XCHAL_HAVE_EXCEPTIONS
	rsr.epc1	a5
	s32i	a5, a3, CS_SA_epc1
	rsr.excsave1	a5
	s32i	a5, a3, CS_SA_excsave1
# ifdef XCHAL_DOUBLEEXC_VECTOR_VADDR
	rsr.depc	a5
	s32i	a5, a3, CS_SA_depc
# endif
#endif
#if XCHAL_HAVE_WINDOWED
	rsr.windowbase	a5
	s32i	a5, a3, CS_SA_windowbase	// save windowbase
	rsr.windowstart	a5
	s32i	a5, a3, CS_SA_windowstart	// save windowstart
#endif
	rsr.sar	a5
	s32i	a5, a3, CS_SA_sar		// save sar

#if XCHAL_HAVE_PSO_CDM
	//  Save PWRCTL, and update according to flags argument.
	movi	a4, XDM_MISC_PWRCTL
	movi	a6, PWRCTL_MEM_WAKEUP
	rer	a7, a4				// get pwrctl
	s32i	a7, a3, CS_SA_pwrctl		// save pwrctl
	//  Avoid setting power-control bits if not already set, i.e. clear them only.
	bbci.l	a2, XTOS_COREF_PSO_SHIFT, 1f	// if not shutting off, don't touch power bits

	//  Set PWRCTL MEM_WAKEUP bit according to flags (whether to let mem power off).
	or	a5, a7, a6	// set...
	xor	a5, a5, a6	// ... and clear MEM_WAKEUP bit to write
	and	a6, a2, a6	// isolate MEM_WAKEUP bit from flags
	or	a5, a5, a6	// set MEM_WAKEUP bit to write from flags
	//  Clear PWRCTL DEBUG_WAKEUP bit if cleared in flags (if letting debug power off).
	movi	a6, ~PWRCTL_DEBUG_WAKEUP
	or	a6, a2, a6	// isolate DEBUG_WAKEUP bit from flags
	and	a6, a5, a6	// clear it if was clear in flags
	//  Update PWRCTL
	wer	a6, a4		// write new pwrctl
	//extw			// let the new pwrctl value settle
1:
#endif

	.set	_idx, 2
	.rept	XCHAL_NUM_INTLEVELS+XCHAL_HAVE_NMI-1
	INDEX_SR rsr.epc a5
	s32i	a5, a3, CS_SA_epc + 4*(_idx-2)
	INDEX_SR rsr.eps a5
	s32i	a5, a3, CS_SA_eps + 4*(_idx-2)
	INDEX_SR rsr.excsave a5
	s32i	a5, a3, CS_SA_excsave + 4*(_idx-2)
	.set	_idx, _idx+1
	.endr

#if XCHAL_HAVE_LOOPS
	rsr.lbeg	a5
	s32i	a5, a3, CS_SA_lbeg
	rsr.lend	a5
	s32i	a5, a3, CS_SA_lend
	rsr.lcount	a5
	s32i	a5, a3, CS_SA_lcount
#endif
#if XCHAL_HAVE_ABSOLUTE_LITERALS
	rsr.litbase	a5
	s32i	a5, a3, CS_SA_litbase
#endif
#if XCHAL_HAVE_VECBASE
	rsr.vecbase	a5
	s32i	a5, a3, CS_SA_vecbase
#endif
#if XCHAL_HAVE_S32C1I && (XCHAL_HW_MIN_VERSION >= XTENSA_HWVERSION_RC_2009_0)	/* have ATOMCTL ? */
	rsr.atomctl	a5
	s32i	a5, a3, CS_SA_atomctl
#endif
#if XCHAL_HAVE_PREFETCH
	movi	a5, 0			// disable prefetch during shutoff
	xsr.prefctl	a5
	s32i	a5, a3, CS_SA_prefctl
#endif
#if XCHAL_USE_MEMCTL
	rsr.memctl	a5
	s32i	a5, a3, CS_SA_memctl
#endif
#if XCHAL_HAVE_INTERRUPTS
	rsr.intenable	a5
	s32i	a5, a3, CS_SA_intenable
#endif
#if XCHAL_HAVE_DEBUG
	//  NOTE:  restore of debug state is conditional,
	//  as the power-down and wakeup code might be actively debugged.
	rsr.icount	a5
	s32i	a5, a3, CS_SA_icount
	rsr.icountlevel	a5
	s32i	a5, a3, CS_SA_icountlevel
	rsr.debugcause	a5
	s32i	a5, a3, CS_SA_debugcause	// (won't get restored?)
	//rsr.ddr	a5
	//s32i	a5, a3, CS_SA_ddr	
# if XCHAL_NUM_IBREAK
	rsr.ibreakenable	a5
	s32i	a5, a3, CS_SA_ibreakenable
# endif
	.set	_idx, 0
	.rept	XCHAL_NUM_IBREAK
	INDEX_SR rsr.ibreaka a5
	s32i	a5, a3, CS_SA_ibreaka + 4*_idx
	.set	_idx, _idx+1
	.endr
	.set	_idx, 0
	.rept	XCHAL_NUM_DBREAK
	INDEX_SR rsr.dbreakc a5
	s32i	a5, a3, CS_SA_dbreakc + 4*_idx
	INDEX_SR rsr.dbreaka a5
	s32i	a5, a3, CS_SA_dbreaka + 4*_idx
	.set	_idx, _idx+1
	.endr
#endif

	.set	_idx, 0
	.rept	XCHAL_NUM_MISC_REGS
	INDEX_SR rsr.misc a5
	s32i	a5, a3, CS_SA_misc + 4*_idx
	.set	_idx, _idx+1
	.endr

#if XCHAL_HAVE_MEM_ECC_PARITY
	rsr.mepc	a5
	s32i	a5, a3, CS_SA_mepc
	rsr.meps	a5
	s32i	a5, a3, CS_SA_meps
	rsr.mesave	a5
	s32i	a5, a3, CS_SA_mesave
	rsr.mesr	a5
	s32i	a5, a3, CS_SA_mesr
	rsr.mecr	a5
	s32i	a5, a3, CS_SA_mecr
	rsr.mevaddr	a5
	s32i	a5, a3, CS_SA_mevaddr
#endif

	/*  TIE state  */
	addi	a4, a3, CS_SA_ncp
	xchal_ncp_store	a4, a5,a6,a7,a8		// save non-coprocessor state
#if XCHAL_HAVE_CP
	rsr.cpenable	a5
	s32i	a5, a3, CS_SA_cpenable
	movi	a6, -1
	wsr.cpenable	a6			// enable all coprocessors
	rsync
	xchal_cp0_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp1_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp2_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp3_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp4_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp5_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp6_store  a4, a5,a6,a7,a8  continue=1
	xchal_cp7_store  a4, a5,a6,a7,a8  continue=1
	//xchal_cp8_store  a4, a5,a6,a7,a8  continue=1
	//xchal_cp9_store  a4, a5,a6,a7,a8  continue=1
	//xchal_cp10_store a4, a5,a6,a7,a8  continue=1
	//xchal_cp11_store a4, a5,a6,a7,a8  continue=1
	//xchal_cp12_store a4, a5,a6,a7,a8  continue=1
	//xchal_cp13_store a4, a5,a6,a7,a8  continue=1
	//xchal_cp14_store a4, a5,a6,a7,a8  continue=1
	//xchal_cp15_store a4, a5,a6,a7,a8  continue=1
#endif

	/*  TLB state (for known MMU types only, not internal custom)  */
#if XCHAL_HAVE_MIMIC_CACHEATTR || XCHAL_HAVE_XLT_CACHEATTR
	addi	a4, a3, CS_SA_tlbs	// where to start storing TLB entry info
	movi	a5, 0x20000000
	movi	a6, 0
1:	rdtlb1	a7, a6			// read DTLB entry PPN + CA
	s32i	a7, a4, 0
	ritlb1	a7, a6			// read ITLB entry PPN + CA
	s32i	a7, a4, 4
	addi	a4, a4, 8
	add	a6, a6, a5
	bnez	a6, 1b

#elif XCHAL_HAVE_PTP_MMU
	//  Declare a table of TLB entries to save/restore.
	//  Each entry is a 32-bit index to use directly with [rw][di]tlb[01].
	//  Indices assume ITLBCFG == DTLBCFG == 0.
	//  Bit 4 means not-for-dtlb, and bit 5 means not-for-itlb
	//  (these bits aren't used by these instructions, so okay to use for this).
	.section .rodata, "a"
	.global _xtos_pso_tlbmap
	.global _xtos_pso_tlbmap_end
	.type _xtos_pso_tlbmap, @object
_xtos_pso_tlbmap:
	.long	0x0C0C0C0C, ARF_ENTRIES	// *TLB way 0, 4/8 entries of 4KB
	.long	0x0C0C0C0C, ARF_ENTRIES	// *TLB way 1, 4/8 entries of 4KB
	.long	0x0C0C0C0C, ARF_ENTRIES	// *TLB way 2, 4/8 entries of 4KB
	.long	0x0C0C0C0C, ARF_ENTRIES	// *TLB way 3, 4/8 entries of 4KB
	.long	0x1A181614, 4		// *TLB way 4, 4 entries of 1MB/4MB/16MB/64MB
# if XCHAL_HAVE_SPANNING_WAY	/* MMU v3 */
	.long	0x1C1B1C1B, 4		// *TLB way 5, 4 entries of 128MB/256MB
	.long	0x1B1D1B1D, 8		// *TLB way 6, 8 entries of 512MB/128MB
# endif
	.long	0x0C0C0C0C, 0x8001	// DTLB way 7, 1 entry of 4KB
	.long	0x0C0C0C0C, 0x8001	// DTLB way 8, 1 entry of 4KB
	.long	0x0C0C0C0C, 0x8001	// DTLB way 9, 1 entry of 4KB
_xtos_pso_tlbmap_end:
	.size _xtos_pso_tlbmap, . - _xtos_pso_tlbmap

	.text
	addi	a4, a3, CS_SA_tlbs	// where to start storing TLB entry info
	movi	a10, _xtos_pso_tlbmap
	movi	a11, _xtos_pso_tlbmap_end
	rsr.dtlbcfg	a14		// page size index (0..3) for each DTLB way
	rsr.itlbcfg	a15		// page size index (0..3) for each ITLB way
	s32i	a14, a3, CS_SA_dtlbcfg
	s32i	a15, a3, CS_SA_itlbcfg
	rsr.ptevaddr	a5
	s32i	a5, a3, CS_SA_ptevaddr
	rsr.rasid	a5
	s32i	a5, a3, CS_SA_rasid
	//  Loop from last way to first (less register pressure that way).
.Loop_tlbmap:
	addi	a11, a11, -8		// next way
	l32i	a8, a11, 0		// map of four (page size log2) per index for this way
	// DTLB page size:
	extui	a12, a14, 0, 4		// page size index for this DTLB way
	srli	a14, a14, 4		// (for next way)
	ssa8l	a12			// prepare to shift right by 8*a12
	srl	a12, a8			// page size log2 for this DTLB way
	ssl	a12			// prepare to shift left by a12
	movi	a12, 1			// (to compute 1 << (page size log2))
	sll	a12, a12		// page size for this DTLB way

	//  Save all entries of this DTLB way:
	l32i	a9, a11, 4		// number of entries for this way
	sub	a5, a11, a10		// way number * 8
	srli	a5, a5, 3		// way number
	extui	a9, a9, 0, 8
1:	rdtlb0	a6, a5			// read DTLB entry VPN + ASID ...
	rdtlb1	a7, a5			// read DTLB entry PPN + CA ...
	add	a5, a5, a12		// next entry of this DTLB way
	s32i	a6, a4, 0		// save entry ...
	s32i	a7, a4, 4
	addi	a4, a4, 8
	addi	a9, a9, -1
	bnez	a9, 1b

	// ITLB page size:
	extui	a12, a15, 0, 4		// page size index for this ITLB way
	srli	a15, a15, 4		// (for next way)
	ssa8l	a12			// prepare to shift right by 8*a12
	srl	a12, a8			// page size log2 for this ITLB way
	ssl	a12			// prepare to shift left by a12
	movi	a12, 1			// (to compute 1 << (page size log2))
	sll	a12, a12		// page size for this ITLB way

	//  Save all entries of this ITLB way:
	l32i	a9, a11, 4		// number of entries for this way
	sub	a5, a11, a10		// way number * 8
	srli	a5, a5, 3		// way number
	bbsi.l	a9, 15, 2f		// skip ITLB if is a DTLB-only way
	extui	a9, a9, 0, 8
1:	ritlb0	a6, a5			// read ITLB entry VPN + ASID ...
	ritlb1	a7, a5			// read ITLB entry PPN + CA ...
	add	a5, a5, a12		// next entry of this ITLB way
	s32i	a6, a4, 0		// save entry ...
	s32i	a7, a4, 4
	addi	a4, a4, 8
	addi	a9, a9, -1
	bnez	a9, 1b
2:
	bne	a11, a10, .Loop_tlbmap	// loop for next TLB way
	//  Done saving TLBs.
#endif

#if XCHAL_HAVE_CACHE_BLOCKOPS
	pfwait.a			// wait for any remaining block-prefetch ops
#endif

#if XCHAL_HAVE_MPU
	addi	a4, a3, CS_SA_mpuentry	// location for MPU save
	mpu_read_map  a4, a5, a6
	rsr.cacheadrdis a4
	addi    a5, a3, CS_SA_cacheadrdis
	s32i    a4, a5, 0

#if XCHAL_DCACHE_IS_WRITEBACK
	//  Must write this piece back to memory, because if it stays
	//  in the cache and we try to restore with caches bypassed,
	//  the wrong values will be fetched from memory.
	//  TODO: See if possible to replace with call to xthal_dcache_region_writeback
	//  TODO: If going to write back full dcache below, skip this step
	addi	a4, a3, CS_SA_mpuentry
	movi	a5, CS_SA_ncp - CS_SA_mpuentry
	dcache_writeback_region a4, a5, a7, a8
#endif
#endif

	//  With data cache coherency enabled, need a full data cache
	//  writeback and invalidate, then disable coherency, before shut-off.
	//  Otherwise, if we'll let dcache power off, writeback its contents.
	//
	//  We make sure the signature only gets written after everything
	//  else is written back (if we writeback), and only gets written
	//  back if the rest gets written back.
	movi	a6, CORE_STATE_SIGNATURE
#if XCHAL_DCACHE_IS_WRITEBACK
# if XCHAL_HAVE_PSO_CDM && XCHAL_DCACHE_IS_COHERENT && XCHAL_HW_MIN_VERSION >= XTENSA_HWVERSION_RE_2012_0
	rsr.memctl	a4
	bbci.l	a2, XTOS_COREF_PSO_SHIFT, 1f	// if not shutting off, leave snoops as is
	bbci.l	a4, MEMCTL_SNOOP_EN_SHIFT, 1f	// snoops (coherence) enabled?
	dcache_writeback_inv_all a4, a5, a7, 0	// yes: writeback-invalidate
	memw					// wait for writeback to complete
	s32i	a6, a3, CS_SA_signature
	dhwbi	a3, CS_SA_signature
	//  Now that dcache is empty, make sure snoops are off during shut-off.
	addi	a4, a4, -MEMCTL_SNOOP_EN
	wsr.memctl	a4
	j	9f
1:
# endif
	bbsi.l	a2, PWRCTL_MEM_WAKEUP_SHIFT, 7f	// letting caches power off?
	dcache_writeback_all	a4, a5, a7, 0	// yes: writeback
	memw					// wait for writeback to complete
	j	8f

	// The signature and the cache/TLB state must be written out to
	// main memory even though the caches stay on, because on restart
	// we will come up with caches bypassed and need to read the state
	// back before the cache/TLB is set up.
7:
        mov     a4, a3
        movi    a5, CS_SA_ncp
        dcache_writeback_region a4, a5, a7, a8
        memw
8:
	s32i	a6, a3, CS_SA_signature
	dhwb	a3, CS_SA_signature		// needed even if caches stay on
#else
	s32i	a6, a3, CS_SA_signature
#endif

9:	l32i	a4, a3, CS_SA_areg + 4*4	// restore a4 (code to jump to after saving)
	memw					// wait for signature to be in memory

	beqz	a4, 1f				// code to jump to?
	jx	a4				// yes, jump to it
1:	l32i	a0, a3, CS_SA_restore_label	// no, return:  restore return PC
	movi	a2, 0				// return 0
	ret


	.size	_xtos_core_save, . - _xtos_core_save

