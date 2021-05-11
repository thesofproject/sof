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

#include <sof/lib/memory.h>

#include <xtensa/coreasm.h>

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
	.literal_position

// VM ROM jumps to boot loader in IMR/SRAM
#if CONFIG_SUECREEK
_boot_ldr_entry:
	.word BOOT_LDR_TEXT_ENTRY_BASE
#else
_boot_ldr_entry:
	.word IMR_BOOT_LDR_TEXT_ENTRY_BASE
#endif
	.align 4

_start:
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

#if CONFIG_XT_BOOT_LOADER
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

	// Assign stack ptr before PS is initialized to avoid any debugger
	// side effects and prevent from double exception.
	movi	sp, __stack

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
	l32r	a0, _boot_ldr_entry	// load SRAM reset handler address
#if CONFIG_SUECREEK
	callx8	a0	// jump to the handler
#else
	callx12	a0	// jump to the handler
#endif

dead:	nop
	j	dead

	.data
	//  Mark argc/argv/envp parameters as weak so that an external
	//  object file can override them.
	.text

	.size   _start, . - _start
