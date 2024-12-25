/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * xtensa/config/system.h -- HAL definitions that are dependent on SYSTEM configuration
 *
 *  NOTE: The location and contents of this file are highly subject to change.
 *
 *  Source for configuration-independent binaries (which link in a
 *  configuration-specific HAL library) must NEVER include this file.
 *  The HAL itself has historically included this file in some instances,
 *  but this is not appropriate either, because the HAL is meant to be
 *  core-specific but system independent.
 */

/* Customer ID = 7578; Build = 0xa69ab; Copyright (c) 2000-2010 Tensilica Inc.
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

#ifndef XTENSA_CONFIG_SYSTEM_H
#define XTENSA_CONFIG_SYSTEM_H

/*----------------------------------------------------------------------
 * CONFIGURED SOFTWARE OPTIONS
 *----------------------------------------------------------------------
 */

/* (sw-only option, whether software uses absolute literals) */
#define XSHAL_USE_ABSOLUTE_LITERALS	0
/* Set if there is some memory that allows both code and literals.  */
#define XSHAL_HAVE_TEXT_SECTION_LITERALS 1

/* (sw-only option, selected ABI) */
#define XSHAL_ABI			XTHAL_ABI_WINDOWED
/*  The above maps to one of the following constants:  */
#define XTHAL_ABI_WINDOWED		0
#define XTHAL_ABI_CALL0			1

/* (sw-only option, selected C library) */
#define XSHAL_CLIB			XTHAL_CLIB_XCLIB
/*  The above maps to one of the following constants:  */
#define XTHAL_CLIB_NEWLIB		0
#define XTHAL_CLIB_UCLIBC		1
#define XTHAL_CLIB_XCLIB		2

#define XSHAL_USE_FLOATING_POINT	1

#define XSHAL_FLOATING_POINT_ABI        1

/*  SW workarounds enabled for HW errata:  */

/*----------------------------------------------------------------------
 * DEVICE ADDRESSES
 *----------------------------------------------------------------------
 */

/*
 *  Strange place to find these, but the configuration GUI
 *  allows moving these around to account for various core
 *  configurations.  Specific boards (and their BSP software)
 *  will have specific meanings for these components.
 */

/*  I/O Block areas:  */
#define XSHAL_IOBLOCK_CACHED_VADDR	0x70000000
#define XSHAL_IOBLOCK_CACHED_PADDR	0x70000000
#define XSHAL_IOBLOCK_CACHED_SIZE	0x0E000000

#define XSHAL_IOBLOCK_BYPASS_VADDR	0x90000000
#define XSHAL_IOBLOCK_BYPASS_PADDR	0x90000000
#define XSHAL_IOBLOCK_BYPASS_SIZE	0x0E000000

/*  System ROM:  */

/*  System RAM:  */
#define XSHAL_RAM_VADDR		0x40000000
#define XSHAL_RAM_PADDR		0x40000000
#define XSHAL_RAM_VSIZE		0x80000000
#define XSHAL_RAM_PSIZE		0x80000000
#define XSHAL_RAM_SIZE		XSHAL_RAM_PSIZE
/*  Largest available area (free of vectors):  */
#define XSHAL_RAM_AVAIL_VADDR	0x50000000
#define XSHAL_RAM_AVAIL_VSIZE	0x80000000

/*
 *  Shadow system RAM (same device as system RAM, at different address).
 *  (Emulation boards need this for the SONIC Ethernet driver
 *   when data caches are configured for writeback mode.)
 *  NOTE: on full MMU configs, this points to the BYPASS virtual address
 *  of system RAM, ie. is the same as XSHAL_RAM_* except that virtual
 *  addresses are viewed through the BYPASS static map rather than
 *  the CACHED static map.
 */
#define XSHAL_RAM_BYPASS_VADDR		0x20000000
#define XSHAL_RAM_BYPASS_PADDR		0x20000000
#define XSHAL_RAM_BYPASS_PSIZE		0x20000000

/*  Alternate system RAM (different device than system RAM):  */

/*  Some available location in which to place devices in a simulation (eg. XTMP):  */
#define XSHAL_SIMIO_CACHED_VADDR	0x20000000
#define XSHAL_SIMIO_BYPASS_VADDR	0x20000000
#define XSHAL_SIMIO_PADDR		0x20000000
#define XSHAL_SIMIO_SIZE		0x20000000

/*----------------------------------------------------------------------
 *  For use by reference testbench exit and diagnostic routines.
 *----------------------------------------------------------------------
 */
#define XSHAL_MAGIC_EXIT		0xdeeeb400
#define XSHAL_STL_INFO_LOCATION		0xfffffffc

/*----------------------------------------------------------------------
 *			DEVICE-ADDRESS DEPENDENT...
 *
 *  Values written to CACHEATTR special register (or its equivalent)
 *  to enable and disable caches in various modes.
 *----------------------------------------------------------------------
 */

/*----------------------------------------------------------------------
 * BACKWARD COMPATIBILITY ...
 *----------------------------------------------------------------------
 */

/*
 *  NOTE:  the following two macros are DEPRECATED.  Use the latter
 *  board-specific macros instead, which are specially tuned for the
 *  particular target environments' memory maps.
 */
/* disable caches in bypass mode */
#define XSHAL_CACHEATTR_BYPASS		XSHAL_XT2000_CACHEATTR_BYPASS
/* default setting to enable caches (no writeback!) */
#define XSHAL_CACHEATTR_DEFAULT		XSHAL_XT2000_CACHEATTR_DEFAULT

/*----------------------------------------------------------------------
 * GENERIC
 *----------------------------------------------------------------------
 */

/*  For the following, a 512MB region is used if it contains a system (PIF) RAM,
 *  system (PIF) ROM, local memory, or XLMI.
 */

/*  These set any unused 512MB region to cache-BYPASS attribute:  */
/* enable caches in write-back mode */
#define XSHAL_ALLVALID_CACHEATTR_WRITEBACK	0x24444422
/* enable caches in write-allocate mode */
#define XSHAL_ALLVALID_CACHEATTR_WRITEALLOC	0x21111122
/* enable caches in write-through mode */
#define XSHAL_ALLVALID_CACHEATTR_WRITETHRU	0x21111122
/* disable caches in bypass mode */
#define XSHAL_ALLVALID_CACHEATTR_BYPASS		0x22222222
/* default setting to enable caches */
#define XSHAL_ALLVALID_CACHEATTR_DEFAULT	XSHAL_ALLVALID_CACHEATTR_WRITEBACK

/*  These set any unused 512MB region to ILLEGAL attribute:  */
/* enable caches in write-back mode */
#define XSHAL_STRICT_CACHEATTR_WRITEBACK	0xF44444FF
/* enable caches in write-allocate mode */
#define XSHAL_STRICT_CACHEATTR_WRITEALLOC	0xF11111FF
/* enable caches in write-through mode */
#define XSHAL_STRICT_CACHEATTR_WRITETHRU	0xF11111FF
/* disable caches in bypass mode */
#define XSHAL_STRICT_CACHEATTR_BYPASS		0xF22222FF
/* default setting to enable caches */
#define XSHAL_STRICT_CACHEATTR_DEFAULT		XSHAL_STRICT_CACHEATTR_WRITEBACK

/*  These set the first 512MB, if unused, to ILLEGAL attribute to help catch
 *  NULL-pointer dereference bugs; all other unused 512MB regions are set
 *  to cache-BYPASS attribute:
 */
/* enable caches in write-back mode */
#define XSHAL_TRAPNULL_CACHEATTR_WRITEBACK	0x2444442F
/* enable caches in write-allocate mode */
#define XSHAL_TRAPNULL_CACHEATTR_WRITEALLOC	0x2111112F
/* enable caches in write-through mode */
#define XSHAL_TRAPNULL_CACHEATTR_WRITETHRU	0x2111112F
/* disable caches in bypass mode */
#define XSHAL_TRAPNULL_CACHEATTR_BYPASS		0x2222222F
/* default setting to enable caches */
#define XSHAL_TRAPNULL_CACHEATTR_DEFAULT	XSHAL_TRAPNULL_CACHEATTR_WRITEBACK

/*----------------------------------------------------------------------
 *ISS (Instruction Set Simulator) SPECIFIC ...
 *----------------------------------------------------------------------
 */

/*  For now, ISS defaults to the TRAPNULL settings:  */
#define XSHAL_ISS_CACHEATTR_WRITEBACK	XSHAL_TRAPNULL_CACHEATTR_WRITEBACK
#define XSHAL_ISS_CACHEATTR_WRITEALLOC	XSHAL_TRAPNULL_CACHEATTR_WRITEALLOC
#define XSHAL_ISS_CACHEATTR_WRITETHRU	XSHAL_TRAPNULL_CACHEATTR_WRITETHRU
#define XSHAL_ISS_CACHEATTR_BYPASS	XSHAL_TRAPNULL_CACHEATTR_BYPASS
#define XSHAL_ISS_CACHEATTR_DEFAULT	XSHAL_TRAPNULL_CACHEATTR_WRITEBACK

#define XSHAL_ISS_PIPE_REGIONS	0
#define XSHAL_ISS_SDRAM_REGIONS	0

/*----------------------------------------------------------------------
 * XT2000 BOARD SPECIFIC ...
 *----------------------------------------------------------------------
 */

/*  For the following, a 512MB region is used if it contains any system RAM,
 *  system ROM, local memory, XLMI, or other XT2000 board device or memory.
 *  Regions containing devices are forced to cache-BYPASS mode regardless
 *  of whether the macro is _WRITEBACK vs. _BYPASS etc.
 */

/*  These set any 512MB region unused on the XT2000 to ILLEGAL attribute:  */
/* enable caches in write-back mode */
#define XSHAL_XT2000_CACHEATTR_WRITEBACK	0xF442442F
/* enable caches in write-allocate mode */
#define XSHAL_XT2000_CACHEATTR_WRITEALLOC	0xF112112F
/* enable caches in write-through mode */
#define XSHAL_XT2000_CACHEATTR_WRITETHRU	0xF112112F
/* disable caches in bypass mode */
#define XSHAL_XT2000_CACHEATTR_BYPASS		0xF222222F
/* default setting to enable caches */
#define XSHAL_XT2000_CACHEATTR_DEFAULT		XSHAL_XT2000_CACHEATTR_WRITEBACK

#define XSHAL_XT2000_PIPE_REGIONS	0x00000000	/* BusInt pipeline regions */
#define XSHAL_XT2000_SDRAM_REGIONS	0x00000024	/* BusInt SDRAM regions */

/*----------------------------------------------------------------------
 * VECTOR INFO AND SIZES
 *----------------------------------------------------------------------
 */

#define XSHAL_VECTORS_PACKED		0	/* UNUSED */
#define XSHAL_STATIC_VECTOR_SELECT	1
#define XSHAL_RESET_VECTOR_VADDR	0x4e100680
#define XSHAL_RESET_VECTOR_PADDR	0x4e100680

/*
 *  Sizes allocated to vectors by the system (memory map) configuration.
 *  These sizes are constrained by core configuration (eg. one vector's
 *  code cannot overflow into another vector) but are dependent on the
 *  system or board (or LSP) memory map configuration.
 *
 *  Whether or not each vector happens to be in a system ROM is also
 *  a system configuration matter, sometimes useful, included here also:
 */
#define XSHAL_RESET_VECTOR_SIZE	0x000002E0
#define XSHAL_RESET_VECTOR_ISROM	0
#define XSHAL_USER_VECTOR_SIZE	0x0000001C
#define XSHAL_USER_VECTOR_ISROM	0
/* for backward compatibility */
#define XSHAL_PROGRAMEXC_VECTOR_SIZE	XSHAL_USER_VECTOR_SIZE
/* for backward compatibility */
#define XSHAL_USEREXC_VECTOR_SIZE	XSHAL_USER_VECTOR_SIZE
#define XSHAL_KERNEL_VECTOR_SIZE	0x0000001C
#define XSHAL_KERNEL_VECTOR_ISROM	0
/* for backward compatibility */
#define XSHAL_STACKEDEXC_VECTOR_SIZE	XSHAL_KERNEL_VECTOR_SIZE
/* for backward compatibility */
#define XSHAL_KERNELEXC_VECTOR_SIZE	XSHAL_KERNEL_VECTOR_SIZE
#define XSHAL_DOUBLEEXC_VECTOR_SIZE	0x0000001C
#define XSHAL_DOUBLEEXC_VECTOR_ISROM	0
#define XSHAL_WINDOW_VECTORS_SIZE	0x00000178
#define XSHAL_WINDOW_VECTORS_ISROM	0
#define XSHAL_INTLEVEL2_VECTOR_SIZE	0x0000001C
#define XSHAL_INTLEVEL2_VECTOR_ISROM	0
#define XSHAL_INTLEVEL3_VECTOR_SIZE	0x0000001C
#define XSHAL_INTLEVEL3_VECTOR_ISROM	0
#define XSHAL_INTLEVEL4_VECTOR_SIZE	0x0000001C
#define XSHAL_INTLEVEL4_VECTOR_ISROM	0
#define XSHAL_INTLEVEL5_VECTOR_SIZE	0x0000001C
#define XSHAL_INTLEVEL5_VECTOR_ISROM	0
#define XSHAL_DEBUG_VECTOR_SIZE		XSHAL_INTLEVEL5_VECTOR_SIZE
#define XSHAL_DEBUG_VECTOR_ISROM	XSHAL_INTLEVEL5_VECTOR_ISROM
#define XSHAL_NMI_VECTOR_SIZE	0x0000001C
#define XSHAL_NMI_VECTOR_ISROM	0
#define XSHAL_INTLEVEL6_VECTOR_SIZE	XSHAL_NMI_VECTOR_SIZE

#endif /*XTENSA_CONFIG_SYSTEM_H*/

