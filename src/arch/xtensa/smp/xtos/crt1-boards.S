// crt1-boards.S
//
// For most hardware / boards, this code sets up the C calling context
// (setting up stack, PS, and clearing BSS) and jumps to __clibrary_start
// which sets up the C library, calls constructors and registers destructors,
// and calls main().
//
// Control arrives here at _start from the reset vector or from crt0-app.S.

// Copyright (c) 1998-2017 Cadence Design Systems, Inc.
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

#include <xtensa/cacheasm.h>
#include <xtensa/cacheattrasm.h>
#include <xtensa/coreasm.h>
#include "xtos-internal.h"
#include <config.h>
#include <arch/memory.h>


// Exports
.global _start

// Imports
//   __clibrary_init		from C library (eg. newlib or uclibc)
//   exit			from C library
//   main			from user application
//   board_init			board-specific (uart/mingloss/tinygloss.c)
//   xthal_dcache_all_writeback	from HAL library
//   __stack			from linker script (see LSP Ref Manual)
//   _bss_table_start		from linker script (see LSP Ref Manual)
//   _bss_table_end		from linker script (see LSP Ref Manual)

.type	main, @function

.type	master_core_data, @object
.type	core_data_ptr, @object

// Macros to abstract away ABI differences

#if __XTENSA_CALL0_ABI__
# define CALL	call0
# define CALLX	callx0
#else
# define CALL	call4
# define CALLX	callx4
#endif

/**************************************************************************/

	.text
	.align 4
_start:
	// each core unpacks xtos structures for itself
	// nevertheless core 0 initializes shared xtosstruct
	get_prid 	a5
#if PLATFORM_MASTER_CORE_ID == 0
	bnez		a5, xtos_per_core
#else
	movi		a4, PLATFORM_MASTER_CORE_ID
	bne			a5, a4, xtos_per_core
#endif

	//  _start is typically NOT at the beginning of the text segment --
	//  it is always called from either the reset vector or other code
	//  that does equivalent initialization (such as crt0-app.S).
	//
	//  Assumptions on entry to _start:
	//	- low (level-one) and medium priority interrupts are disabled
	//	  via PS.INTLEVEL and/or INTENABLE (PS.INTLEVEL is expected to
	//	  be zeroed, to potentially enable them, before calling main)
	//	- C calling context not initialized:
	//	  - PS not initialized
	//	  - SP not initialized
	//	- the following are initialized:
	//	  - LITBASE, cache attributes, WindowBase, WindowStart,
	//	    CPENABLE, FP's FCR and FSR, EXCSAVE[n]

	// Keep a0 zero.  It is used to initialize a few things.
	// It is also the return address, where zero indicates
	// that the frame used by _start is the bottommost frame.
	//
#if !XCHAL_HAVE_HALT || !XCHAL_HAVE_BOOTLOADER		// not needed for Xtensa TX
	movi	a0, 0		// keep this register zero.
#endif

#if XTOS_RESET_UNNEEDED && !XCHAL_HAVE_HALT
#include "reset-unneeded.S"
#endif

	// Init xtos struct ptr
	movi	a2, master_core_data
	movi	a3, core_data_ptr
	s32i	a2, a3, 0

	// Initialize the stack pointer.
	// See the "ABI and Software Conventions" chapter in the
	// Xtensa ISA Reference manual for details.

	// NOTE: Because the _start routine does not use any memory in its
	// stack frame, and because all of its CALL instructions use a
	// window size of 4 (or zero), the stack frame for _start can be empty.

// Common xtos structures used by all cores.
xtos_common:
	// Unpack xtos_exc_handler_table from rodata.
	movi	a6, xtos_exc_handler_table
	movi	a9, xtos_exc_handler_table_r
	movi	a13, XCHAL_EXCCAUSE_NUM*4
xtos_exc_handler_table_loop:
	l32i	a12, a9, 0
	s32i	a12, a6, 0
	addi	a13, a13, -4
	addi	a6, a6, 4
	addi	a9, a9, 4
	bnez	a13, xtos_exc_handler_table_loop

// Xtos structures initialized per core from rodata.
xtos_per_core:
#if CONFIG_BOOT_LOADER
	.weak	_Level2FromVector
	.weak	_Level3FromVector
	.weak	_Level4FromVector
	.weak	_Level5FromVector

	movi	a4, _Level2FromVector
	writesr	excsave 2 a4
	movi	a4, _Level3FromVector
	writesr	excsave 3 a4
	movi	a4, _Level4FromVector
	writesr	excsave 4 a4
	movi	a4, _Level5FromVector
	writesr	excsave 5 a4
#endif

	get_prid	a5
#if PLATFORM_MASTER_CORE_ID == 0
	beqz		a5, xtos_per_core_obtain_xtos_structs
#else
	movi		a4, PLATFORM_MASTER_CORE_ID
	beq		a5, a4, xtos_per_core_obtain_xtos_structs
#endif

xtos_per_core_cacheattr:
#if XCHAL_HAVE_CACHEATTR || XCHAL_HAVE_MIMIC_CACHEATTR || \
		XCHAL_HAVE_XLT_CACHEATTR || \
		(XCHAL_HAVE_PTP_MMU && XCHAL_HAVE_SPANNING_WAY)
	movi	a2, _memmap_cacheattr_reset /* absolute symbol, not a ptr */
	cacheattr_set /* set CACHEATTR from a2 (clobbers a3-a8) */
#endif

xtos_per_core_vecbase:
#if XCHAL_HAVE_VECBASE
	/* note: absolute symbol, not a ptr */
	movi	a2, _memmap_vecbase_reset
	wsr.vecbase	a2
#endif

// Obtain core structs from given address.
xtos_per_core_obtain_xtos_structs:
	get_prid	a5
	movi		a2, 4
	mull		a2, a2, a5
	movi		a3, core_data_ptr
	add			a3, a3, a2
	l32i		a3, a3, 0

xtos_per_core_init_intstruct:
	movi	a2, 0
	s32i	a2, a3, 0
	movi	a2, 0xFFFFFFFF
	s32i	a2, a3, 4
	addi	a3, a3, XTOS_INTSTRUCT_SIZE_PER_CORE

xtos_per_core_init_interrupt_table:
	// Setup iterator.
	movi	a6, XCHAL_NUM_INTERRUPTS

	// Setup init constants.
	movi	a2, xtos_unhandled_interrupt

	1:
	addi	a6, a6, -1
	s32i	a2, a3, 0
	s32i	a6, a3, 4
	// Int handler size should be 8.
	addi	a3, a3, 8
	bnez	a6, 1b

xtos_per_core_init_interrupt_mask:
	movi	a6, XCHAL_NUM_INTERRUPTS
	movi	a5, ~XCHAL_LOWPRI_MASK
	movi	a7, -2

	2:
	// i <==> a6
	addi	a6, a6, -1
	// a4 := 1 << i
	movi	a4, 1
	ssl		a6
	sll		a4, a4
	// a2 := (-2 * (1 << i) - 1) | ~XCHAL_LOWPRI_MASK
	mull	a2, a4, a7
	addi	a2, a2, -1
	or		a2, a2, a5

	s32i	a2, a3, 0
	s32i	a4, a3, 4
	// Int handler size should be 8.
	addi	a3, a3, 8
	addi	a4, a4, 1
	bnez	a6, 2b

	// Assign stack ptr before PS is initialized to avoid any debugger
	// side effects and prevent from double exception.
	xtos_stack_addr_percore_basic sp, a3, _stack_sentry, ARCH_STACK_SIZE

	/*
	 *  Now that sp (a1) is set, we can set PS as per the application
	 *  (user vector mode, enable interrupts, enable window exceptions if applicable).
	 */
#if XCHAL_HAVE_EXCEPTIONS
	movi	a3, PS_UM|PS_WOE_ABI	// PS.WOE = 0|1, PS.UM = 1, PS.EXCM = 0, PS.INTLEVEL = 0
	wsr.ps	a3
	rsync
#endif

	/*
	 *  Do any initialization that affects the memory map, such as
	 *  setting up TLB entries, that needs to be done before we can
	 *  successfully clear BSS (e.g. if some BSS segments are in
	 *  remapped areas).
	 *
	 *  NOTE:  This hook works where the reset vector does not unpack
	 *  segments (see "ROM packing" in the LSP manual), or where
	 *  unpacking of segments is not affected by memory remapping.
	 *  If ROM unpacking is affected, TLB setup must be done in
	 *  assembler from the reset vector.
	 *
	 *  The __memmap_init() routine can be a C function, however it
	 *  does not have BSS initialized!  In particular, __memmap_init()
	 *  cannot set BSS variables, i.e. uninitialized global variables
	 *  (they'll be wiped out by the following BSS clear), nor can it
	 *  assume they are yet initialized to zero.
	 *
	 *  The __memmap_init() function is optional.  It is marked as a
	 *  weak symbol, so that it gets valued zero if not defined.
	 */
	.weak	__memmap_init
	movi	a4, __memmap_init
	beqz	a4, 1f
	CALLX	a4
1:

	//  Call:   int main(int argc, char ** argv, char ** environ);
	CALL	main
	// Does not return here.

	.data
	//  Mark argc/argv/envp parameters as weak so that an external
	//  object file can override them.
	.text

	.size   _start, . - _start
