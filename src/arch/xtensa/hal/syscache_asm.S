// 
// syscache_asm.S - system-dependent assembly language cache management routines
//
// These functions are now obsolete.  They cannot be properly implemented
// in the HAL, because the required settings of CACHEATTR are entirely
// system- or board-dependent.  The HAL is not board specific; it is merely
// processor-configuration specific.  These cache enable and disable
// functions do a "best-guess" of what values may be appropriate.
// They should be avoided.  (Instead, use xthal_set_[id]cacheattr()
// and provide specific CACHEATTR values for the board or system.
// See the LSP ref manual for info on how to obtain such a value as
// computed by xt-genldscripts for a specific LSP, e.g. by using the
// address of the _memmap_cacheattr_reset symbol.)
//
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/hal/syscache_asm.S#1 $

// Copyright (c) 2003-2013 Tensilica Inc.
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

#ifdef INCLUDE_DEPRECATED_HAL_CACHE_CODE

#include <xtensa/cacheasm.h>
#include <xtensa/cacheattrasm.h>
#include <xtensa/config/system.h>

/***  Modify this for your particular board or system:  ***/
#define CACHEATTR_DEFAULT	XSHAL_ISS_CACHEATTR_DEFAULT
#define CACHEATTR_BYPASS	XSHAL_ISS_CACHEATTR_BYPASS

//----------------------------------------------------------------------
//  Enable and disable the caches
//----------------------------------------------------------------------

	.text

	.global	xthal_icache_enable
	.global	xthal_dcache_enable
	.global	xthal_icache_enable_nw
	.global	xthal_dcache_enable_nw

	.global	xthal_icache_disable
	.global	xthal_dcache_disable
	.global	xthal_icache_disable_nw
	.global	xthal_dcache_disable_nw

	/*
	 *  Since we can't enable/disable the icache and dcache independently,
	 *  and don't have a nice place to store a state which would enable
	 *  us to only enable them both when both have been requested to be
	 *  enabled, we simply enable both for any request to enable either,
	 *  and disable both for any request to disable either cache.
	 */

	.align	4
xthal_icache_enable:
	abi_entry
	movi	a3, xthal_set_icacheattr
	movi	a6, CACHEATTR_DEFAULT		// get cache-enabled attributes
	callx4	a3				// enable i-cache
	mov	a2, a6				// (in case future version has a return value)
	abi_return
	.size	xthal_icache_enable, . - xthal_icache_enable

	.align	4
xthal_dcache_enable:
	abi_entry
	movi	a3, xthal_set_dcacheattr
	movi	a6, CACHEATTR_DEFAULT		// get cache-enabled attributes
	callx4	a3				// enable d-cache
	mov	a2, a6				// (in case future version has a return value)
	abi_return
	.size	xthal_dcache_enable, . - xthal_dcache_enable

	.align	4
xthal_icache_disable:
	abi_entry
	movi	a3, xthal_set_icacheattr
	movi	a6, CACHEATTR_BYPASS		// get cache-disabled attributes
	callx4	a3				// disable i-cache
	mov	a2, a6				// (in case future version has a return value)
	abi_return
	.size	xthal_icache_disable, . - xthal_icache_disable

	.align	4
xthal_dcache_disable:
	abi_entry
	movi	a3, xthal_set_dcacheattr
	movi	a6, CACHEATTR_BYPASS		// get cache-disabled attributes
	callx4	a3				// disable d-cache
	mov	a2, a6				// (in case future version has a return value)
	abi_return
	.size	xthal_dcache_disable, . - xthal_dcache_disable

	.align	4
xthal_icache_enable_nw:
	movi	a3, xthal_set_icacheattr_nw
	movi	a2, CACHEATTR_DEFAULT		// get cache-enabled attributes
	jx	a3				// enable i-cache
	.size	xthal_icache_enable_nw, . - xthal_icache_enable_nw

	.align	4
xthal_dcache_enable_nw:
	movi	a3, xthal_set_dcacheattr_nw
	movi	a2, CACHEATTR_DEFAULT		// get cache-enabled attributes
	jx	a3				// enable d-cache
	.size	xthal_dcache_enable_nw, . - xthal_dcache_enable_nw

	.align	4
xthal_icache_disable_nw:
	movi	a3, xthal_set_icacheattr_nw
	movi	a2, CACHEATTR_BYPASS		// get cache-disabled attributes
	jx	a3				// disable i-cache
	.size	xthal_icache_disable_nw, . - xthal_icache_disable_nw

	.align	4
xthal_dcache_disable_nw:
	movi	a3, xthal_set_dcacheattr_nw
	movi	a2, CACHEATTR_BYPASS		// get cache-disabled attributes
	jx	a3				// disable d-cache
	.size	xthal_dcache_disable_nw, . - xthal_dcache_disable_nw

#endif /* INCLUDE_DEPRECATED_HAL_CACHE_CODE */

