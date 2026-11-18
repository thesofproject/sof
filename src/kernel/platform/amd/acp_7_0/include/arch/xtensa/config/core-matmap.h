/* 
 * xtensa/config/core-matmap.h -- Memory access and translation mapping
 *	parameters (CHAL) of the Xtensa processor core configuration.
 *
 *  If you are using Xtensa Tools, see <xtensa/config/core.h> (which includes
 *  this file) for more details.
 *
 *  In the Xtensa processor products released to date, all parameters
 *  defined in this file are derivable (at least in theory) from
 *  information contained in the core-isa.h header file.
 *  In particular, the following core configuration parameters are relevant:
 *	XCHAL_HAVE_CACHEATTR
 *	XCHAL_HAVE_MIMIC_CACHEATTR
 *	XCHAL_HAVE_XLT_CACHEATTR
 *	XCHAL_HAVE_PTP_MMU
 *	XCHAL_ITLB_ARF_ENTRIES_LOG2
 *	XCHAL_DTLB_ARF_ENTRIES_LOG2
 *	XCHAL_DCACHE_IS_WRITEBACK
 *	XCHAL_ICACHE_SIZE		(presence of I-cache)
 *	XCHAL_DCACHE_SIZE		(presence of D-cache)
 *	XCHAL_HW_VERSION_MAJOR
 *	XCHAL_HW_VERSION_MINOR
 */

/* Customer ID=18056; Build=0xa6a6b; Copyright (c) 1999-2023 Tensilica Inc.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */


#ifndef XTENSA_CONFIG_CORE_MATMAP_H
#define XTENSA_CONFIG_CORE_MATMAP_H


/*----------------------------------------------------------------------
			CACHE (MEMORY ACCESS) ATTRIBUTES
  ----------------------------------------------------------------------*/
/*----------------------------------------------------------------------
				MPU
  ----------------------------------------------------------------------*/

/* Mappings for legacy constants where appropriate */

#define XCHAL_CA_WRITEBACK (XTHAL_MEM_WRITEBACK | XTHAL_AR_RWXrwx)

#define XCHAL_CA_WRITEBACK_NOALLOC (XTHAL_MEM_WRITEBACK_NOALLOC| XTHAL_AR_RWXrwx )

#define XCHAL_CA_WRITETHRU (XTHAL_MEM_WRITETHRU | XTHAL_AR_RWXrwx)

#define XCHAL_CA_ILLEGAL   (XTHAL_AR_NONE   | XTHAL_MEM_DEVICE)
#define XCHAL_CA_BYPASS    (XTHAL_AR_RWXrwx | XTHAL_MEM_DEVICE)
#define XCHAL_CA_BYPASSBUF    (XTHAL_AR_RWXrwx | XTHAL_MEM_DEVICE |\
        XTHAL_MEM_BUFFERABLE)
#define XCHAL_CA_BYPASS_RX (XTHAL_AR_RX     | XTHAL_MEM_DEVICE)
#define XCHAL_CA_BYPASS_RW (XTHAL_AR_RW     | XTHAL_MEM_DEVICE)
#define XCHAL_CA_BYPASS_R  (XTHAL_AR_R      | XTHAL_MEM_DEVICE)
#define XCHAL_HAVE_CA_WRITEBACK_NOALLOC 1


/* 
 * Contents of MPU background map.
 * NOTE:  caller must define the XCHAL_MPU_BGMAP() macro (not defined here
 * but specified below) before expanding the XCHAL_MPU_BACKGROUND_MAP(s) macro.
 *
 * XCHAL_MPU_BGMAP(s, vaddr_start, vaddr_last, rights, memtype, x...)
 *
 *	s = passed from XCHAL_MPU_BACKGROUND_MAP(s), eg. to select how to expand
 *	vaddr_start = first byte of region (always 0 for first entry)
 *	vaddr_end = last byte of region (always 0xFFFFFFFF for last entry)
 *	rights = access rights
 *	memtype = memory type
 *	x = reserved for future use (0 until then)
 */
/* parasoft-begin-suppress MISRA2012-RULE-20_7 "Macro use model requires s to not be in ()" */
#define XCHAL_MPU_BACKGROUND_MAP(s) \
	XCHAL_MPU_BGMAP(s, 0x00000000, 0x7fffffff,  7,   6, 0) \
	XCHAL_MPU_BGMAP(s, 0x80000000, 0xffffffff,  7,   6, 0) \
/* parasoft-end-suppress MISRA2012-RULE-20_7 "Macro use model requires s to not be in ()" */

	/* end */



#endif /*XTENSA_CONFIG_CORE_MATMAP_H*/

