//
// misc.c - miscellaneous constants
//
// $Id: //depot/rel/Eaglenest/Xtensa/OS/hal/misc.c#1 $

// Copyright (c) 2004-2005 Tensilica Inc.
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


// Software release info (not configuration-specific!):
const unsigned int  Xthal_release_major		= XTHAL_RELEASE_MAJOR;
const unsigned int  Xthal_release_minor		= XTHAL_RELEASE_MINOR;
const char * const  Xthal_release_name		= XTHAL_RELEASE_NAME;
#ifdef XTHAL_RELEASE_INTERNAL
const char * const  Xthal_release_internal	= XTHAL_RELEASE_INTERNAL;
#else
const char * const  Xthal_release_internal	= 0;
#endif
/*  Old format, for backward compatibility:  */
const unsigned int Xthal_rev_no = (XTHAL_MAJOR_REV<<16)|XTHAL_MINOR_REV;

// number of registers in register window, or number of registers if not windowed
const unsigned int  Xthal_num_aregs		= XCHAL_NUM_AREGS;
const unsigned char Xthal_num_aregs_log2	= XCHAL_NUM_AREGS_LOG2;

const unsigned char Xthal_memory_order		= XCHAL_MEMORY_ORDER;
const unsigned char Xthal_have_windowed		= XCHAL_HAVE_WINDOWED;
const unsigned char Xthal_have_density		= XCHAL_HAVE_DENSITY;
const unsigned char Xthal_have_booleans		= XCHAL_HAVE_BOOLEANS;
const unsigned char Xthal_have_loops		= XCHAL_HAVE_LOOPS;
const unsigned char Xthal_have_nsa		= XCHAL_HAVE_NSA;
const unsigned char Xthal_have_minmax		= XCHAL_HAVE_MINMAX;
const unsigned char Xthal_have_sext		= XCHAL_HAVE_SEXT;
const unsigned char Xthal_have_clamps		= XCHAL_HAVE_CLAMPS;
const unsigned char Xthal_have_mac16		= XCHAL_HAVE_MAC16;
const unsigned char Xthal_have_mul16		= XCHAL_HAVE_MUL16;
const unsigned char Xthal_have_fp		= XCHAL_HAVE_FP;
const unsigned char Xthal_have_speculation	= XCHAL_HAVE_SPECULATION;
const unsigned char Xthal_have_exceptions	= XCHAL_HAVE_EXCEPTIONS;
const unsigned char Xthal_xea_version		= XCHAL_XEA_VERSION;
const unsigned char Xthal_have_interrupts	= XCHAL_HAVE_INTERRUPTS;
const unsigned char Xthal_have_highlevel_interrupts	= XCHAL_HAVE_HIGHLEVEL_INTERRUPTS;
const unsigned char Xthal_have_nmi		= XCHAL_HAVE_NMI;
const unsigned char Xthal_have_prid		= XCHAL_HAVE_PRID;
const unsigned char Xthal_have_release_sync	= XCHAL_HAVE_RELEASE_SYNC;
const unsigned char Xthal_have_s32c1i		= XCHAL_HAVE_S32C1I;
const unsigned char Xthal_have_threadptr	= XCHAL_HAVE_THREADPTR;

const unsigned char Xthal_have_pif		= XCHAL_HAVE_PIF;
const unsigned short Xthal_num_writebuffer_entries	= XCHAL_NUM_WRITEBUFFER_ENTRIES;

const unsigned int  Xthal_build_unique_id	= XCHAL_BUILD_UNIQUE_ID;
// Release info for hardware targeted by software upgrades:
const unsigned int  Xthal_hw_configid0		= XCHAL_HW_CONFIGID0;
const unsigned int  Xthal_hw_configid1		= XCHAL_HW_CONFIGID1;
const unsigned int  Xthal_hw_release_major	= XCHAL_HW_VERSION_MAJOR;
const unsigned int  Xthal_hw_release_minor	= XCHAL_HW_VERSION_MINOR;
const char * const  Xthal_hw_release_name	= XCHAL_HW_VERSION_NAME;
const unsigned int  Xthal_hw_min_version_major	= XCHAL_HW_MIN_VERSION_MAJOR;
const unsigned int  Xthal_hw_min_version_minor	= XCHAL_HW_MIN_VERSION_MINOR;
const unsigned int  Xthal_hw_max_version_major	= XCHAL_HW_MAX_VERSION_MAJOR;
const unsigned int  Xthal_hw_max_version_minor	= XCHAL_HW_MAX_VERSION_MINOR;
#ifdef XCHAL_HW_RELEASE_INTERNAL
const char * const  Xthal_hw_release_internal	= XCHAL_HW_RELEASE_INTERNAL;
#else
const char * const  Xthal_hw_release_internal	= 0;
#endif

/*  MMU related info...  */

const unsigned char Xthal_have_spanning_way	= XCHAL_HAVE_SPANNING_WAY;
const unsigned char Xthal_have_identity_map	= XCHAL_HAVE_IDENTITY_MAP;
const unsigned char Xthal_have_mimic_cacheattr	= XCHAL_HAVE_MIMIC_CACHEATTR;
const unsigned char Xthal_have_xlt_cacheattr	= XCHAL_HAVE_XLT_CACHEATTR;
const unsigned char Xthal_have_cacheattr	= XCHAL_HAVE_CACHEATTR;
const unsigned char Xthal_have_tlbs		= XCHAL_HAVE_TLBS;

const unsigned char Xthal_mmu_asid_bits		= XCHAL_MMU_ASID_BITS;
const unsigned char Xthal_mmu_asid_kernel	= XCHAL_MMU_ASID_KERNEL;
const unsigned char Xthal_mmu_rings		= XCHAL_MMU_RINGS;
const unsigned char Xthal_mmu_ring_bits		= XCHAL_MMU_RING_BITS;
const unsigned char Xthal_mmu_sr_bits		= XCHAL_MMU_SR_BITS;
const unsigned char Xthal_mmu_ca_bits		= XCHAL_MMU_CA_BITS;
#if XCHAL_HAVE_TLBS
const unsigned int  Xthal_mmu_max_pte_page_size	= XCHAL_MMU_MAX_PTE_PAGE_SIZE;
const unsigned int  Xthal_mmu_min_pte_page_size	= XCHAL_MMU_MIN_PTE_PAGE_SIZE;
const unsigned char Xthal_itlb_way_bits	= XCHAL_ITLB_WAY_BITS;
const unsigned char Xthal_itlb_ways	= XCHAL_ITLB_WAYS;
const unsigned char Xthal_itlb_arf_ways	= XCHAL_ITLB_ARF_WAYS;
const unsigned char Xthal_dtlb_way_bits	= XCHAL_DTLB_WAY_BITS;
const unsigned char Xthal_dtlb_ways	= XCHAL_DTLB_WAYS;
const unsigned char Xthal_dtlb_arf_ways	= XCHAL_DTLB_ARF_WAYS;
#else
const unsigned int  Xthal_mmu_max_pte_page_size	= 0;
const unsigned int  Xthal_mmu_min_pte_page_size	= 0;
const unsigned char Xthal_itlb_way_bits	= 0;
const unsigned char Xthal_itlb_ways	= 0;
const unsigned char Xthal_itlb_arf_ways	= 0;
const unsigned char Xthal_dtlb_way_bits	= 0;
const unsigned char Xthal_dtlb_ways	= 0;
const unsigned char Xthal_dtlb_arf_ways	= 0;
#endif


/*  Internal memories...  */

const unsigned char Xthal_num_instrom	= XCHAL_NUM_INSTROM;
const unsigned char Xthal_num_instram	= XCHAL_NUM_INSTRAM;
const unsigned char Xthal_num_datarom	= XCHAL_NUM_DATAROM;
const unsigned char Xthal_num_dataram	= XCHAL_NUM_DATARAM;
const unsigned char Xthal_num_xlmi	= XCHAL_NUM_XLMI;

/*  Define arrays of internal memories' addresses and sizes:  */
#define MEMTRIPLET(n,mem,memcap)	_MEMTRIPLET(n,mem,memcap)
#define _MEMTRIPLET(n,mem,memcap)	MEMTRIPLET##n(mem,memcap)
#define MEMTRIPLET0(mem,memcap) \
	const unsigned int  Xthal_##mem##_vaddr[1] = { 0 }; \
	const unsigned int  Xthal_##mem##_paddr[1] = { 0 }; \
	const unsigned int  Xthal_##mem##_size [1] = { 0 };
#define MEMTRIPLET1(mem,memcap) \
	const unsigned int  Xthal_##mem##_vaddr[1] = { XCHAL_##memcap##0_VADDR }; \
	const unsigned int  Xthal_##mem##_paddr[1] = { XCHAL_##memcap##0_PADDR }; \
	const unsigned int  Xthal_##mem##_size [1] = { XCHAL_##memcap##0_SIZE };
#define MEMTRIPLET2(mem,memcap) \
	const unsigned int  Xthal_##mem##_vaddr[2] = { XCHAL_##memcap##0_VADDR, XCHAL_##memcap##1_VADDR }; \
	const unsigned int  Xthal_##mem##_paddr[2] = { XCHAL_##memcap##0_PADDR, XCHAL_##memcap##1_PADDR }; \
	const unsigned int  Xthal_##mem##_size [2] = { XCHAL_##memcap##0_SIZE,  XCHAL_##memcap##1_SIZE };
MEMTRIPLET(XCHAL_NUM_INSTROM, instrom, INSTROM)
MEMTRIPLET(XCHAL_NUM_INSTRAM, instram, INSTRAM)
MEMTRIPLET(XCHAL_NUM_DATAROM, datarom, DATAROM)
MEMTRIPLET(XCHAL_NUM_DATARAM, dataram, DATARAM)
MEMTRIPLET(XCHAL_NUM_XLMI,    xlmi,    XLMI)

/*  Timer info...  */

const unsigned char Xthal_have_ccount	= XCHAL_HAVE_CCOUNT;
const unsigned char Xthal_num_ccompare	= XCHAL_NUM_TIMERS;

#ifdef INCLUDE_DEPRECATED_HAL_CODE
const unsigned char Xthal_have_old_exc_arch	= XCHAL_HAVE_XEA1;
const unsigned char Xthal_have_mmu	= XCHAL_HAVE_TLBS;
const unsigned int  Xthal_num_regs	= XCHAL_NUM_AREGS;	/*DEPRECATED*/
const unsigned char Xthal_num_irom	= XCHAL_NUM_INSTROM;	/*DEPRECATED*/
const unsigned char Xthal_num_iram	= XCHAL_NUM_INSTRAM;	/*DEPRECATED*/
const unsigned char Xthal_num_drom	= XCHAL_NUM_DATAROM;	/*DEPRECATED*/
const unsigned char Xthal_num_dram	= XCHAL_NUM_DATARAM;	/*DEPRECATED*/
const unsigned int  Xthal_configid0	= XCHAL_HW_CONFIGID0;
const unsigned int  Xthal_configid1	= XCHAL_HW_CONFIGID1;
#endif

