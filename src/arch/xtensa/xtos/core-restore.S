// core-restore.S  --  core state restore routine (used by PSO)
// $Id: //depot/rel/Eaglenest/Xtensa/OS/xtos/core-restore.S#1 $

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
#include <xtensa/specreg.h>
#include <xtensa/cacheasm.h>
#include <xtensa/cacheattrasm.h>
#include <xtensa/xdm-regs.h>
#include <xtensa/config/specreg.h>
#include <xtensa/config/core-isa.h>
#include <xtensa/xtruntime-core-state.h>
#include "xtos-internal.h"


	.text



	//  void  _xtos_core_restore(unsigned retvalue, XtosCoreState *savearea)
	//
	//  Restore processor state.
	//  If save area signature is bad, function just returns.
	//  Else, processor state is restored, and execution resumes
	//  according to that saved processor state.
	//
	//  On entry:
	//	Caches are ready to use (initialized or warm, as the case may be).
	//
	.align	4
	.global	_xtos_core_restore
	.type _xtos_core_restore,@function
_xtos_core_restore:
	abi_entry

	//  Check the save area's signature:
	movi	a5, CORE_STATE_SIGNATURE
	l32i	a4, a3, CS_SA_signature
	movi	a6, 0
	bne	a4, a5, 1f			// if bad, just return

#if XCHAL_HAVE_INTERRUPTS
	rsil	a4, 15				// disable interrupts...
	wsr	a6, INTENABLE
#endif

	//  Here, call0 is used as an unlimited range jump.  It does not return.
	call0	_xtos_core_restore_nw

1:	abi_return

	.size	_xtos_core_restore, . - _xtos_core_restore



	//  Restore processor state.
	//  On entry:
	//	Caches are ready to use (initialized or warm, as the case may be).
	//	a2 = return value passed to restored processor state
	//	a3 = pointer to save area to restore from
	//	INTENABLE = 0  (interrupts all disabled)
	//	LITBASE = initialized (per reset vector, not restored)
	//	touching a4..a7 won't overflow
	//	other registers are mostly undefined
	//
	.align	4
	.global	_xtos_core_restore_nw
	.type _xtos_core_restore_nw,@function
_xtos_core_restore_nw:

#if XCHAL_HAVE_WINDOWED
	s32i	a2, a3, CS_SA_areg + 2*4	// save a2 thru rotation
	wsr	a3, EXCSAVE1			// save a3 thru rotation
	l32i	a6, a3, CS_SA_windowstart	// restore windowstart
	l32i	a5, a3, CS_SA_windowbase	// restore windowbase
	wsr	a6, WINDOWSTART
	wsr	a5, WINDOWBASE
	rsync
	//  a0-a15 have possibly all changed, so need to reload a3
	rsr	a3, EXCSAVE1			// restore a3
	l32i	a2, a3, CS_SA_areg + 2*4	// restore a2 (return value)
#endif

	//movi	a0, 0
	l32i	a0, a3, CS_SA_restore_label     // _xtos_core_save_common's return PC

	//  Just for consistency...
#if XCHAL_HAVE_INTERRUPTS || XCHAL_HAVE_EXCEPTIONS
	movi	a4, 0x11
	wsr	a4, ps
	rsync
#endif

	l32i	a5, a3, CS_SA_sar		// restore sar
	wsr	a5, sar

#if XCHAL_HAVE_PSO_CDM
	//  Restore PWRCTL (except ShutProcOffOnPWait, cleared when all is done).
	movi	a4, XDM_MISC_PWRCTL
	movi	a7, PWRCTL_CORE_SHUTOFF		// aka ShutProcOffOnPWait
	rer	a6, a4				// read pwrctl
	l32i	a5, a3, CS_SA_pwrctl		// get saved pwrctl
	and	a7, a7, a6			// keep just ShutProcOffOnPWait bit
	or	a5, a5, a7			// keep it set if already set (clear later)
	wer	a5, a4				// restore pwrctl (except ShutProcOffOnPWait)
#endif

	.macro	rest_level	num
	 l32i	a5, a3, CS_SA_epc + 4*\num
	 wsr	a5, EPC_2 + \num
	 l32i	a5, a3, CS_SA_eps + 4*\num
	 wsr	a5, EPS_2 + \num
	 l32i	a5, a3, CS_SA_excsave + 4*\num
	 wsr	a5, EXCSAVE_2 + \num
	.endm
	iterate	0, XCHAL_NUM_INTLEVELS+XCHAL_HAVE_NMI-2, rest_level

#if XCHAL_HAVE_LOOPS
	l32i	a5, a3, CS_SA_lbeg
	wsr	a5, LBEG
	l32i	a5, a3, CS_SA_lend
	wsr	a5, LEND
	l32i	a5, a3, CS_SA_lcount
	wsr	a5, LCOUNT
#endif
#if XCHAL_HAVE_ABSOLUTE_LITERALS
	l32i	a5, a3, CS_SA_litbase
	wsr	a5, LITBASE
#endif
#if XCHAL_HAVE_VECBASE
	l32i	a5, a3, CS_SA_vecbase
	wsr	a5, VECBASE
#endif
#if XCHAL_HAVE_S32C1I && (XCHAL_HW_MIN_VERSION >= XTENSA_HWVERSION_RC_2009_0)	/* have ATOMCTL ? */
	l32i	a5, a3, CS_SA_atomctl
	wsr	a5, ATOMCTL
#endif
#if XCHAL_HAVE_PREFETCH
	l32i	a5, a3, CS_SA_prefctl
	wsr	a5, PREFCTL
#endif
#if XCHAL_USE_MEMCTL
	l32i	a5, a3, CS_SA_memctl
	wsr	a5, MEMCTL
#endif
#if XCHAL_HAVE_DEBUG
	//  NOTE:  restore of debug state is conditional,
	//  as the power-down and wakeup code might be actively debugged.
	rsr	a5, ICOUNTLEVEL
	bnez	a5, 1f				// skip if being single-stepped (not failsafe!)
	l32i	a5, a3, CS_SA_icount
	wsr	a5, ICOUNT
	l32i	a5, a3, CS_SA_icountlevel
	wsr	a5, ICOUNTLEVEL
1:
	//l32i	a5, a3, CS_SA_debugcause	// (won't get restored?)
	//wsr	a5, DEBUGCAUSE
	//l32i	a5, a3, CS_SA_ddr	
	//wsr	a5, DDR
# if XCHAL_NUM_IBREAK
	rsr	a6, IBREAKENABLE
	bnez	a5, 1f				// skip restore if already some ibreaks defined
	.macro	rest_ibreak	num
	 l32i	a5, a3, CS_SA_ibreaka + 4*\num
	 wsr	a5, IBREAKA + \num
	.endm
	iterate	0, XCHAL_NUM_IBREAK-1, rest_ibreak
	l32i	a5, a3, CS_SA_ibreakenable
	wsr	a5, IBREAKENABLE
1:
# endif
	.macro	rest_dbreak	num
	 rsr	a6, DBREAKC + \num
	 bbsi.l	a6, 30, 1f			// skip restore of that dbreak if already active
	 bbsi.l	a6, 31, 1f			// ditto
	 l32i	a5, a3, CS_SA_dbreaka + 4*\num
	 wsr	a5, DBREAKA + \num
	 l32i	a5, a3, CS_SA_dbreakc + 4*\num
	 wsr	a5, DBREAKC + \num
1:
	.endm
	iterate	0, XCHAL_NUM_DBREAK-1, rest_dbreak
#endif
	.macro	rest_misc	num
	 l32i	a5, a3, CS_SA_misc + 4*\num
	 wsr	a5, MISC_REG_0 + \num
	.endm
	iterate	0, XCHAL_NUM_MISC_REGS-1, rest_misc
#if XCHAL_HAVE_MEM_ECC_PARITY
	l32i	a5, a3, CS_SA_mepc
	wsr	a5, MEPC
	l32i	a5, a3, CS_SA_meps
	wsr	a5, MEPS
	l32i	a5, a3, CS_SA_mesave
	wsr	a5, MESAVE
	l32i	a5, a3, CS_SA_mesr
	wsr	a5, MESR
	l32i	a5, a3, CS_SA_mecr
	wsr	a5, MECR
	l32i	a5, a3, CS_SA_mevaddr
	wsr	a5, MEVADDR
#endif

	/*  TIE state  */
	addi	a4, a3, CS_SA_ncp
	xchal_ncp_load	a4, a5,a6,a7,a8		// restore non-coprocessor state
#if XCHAL_HAVE_CP
	movi	a6, -1
	wsr	a6, CPENABLE			// enable all coprocessors
	rsync
	xchal_cp0_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp1_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp2_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp3_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp4_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp5_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp6_load  a4, a5,a6,a7,a8  continue=1
	xchal_cp7_load  a4, a5,a6,a7,a8  continue=1
	//xchal_cp8_load  a4, a5,a6,a7,a8  continue=1
	//xchal_cp9_load  a4, a5,a6,a7,a8  continue=1
	//xchal_cp10_load a4, a5,a6,a7,a8  continue=1
	//xchal_cp11_load a4, a5,a6,a7,a8  continue=1
	//xchal_cp12_load a4, a5,a6,a7,a8  continue=1
	//xchal_cp13_load a4, a5,a6,a7,a8  continue=1
	//xchal_cp14_load a4, a5,a6,a7,a8  continue=1
	//xchal_cp15_load a4, a5,a6,a7,a8  continue=1
	l32i	a5, a3, CS_SA_cpenable
	wsr	a5, CPENABLE
#endif

	/*  TLB state (for known MMU types only, not internal custom)  */
	//  FIXME FIXME FIXME TODO:
	//  This restore code does not work in the general case,
	//  for CaXLT or full MMU, in particular when any address mappings
	//  were active when saved, that don't match reset state and affect
	//  code and data currently being accessed for restore.
#if XCHAL_HAVE_MIMIC_CACHEATTR || XCHAL_HAVE_XLT_CACHEATTR
	addi	a4, a3, CS_SA_tlbs	// where to start loading TLB entry info
	movi	a5, 0x20000000
	movi	a6, 0
1:
	l32i	a7, a4, 0
	wdtlb	a7, a6			// write DTLB entry PPN + CA
	dsync
	l32i	a7, a4, 4
	j	2f
	//  Ensure WITLB and ISYNC are in same cache line, when writing ITLB 
	//  entry that maps this currently running code
	//  (micro-architecture defined sequence):
	.begin	no-transform
	.align	16
2:	witlb	a7, a6			// write ITLB entry PPN + CA
	isync
	.end	no-transform
	nop
	nop
	addi	a4, a4, 8
	add	a6, a6, a5
	bnez	a6, 1b

#elif XCHAL_HAVE_PTP_MMU
	addi	a4, a3, CS_SA_tlbs	// where to start storing TLB entry info
	movi	a10, _xtos_pso_tlbmap
	movi	a11, _xtos_pso_tlbmap_end
	l32i	a14, a3, CS_SA_dtlbcfg
	l32i	a15, a3, CS_SA_itlbcfg
	wsr	a14, dtlbcfg		// page size index (0..3) for each DTLB way
	wsr	a15, itlbcfg		// page size index (0..3) for each ITLB way
	l32i	a5, a3, CS_SA_ptevaddr
	wsr	a5, ptevaddr
	rsync
	//  Loop from last way to first (less register pressure that way).
.Loop_tlbmap_rest:
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

	//  Restore all entries of this DTLB way:
	l32i	a9, a11, 4		// number of entries for this way
	sub	a5, a11, a10		// way number * 8
	srli	a5, a5, 3		// way number
	extui	a9, a9, 0, 8
1:
	l32i	a6, a4, 0		// read entry VPN + ASID
	extui	a7, a6, 0, 8		// get ASID
	bnez	a7, 2f			// if non-zero, need WDTLB
	add	a6, a6, a5		// zero, so need IDTLB - add way number
	idtlb	a6			// invalidate DTLB entry
	j	5f
2:	//  Non-zero ASID.  Put in RASID and adjust PS.RING accordingly.
	bgeui	a7, 5, 3f		// branch if ASID >= 5
	addi	a7, a7, -1
	slli	a7, a7, 6		// PS.RING = ASID - 1
	addi	a7, a7, 0x11		// PS.EXCM=1, PS.INTLEVEL=1
	movi	a6, 0x04030201		// for ASID in {1 .. 4}
	j	4f
3:	// ASID >= 5, place it in RASID
	movi	a6, 0x00030201
	slli	a7, a7, 24
	add	a6, a7, a6		// RASID = 0x <ASID> 03 02 01
	movi	a7, 0xd1		// PS.RING=3, PS.EXCM=1, PS.INTLEVEL=1
4:	wsr	a6, rasid
	wsr	a7, ps
	rsync
	l32i	a6, a4, 0		// read entry VPN + ASID
	l32i	a7, a4, 4		// read entry PPN + CA
	srli	a6, a6, 8		// replace ASID ...
	slli	a6, a6, 8		// ...
	add	a6, a6, a5		// ... with way number
	wdtlb	a7, a6			// write DTLB entry ...
5:	dsync
	addi	a4, a4, 8
	add	a5, a5, a12		// next entry of this DTLB way
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

	//  Restore all entries of this ITLB way:
	l32i	a9, a11, 4		// number of entries for this way
	sub	a5, a11, a10		// way number * 8
	srli	a5, a5, 3		// way number
	bbsi.l	a9, 15, 6f		// skip ITLB if is a DTLB-only way
	extui	a9, a9, 0, 8
1:
	l32i	a6, a4, 0		// read entry VPN + ASID
	extui	a7, a6, 0, 8		// get ASID
	bnez	a7, 2f			// if non-zero, need WITLB
	add	a6, a6, a5		// zero, so need IITLB - add way number
	iitlb	a6			// invalidate ITLB entry
	j	5f
2:	//  Non-zero ASID.  Put in RASID and adjust PS.RING accordingly.
	bgeui	a7, 5, 3f		// branch if ASID >= 5
	addi	a7, a7, -1
	slli	a7, a7, 6		// PS.RING = ASID - 1
	addi	a7, a7, 0x11		// PS.EXCM=1, PS.INTLEVEL=1
	movi	a6, 0x04030201		// for ASID in {1 .. 4}
	j	4f
3:	// ASID >= 5, place it in RASID
	movi	a6, 0x00030201
	slli	a7, a7, 24
	add	a6, a7, a6		// RASID = 0x <ASID> 03 02 01
	movi	a7, 0xd1		// PS.RING=3, PS.EXCM=1, PS.INTLEVEL=1
4:	wsr	a6, rasid
	wsr	a7, ps
	rsync
	l32i	a6, a4, 0		// read entry VPN + ASID
	l32i	a7, a4, 4		// read entry PPN + CA
	srli	a6, a6, 8		// replace ASID ...
	slli	a6, a6, 8		// ...
	add	a6, a6, a5		// ... with way number
	j	8f
	.align	16			// ensure WITLB and ISYNC in same cache line
8:	witlb	a7, a6			// write ITLB entry ...
5:	isync
	addi	a4, a4, 8
	add	a5, a5, a12		// next entry of this ITLB way
	addi	a9, a9, -1
	bnez	a9, 1b
6:

	bne	a11, a10, .Loop_tlbmap_rest	// loop for next TLB way
	l32i	a5, a3, CS_SA_rasid
	wsr	a5, rasid
	movi	a6, 0x11
	wsr	a6, ps
	rsync
	//  Done saving TLBs.
#endif

#if XCHAL_HAVE_WINDOWED
	// All the stack frames (except for our own) are supposed to be spilled
	// into the stack. So now we restore the saved registers for our caller
	// (and its caller) into the correct locations in the stack. See the
	// comments in core-save.S and also the Xtensa Programmers Guide for
	// more information. Of course we only restore if there is valid saved
	// state.

	l32i	a4, a3, CS_SA_caller_regs_saved		// flag
	beqz	a4, .Lendcr				// skip restore if 0

	// Restore our caller's a0-a3

	l32i	a1, a3, CS_SA_areg + 1*4		// restore a1
	addi	a4, a1, -16
	l32i	a5, a3, CS_SA_caller_regs
	l32i	a6, a3, CS_SA_caller_regs + 4
	s32i	a5, a4, 0				// caller a0
	s32i	a6, a4, 4				// caller a1
	l32i	a5, a3, CS_SA_caller_regs + 8
	l32i	a6, a3, CS_SA_caller_regs + 12
	s32i	a5, a4, 8				// caller a2
	s32i	a6, a4, 12				// caller a3

	// Now restore our callers caller's a0-a3

	l32i	a5, a3, CS_SA_caller_regs + 16
	l32i	a6, a3, CS_SA_caller_regs + 20
	s32i	a5, a1, 0				// caller caller a0
	s32i	a6, a1, 4				// caller caller a1
	l32i	a5, a3, CS_SA_caller_regs + 24
	l32i	a6, a3, CS_SA_caller_regs + 28
	s32i	a5, a1, 8				// caller caller a2
	s32i	a6, a1, 12				// caller caller a3

	// Now restore caller's a4-a11 as required
	// NOTE a0 is pointing to _xtos_core_save() not the actual caller

	l32i	a4, a3, CS_SA_areg			// load actual return address
	extui	a4, a4, 30, 2				// top 2 bits of ret addr
	blti	a4, 2, .Lendcr
	l32i	a5, a1, 4				// a5 <- caller caller a1
	slli	a4, a4, 4
	sub	a4, a5, a4				// a4 <- bottom of extra save area
	addi	a5, a5, -16				// a5 <- top of extra save area
	addi	a6, a3, CS_SA_caller_regs + 32		// location to start restore from
.Lcrloop:
	l32i	a7, a6, 0				// Restore in groups of 4 registers
	l32i	a8, a6, 4
	s32i	a7, a4, 0
	s32i	a8, a4, 4
	l32i	a7, a6, 8
	l32i	a8, a6, 12
	s32i	a7, a4, 8
	s32i	a8, a4, 12
	addi	a4, a4, 16
	addi	a6, a6, 16
	blt	a4, a5, .Lcrloop
.Lendcr:
#endif

	// Restore timers and CCOUNT right before enabling interrupts. We will
	// try to restore any timer interrupts that were pending (as indicated
	// by the INTERRUPT register) at the time of the state save.
#if XCHAL_HAVE_CCOUNT
	.macro	restore_timer	num intr
	l32i	a5, a3, CS_SA_ccompare + 4*\num		// Load CCOMPARE value
	l32i	a6, a3, CS_SA_interrupt			// Load old INTERRUPT value
	wsr	a5, CCOMPARE_0 + \num			// Restore CCOMPARE
	bbci	a6, \intr, .Lrtdone\num			// Intr not set for this timer
	addi	a5, a5, -1				// CCOUNT = CCOMPARE - 1
.Lrttry\num:
	wsr	a5, CCOUNT				// Set CCOUNT and wait
	esync
	nop
	rsr	a6, INTERRUPT
	bbci	a6, \intr, .Lrttry\num			// If intr not set then retry
.Lrtdone\num:
	.endm

#if XCHAL_NUM_TIMERS > 0
	restore_timer	0 XCHAL_TIMER0_INTERRUPT
#endif
#if XCHAL_NUM_TIMERS > 1
	restore_timer	1 XCHAL_TIMER1_INTERRUPT
#endif
#if XCHAL_NUM_TIMERS > 2
        restore_timer	2 XCHAL_TIMER2_INTERRUPT
#endif
#if XCHAL_NUM_TIMERS > 3
        restore_timer	3 XCHAL_TIMER3_INTERRUPT
#endif

	// Attempt to clear any spurious timer interrupts caused by the CCOUNT
	// dance above.
#if XCHAL_NUM_TIMERS > 0
	l32i	a5, a3, CS_SA_ccount			// Restore CCOUNT
	wsr	a5, CCOUNT
	l32i	a5, a3, CS_SA_interrupt			// Load old intr value
	bbsi	a5, XCHAL_TIMER0_INTERRUPT, .Lx1	// Skip if timer0 intr set
	rsr	a6, CCOMPARE_0				// Force timer0 intr clear
	wsr	a6, CCOMPARE_0
.Lx1:
#if XCHAL_NUM_TIMERS > 1
	bbsi	a5, XCHAL_TIMER1_INTERRUPT, .Lx2	// Skip if timer1 intr set
	rsr	a6, CCOMPARE_1				// Force timer1 intr clear
	wsr	a6, CCOMPARE_1
.Lx2:
#endif
#if XCHAL_NUM_TIMERS > 2
	bbsi	a5, XCHAL_TIMER2_INTERRUPT, .Lx3	// Skip if timer2 intr set
	rsr	a6, CCOMPARE_2				// Force timer2 intr clear
	wsr	a6, CCOMPARE_2
.Lx3:
#endif
#if XCHAL_NUM_TIMERS > 3
	bbsi	a5, XCHAL_TIMER3_INTERRUPT, .Lx4	// Skip if timer3 intr set
	rsr	a6, CCOMPARE_3				// Force timer3 intr clear
	wsr	a6, CCOMPARE_3
.Lx4:
#endif
#endif

	l32i	a5, a3, CS_SA_ccount			// Restore CCOUNT again
	wsr	a5, CCOUNT
#endif

#if XCHAL_HAVE_INTERRUPTS
	rsil	a6, 15				// disable interrupts before enabling with INTENABLE
	l32i	a5, a3, CS_SA_intenable
	wsr	a5, INTENABLE
	movi	a4, XCHAL_INTTYPE_MASK_SOFTWARE	// restore any pending software interrupts
	l32i	a5, a3, CS_SA_interrupt
	and	a5, a5, a4
	wsr	a5, INTSET
	rsync
#endif

	//l32i	a0, a3, CS_SA_restore_label	// _xtos_core_save_common's return PC
#if XCHAL_HAVE_INTERRUPTS || XCHAL_HAVE_EXCEPTIONS
	//l32i	a4, a3, CS_SA_ps
	l32i	a5, a3, CS_SA_epc1
	wsr	a5, EPC1
	l32i	a5, a3, CS_SA_excsave1
	wsr	a5, EXCSAVE1
# ifdef XCHAL_DOUBLEEXC_VECTOR_VADDR
	l32i	a5, a3, CS_SA_depc
	wsr	a5, DEPC
# endif
	//wsr	a4, ps				// PS restored by caller
	//rsync
#endif

#if XCHAL_HAVE_PSO_CDM
	//  As late as possible, wait for debug to wakeup, and clear PWRCTL.ShutProcOffOnPWait.
	movi	a4, XDM_MISC_PWRCTL
	rer	a5, a4				// read pwrctl

	//  Wait for debug powerup to complete (if started):
	bbci.l	a5, PWRCTL_DEBUG_WAKEUP_SHIFT, 1f
	movi	a7, XDM_MISC_PWRSTAT
2:	rer	a6, a7				// read PWRSTAT
	bbci.l	a6, PWRSTAT_DEBUG_DOMAIN_ON_SHIFT, 2b	// loop until debug is powered up
1:

	movi	a7, ~PWRCTL_CORE_SHUTOFF	// aka ShutProcOffOnPWait
	and	a5, a5, a7			// clear ShutProcOffOnPWait bit
	wer	a5, a4				// update pwrctl
#endif

	movi	a4, 0
	s32i	a4, a3, CS_SA_signature		// make sure save area is marked as no longer valid
#if XCHAL_DCACHE_IS_WRITEBACK
	dhwb	a3, CS_SA_signature
#endif
	ret					// return from _xtos_core_save_common
						// NOTE: a2 holds return value as specified to
						// _xtos_core_restore()

	.size	_xtos_core_restore_nw, . - _xtos_core_restore_nw

