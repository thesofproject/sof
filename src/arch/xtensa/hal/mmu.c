// 
// mmu.c - MMU related functions
//
// $Id: //depot/rel/Foxhill/dot.8/Xtensa/OS/hal/mmu.c#1 $

// Copyright (c) 2002, 2008 Tensilica Inc.
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

#include <xtensa/config/core.h>

/*
 *  Convert a virtual address to a physical address
 *  (through static maps only).
 *  Returns 0 if successful (*paddrp is set), -1 if not (no mapping).
 */
int  xthal_static_v2p( unsigned vaddr, unsigned *paddrp /*, unsigned len, unsigned rasid*/ )
{
#if XCHAL_HAVE_PTP_MMU && !XCHAL_HAVE_SPANNING_WAY
    if( vaddr >= XCHAL_KSEG_CACHED_VADDR
	&& vaddr <= XCHAL_KSEG_CACHED_VADDR + XCHAL_KSEG_CACHED_SIZE )
	vaddr += XCHAL_KSEG_CACHED_PADDR - XCHAL_KSEG_CACHED_VADDR;
    else if( vaddr >= XCHAL_KSEG_BYPASS_VADDR
	&& vaddr <= XCHAL_KSEG_BYPASS_VADDR + XCHAL_KSEG_BYPASS_SIZE )
	vaddr += XCHAL_KSEG_BYPASS_PADDR - XCHAL_KSEG_BYPASS_VADDR;
    else if( vaddr >= XCHAL_KIO_CACHED_VADDR
	&& vaddr <= XCHAL_KIO_CACHED_VADDR + XCHAL_KIO_CACHED_SIZE )
	vaddr += XCHAL_KIO_CACHED_PADDR - XCHAL_KIO_CACHED_VADDR;
    else if( vaddr >= XCHAL_KIO_BYPASS_VADDR
	&& vaddr <= XCHAL_KIO_BYPASS_VADDR + XCHAL_KIO_BYPASS_SIZE )
	vaddr += XCHAL_KIO_BYPASS_PADDR - XCHAL_KIO_BYPASS_VADDR;
    else
	return( -1 );		/* no known mapping */
#endif /* XCHAL_HAVE_PTP_MMU && !XCHAL_HAVE_SPANNING_WAY */
    *paddrp = vaddr;		/* virtual == physical */
    return( 0 );
}

/*
 *  Convert a physical address to a virtual address
 *  (through static maps only).
 *  Returns 0 if successful (*vaddrp is set), -1 if not (no mapping).
 *
 *  NOTE:  A physical address can be mapped from multiple virtual addresses
 *  (or one or none).
 *  There should be better parameter(s) to help select the mapping returned
 *  (eg. cache mode, address, asid, etc), or somehow return them all.
 *  Mappings returned currently assume the current RASID setting.
 */
int  xthal_static_p2v( unsigned paddr, unsigned *vaddrp, /*unsigned len, unsigned rasid,*/ unsigned cached )
{
#if XCHAL_HAVE_PTP_MMU && !XCHAL_HAVE_SPANNING_WAY
    if( cached ) {
	if( paddr >= XCHAL_KSEG_CACHED_PADDR
	    && paddr <= XCHAL_KSEG_CACHED_PADDR + XCHAL_KSEG_CACHED_SIZE )
	    paddr += XCHAL_KSEG_CACHED_VADDR - XCHAL_KSEG_CACHED_PADDR;
	else if( paddr >= XCHAL_KIO_BYPASS_PADDR
	    && paddr <= XCHAL_KIO_BYPASS_PADDR + XCHAL_KIO_BYPASS_SIZE )
	    paddr += XCHAL_KIO_BYPASS_VADDR - XCHAL_KIO_BYPASS_PADDR;
	else
	    return -1;		/* no known mapping */
    } else {
	if( paddr >= XCHAL_KSEG_BYPASS_PADDR
	    && paddr <= XCHAL_KSEG_BYPASS_PADDR + XCHAL_KSEG_BYPASS_SIZE )
	    paddr += XCHAL_KSEG_BYPASS_VADDR - XCHAL_KSEG_BYPASS_PADDR;
	else if( paddr >= XCHAL_KIO_CACHED_PADDR
	    && paddr <= XCHAL_KIO_CACHED_PADDR + XCHAL_KIO_CACHED_SIZE )
	    paddr += XCHAL_KIO_CACHED_VADDR - XCHAL_KIO_CACHED_PADDR;
	else
	    return -1;		/* no known mapping */
    }
#endif /* XCHAL_HAVE_PTP_MMU && !XCHAL_HAVE_SPANNING_WAY */
    *vaddrp = paddr;		/* virtual == physical */
    return( 0 );
}

