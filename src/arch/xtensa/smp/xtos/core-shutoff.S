// reset-pso.S  --  PSO restore routine, invoked from Reset Vector
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/xtos/core-shutoff.S#1 $

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
#include "xtos-internal.h"

#if XCHAL_HAVE_PSO
	.macro	simulate_reset
	//  Single or multiple power domains, no retention.
	//  Just simulate reset.  Set PS:
	movi	a5, 0x1F
	wsr.ps	a5
	rsync
	//  Scramble things:
	rotw	3
	//  Jump to reset vector:
	movi	a5, _ResetVector
	jx	a5
	.endm
#endif


#if XCHAL_HAVE_PSO_CDM && ! XCHAL_HAVE_PSO_FULL_RETENTION
	//  PSO: core state save area.
	//  This could be pretty large (includes TIE state, TLB state, many regs).
	//
	.section .bss, "aw"
	.align XCHAL_TOTAL_SA_ALIGN
	.global _xtos_pso_savearea
	.type _xtos_pso_savearea, @object
	.size _xtos_pso_savearea, XtosCoreStateSize
_xtos_pso_savearea:
	.space XtosCoreStateSize
#endif


	.text


	//  This version of the _xtos_core_shutoff() function can be called from assembly-level,
	//  where the stack might not be defined/usable, so can't do window-spill
	//  etc.  This saves address registers per call0 ABI (all except a0/a2..a11).
	//
	//  On entry:  a0 = return PC, a2 = flags argument, a3..a11 = undefined/available.
	//  All other registers are saved/restored.
	//
	.align	4
	.global	_xtos_core_shutoff_nw
	.type _xtos_core_shutoff_nw,@function
_xtos_core_shutoff_nw:
#if XCHAL_HAVE_PSO_CDM && ! XCHAL_HAVE_PSO_FULL_RETENTION
	movi	a5, _xtos_core_save_nw
#endif
	mov	a11, a0			// ABI-independent return PC
	j	.Lcommon_shutoff
	.size	_xtos_core_shutoff_nw, . - _xtos_core_shutoff_nw



	//  int  _xtos_core_shutoff(unsigned flags)
	//
	//  Save all processor state and shut-off the core.
	//  Returns when the core wakes up, and all state was restored
	//  (except in single power domain case, see below).
	//
	//  For more details see:  System SW Ref Manual, XTOS Chapter.
	//
	//  Possible return values:
	//
	//	0	core did shut-off (return via reset vector,
	//			or after waiti for full-retention case)
	//
	//	1,2	core did not shut-off (other requestors were already
	//		requesting this core to stay on at time of call)
	//		(1 == early,  2 == late)
	//
	//	3	core did not shut-off (multi-power-domains no retention,
	//			and waiti resumed; FIXME: can this happen?)
	//
	//	-1	core does not have PSO feature
	//
	//  NOTE:  in the single power domain case, this function never returns.
	//	The external system must power the core off than back on,
	//	and execution resumes at the reset vector.
	//
	//  The flags parameter indicates whether to request memory and/or debug domains
	//  to stay powered on while the core is shut-off.  (This parameter is ignored
	//  for the single power domain case.)  If 0, they are both allowed to
	//  shut-off (although other external requesters may keep them powered on).
	//  Otherwise, one or both of these bits may be specified (or'ed together):
	//	XTOS_KEEPON_MEM		force memory domain on during core power shut-off
	//	XTOS_KEEPON_DEBUG	force debug domain on during core power shut-off
	//  If XTOS_KEEPON_MEM is specified, dcache writeback is NOT done.
	//
	//  Effectively, the flags parameter sets the value of these two PWRCTL register
	//  bits (over ERI) during core power shut-off.  The value of these two bits
	//  (as they were prior to calling this function) are saved, and restored on wakeup.
	//  Thus, if the core was requesting that the debug domain be powered on, and
	//  _xtos_core_shutoff() lets it power-off, then upon wakeup, the software restore
	//  sequence restores debug domain power, and waits for debug power to be ON.
	//
	//  
	.align	4
	.global	_xtos_core_shutoff
	.type _xtos_core_shutoff,@function
_xtos_core_shutoff:
	abi_entry

#if XCHAL_HAVE_PSO_CDM && ! XCHAL_HAVE_PSO_FULL_RETENTION
	movi	a5, _xtos_core_save_entry
#endif
	movi	a11, 1f		// ABI-independent return PC
	j	.Lcommon_shutoff

1:	abi_return




	.align	4
.Lcommon_shutoff:



#if XCHAL_HAVE_PSO_CDM && XCHAL_HAVE_PSO_FULL_RETENTION
	//  Multiple power domains, full retention in HW.
	//  Do the minimum required (things that need to be changed during shutoff):

	//  Check whether other agents are keeping this core powered on,
	//  and avoid going through save sequence if we're not going to
	//  power down anyway.
	movi	a3, XDM_MISC_PWRSTAT
	rer	a6, a3
	movi	a5, 1		// indicates other agents want this core powered on
	bbsi.l	a6, PWRSTAT_CORE_STILL_NEEDED_SHIFT, 1f

	rsil	a8, 15				// disable interrupts

#  if XCHAL_HAVE_PREFETCH
	//  Save prefetch control and disable prefetch.
	movi	a10, 0
	xsr.prefctl	a10
#  endif

#  if XCHAL_DCACHE_IS_WRITEBACK
	bbsi.l	a2, PWRCTL_MEM_WAKEUP_SHIFT, 7f	// letting caches power off?
	dcache_writeback_all	a4, a7, a9,0	// yes: writeback
	memw					// wait for writeback to complete
7:
#  endif

	//  Save PWRCTL, and set ShutProcOffOnPWait (for WAITI to shut-off the core).
	//  (With dcache coherence, can this be used as signal to system
	//  to turn off snoops to this core?)
	//
	movi	a4, XDM_MISC_PWRCTL
	rer	a9, a4				// get pwrctl
	movi	a6, PWRCTL_CORE_SHUTOFF		// aka ShutProcOffOnPWait
	or	a7, a9, a6			// indicate WAITI will shut-off
	xor	a9, a7, a6			// make sure it's clear in saved pwrctl
	wer	a7, a4				// write new pwrctl

	//  Make sure everything stabilizes:
	isync
	extw

	//  With ShutProcOffOnPWait set, external agents can't change their mind.
	//  So check again whether other agents are keeping this core powered on,
	//  and avoid going through save sequence if we're not going to
	//  power down anyway.
	rer	a6, a3		// read PWRSTAT
	movi	a5, 2		// if abort: external agent wants core powered on
	bbsi.l	a6, PWRSTAT_CORE_STILL_NEEDED_SHIFT, .Lshutoff_late_abort

	//  Set PWRCTL MEM_WAKEUP bit according to flags (whether to let mem power off).
	movi	a6, PWRCTL_MEM_WAKEUP
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
	extw			// let the new pwrctl value settle

	//  Okay, go for it -- power down (shutoff).

#  if !XTOS_PSO_TEST
	waiti	0		// now shut-off! (interrupts enabled for power-on)
#  endif
	//  Resumes here after power comes back on, after some interrupt taken.
	wsr.ps	a8		// restore interrupts
	movi	a5, 0		// successful
	rsync			// ensure wsr.ps completes

	// FIXME:  do we need to initialize caches?

.Lshutoff_late_abort:
	wer	a7, a4		// restore pwrctl (except ShutProcOffOnPWait still set)

	//  Wait for debug powerup to complete (if started):
	bbci.l	a7, PWRCTL_DEBUG_WAKEUP_SHIFT, 3f
	movi	a6, XDM_MISC_PWRSTAT
2:	rer	a6, a6				// read PWRSTAT
	bbci.l	a6, PWRSTAT_DEBUG_DOMAIN_ON_SHIFT, 2b	// loop until powered up
3:

#  if XCHAL_HAVE_PREFETCH
	wsr.prefctl	a10	// restore prefetch control
#  endif

	//  If CachesLostPower bit set, is there need to initialize caches?

	wer	a9, a4		// clear ShutProcOffOnPWait

	wsr.ps	a8		// restore interrupts
	rsync			// ensure wsr.ps completes

1:	mov	a2, a5
	jx	a11		// return (to ABI-dependent code if C callable)



#elif XCHAL_HAVE_PSO_CDM /*&& !XCHAL_HAVE_PSO_FULL_RETENTION*/
	//  Multiple power domains, no hardware retention.
	//  Do full core state save/restore in software.

	//  Check whether other agents are keeping this core powered on,
	//  and avoid going through save sequence if we're not going to
	//  power down anyway.
	movi	a3, XDM_MISC_PWRSTAT
	rer	a3, a3
	bbsi.l	a3, PWRSTAT_CORE_STILL_NEEDED_SHIFT, 1f

	movi	a3, XTOS_COREF_PSO
	or	a2, a2, a3			// indicate power shutoff in flags

	movi	a3, _xtos_pso_savearea
	movi	a4, _xtos_core_shutoff_aftersave
	jx	a5		// continue in _xtos_core_save (past prologue)
				// which saves processor state, powers down
				// debug/mem per a2, shuts off prefetch and
				// snooping, and jumps to a4

1:	movi	a2, 1		// other agents want this core powered on
	jx	a11		// return (to ABI-dependent code if C callable)

	.align	4
	//.global	_xtos_core_shutoff_aftersave
_xtos_core_shutoff_aftersave:

	//  Multiple power domains, no retention.

	//  Set ShutProcOffOnPWait, for WAITI to shut-off the core.
	//  (With dcache coherence, can this be used as signal to system
	//  to turn off snoops to this core?)
	//
	movi	a4, XDM_MISC_PWRCTL
	rer	a7, a4				// get pwrctl
	movi	a6, PWRCTL_CORE_SHUTOFF		// aka ShutProcOffOnPWait
	or	a7, a7, a6			// indicate WAITI will shut-off
	wer	a7, a4				// write new pwrctl

	//  Make sure everything stabilizes:
	isync
	extw

	//  Check whether other agents are keeping this core powered on,
	//  and avoid going through save sequence if we're not going to
	//  power down anyway.
	movi	a4, XDM_MISC_PWRSTAT
	movi	a2, 2		// if abort: external agent wants core powered on
	rer	a6, a4
	bbsi.l	a6, PWRSTAT_CORE_STILL_NEEDED_SHIFT, .Lshutoff_late_abort

	//  Call system-specific function to wait for system specific
	//  transactions to quiesce before shutting down the processor.
	//  This function may also abort the shutdown, however whoever
	//  attempts it must do it carefully:  the function must know
	//  that it's possible to abort, it must do whatever's needed
	//  in the system to resume normal execution (e.g. restart
	//  snoops, DMA, etc), and for power reasons the software must
	//  avoid calling this shutdown routine in the first place if
	//  it can know then that it would end up aborting here.
	//
	//  This is always a call0 function.
	//  TBD:  can it be a C function instead?
	//  TBD:  describe exact calling conventions, if asm call0

	.weak	xtos_system_ready_for_core_shutoff
	movi	a2, xtos_system_ready_for_core_shutoff
	//isync
	beqz	a2, 1f
	callx0	a2
	bnez	a2, .Lshutoff_late_abort	// if function returns error, abort shutdown
1:

	//  Okay, go for it -- power down (shutoff).


#  if XTOS_PSO_TEST
	//  Test only -- weakly simulate shutoff in sw, don't actually do it.
	simulate_reset
#  elif XCHAL_HAVE_INTERRUPTS
	waiti	15		// now shut-off!
#  elif XCHAL_HAVE_HALT
	halt
#  else
#   error "PSO assumes interrupts (for WAITI) or HALT architecture (for HALT)"
#  endif

	//  Execution should not proceed here.
	//  If we get here, some error has occurred [FIXME]

	movi	a2, 3				// WAITI resumed

.Lshutoff_late_abort:
	//  We end up here if returning from shutoff request.
	//  Here, a2 == return code.
	//  Restore what's been clobbered (and doesn't get restored by caller):
	//	PWRCTL, MEMCTL, return PC.

	l32i	a0, a3, CS_SA_restore_label	// restore return PC

	//  Restore PWRCTL.
	movi	a4, XDM_MISC_PWRCTL
	l32i	a5, a3, CS_SA_pwrctl		// get saved pwrctl
	wer	a5, a4				// restore pwrctl
	//  Wait for debug powerup to complete (if started):
	bbci.l	a5, PWRCTL_DEBUG_WAKEUP_SHIFT, 1f
	movi	a7, XDM_MISC_PWRSTAT
2:	rer	a6, a7				// read PWRSTAT
	bbci.l	a6, PWRSTAT_DEBUG_DOMAIN_ON_SHIFT, 2b	// loop until powered up
1:

	//  Restore MEMCTL.
#  if XCHAL_USE_MEMCTL
	l32i	a5, a3, CS_SA_memctl
	wsr.memctl	a5
#  endif

	//  Clear the signature, to mark save area as no longer valid.
	s32i	a2, a3, CS_SA_signature
#  if XCHAL_DCACHE_IS_WRITEBACK
	dhwb	a3, CS_SA_signature
#  endif

	ret			// return from _xtos_core_save_common



#elif XCHAL_HAVE_PSO
	//  Single power domain.  (No retention.)

	rsil	a8, 15				// disable interrupts

#  if XCHAL_HAVE_PREFETCH
	//  Disable prefetch.
	movi	a10, 0
	wsr.memctl	a10
#  endif

#  if XCHAL_DCACHE_IS_WRITEBACK
	bbsi.l	a2, PWRCTL_MEM_WAKEUP_SHIFT, 7f	// letting caches power off?
	dcache_writeback_all	a4, a5, a6, 0	// yes: writeback
	memw					// wait for writeback to complete
7:
#  endif

1:	waiti	15		// wait for shut-off
	j	1b		// loop until we get powered off



#else
	//  No PSO.
	movi	a2, -1
	jx	a11		// return (to ABI-dependent code if C callable)

#endif




#if XCHAL_HAVE_PSO_CDM
# if XCHAL_HAVE_PSO_FULL_RETENTION

# else /* not full retention */


# endif /* !retention */
#endif /* multi power domains */


	.size	_xtos_core_shutoff, . - _xtos_core_shutoff

