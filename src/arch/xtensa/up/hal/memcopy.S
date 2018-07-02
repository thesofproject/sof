/*
 *  Core HAL library functions xthal_memcpy and xthal_bcopy
 */

/*
 * Copyright (c) 2003, 2006, 2010 Tensilica Inc.
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

#include <xtensa/coreasm.h>


#ifdef __XTENSA_EB__
# define BL(b,l)	b
#else
# define BL(b,l)	l
#endif

	.macro	srcel	r, early, late	// combine early and late words, shift into \r
	src	\r, BL(\early,\late), BL(\late,\early)
	.endm

	.macro	ssa8f	r	// set shift-amount for shift *from* given 2-bit alignment
	BL(ssa8b,ssa8l)	\r
	.endm

	.macro	ssa8t	r	// set shift-amount for shift *to* given 2-bit alignment
	BL(ssa8l,ssa8b)	\r	// (reverse of ssa8f)
	.endm

	.macro	s2ll	r, s	// shift-to-later logical (away from zero-addressed byte)
	BL(srl,sll)	\r, \s
	.endm

	.macro	s2el	r, s	// shift-to-early logical (towards zero-addressed byte)
	BL(sll,srl)	\r, \s
	.endm

/*
 * void *xthal_memcpy(void *dst, const void *src, size_t len);
 * void *xthal_bcopy(const void *src, void *dst, size_t len);
 *
 * This function is intended to do the same thing as the standard
 * library function memcpy() (or bcopy()) for most cases.
 * However, it uses strictly 32-bit load and store instructions
 * to copy data.  This ensures this function will work
 * where the source and/or destination references an
 * instruction RAM or ROM, which can only be accessed
 * using l32i (IRAM+IROM) and s32i (IRAM).
 *
 * The bcopy version is provided here to avoid the overhead
 * of an extra call, for callers that require this convention.
 *
 * The (general case) algorithm is as follows:
 *   If destination is unaligned, align it by copying 1 to 3 bytes.
 *   If source is aligned,
 *     do 16 bytes with a loop, and then finish up with
 *     8, 4, and 0-3 byte copies conditional on the length;
 *   else (if source is unaligned),
 *     do the same, but use SRC to align the source data.
 *   This code tries to use fall-through branches for the common
 *     case of aligned source and destination and multiple
 *     of 4 length.
 *
 * Register use:
 *	a0/ return address
 *	a1/ stack pointer
 *	a2/ return value
 *	a3/ src
 *	a4/ length
 *	a5/ dst
 *	a6/ tmp
 *	a7/ tmp
 *	a8/ tmp
 *	a9/ tmp
 *	a10/ tmp
 *	a11/ tmp
 *	a12/ tmp
 */

/* xthal_bcopy and xthal_memcpy need to allocate the same stack size
 * on entry since they share the same function-return code.  Also,
 * there is more than one return point. */

#define SAVE_A0  0
#define SAVE_A3  4
#define SAVE_A4  8
#define SAVE_A5  12
#define SAVE_A12 16
#define STKSIZE  32


	.text
	.align	4
	.global	xthal_bcopy
	.type	xthal_bcopy,@function
xthal_bcopy:
#ifdef __XTENSA_CALL0_ABI__
	addi    sp, sp, -STKSIZE
	s32i    a12, a1, SAVE_A12
#else
	entry	sp, 32		// allow for call8 below
#endif
	// a2=src, a3=dst, a4=len
	mov	a5, a3		// copy dst so that a2 is return value
	mov	a3, a2
	mov	a2, a5
	j	.Lcommon	// go to common code for memcpy+bcopy

	.size	xthal_bcopy, . - xthal_bcopy



/*
 * Destination is unaligned
 */

	.align	4
xthal_memcpy.prefixcode:	// purely for purpose of .size
.Ldstunaligned:
	mov	a10, a5
	mov	a11, a3
	movi	a12, 4
	sub	a6, a12, a6	// number of bytes to copy for dst alignment
	mov	a12, a6
#ifdef __XTENSA_CALL0_ABI__
	s32i	a0, a1, SAVE_A0	// preserve live registers
	s32i	a3, a1, SAVE_A3
	s32i	a4, a1, SAVE_A4
	s32i	a5, a1, SAVE_A5
	call0	xthal_copy123
	l32i	a0, a1, SAVE_A0	// restore live registers
	l32i	a3, a1, SAVE_A3
	l32i	a4, a1, SAVE_A4
	l32i	a5, a1, SAVE_A5
	mov	a6, a12		// restore a6 from callee-saved register
#else
	call8	xthal_copy123
#endif
	add	a5, a5, a6
	add	a3, a3, a6
	sub	a4, a4, a6
	j	.Ldstaligned

	//  Not sure how else to count code that precedes a function, in .size:
	.size	xthal_memcpy.prefixcode, . - xthal_memcpy.prefixcode


	.align	4
	.global	xthal_memcpy
	.type	xthal_memcpy,@function
xthal_memcpy:
#ifdef __XTENSA_CALL0_ABI__
	addi    sp, sp, -STKSIZE
	s32i    a12, a1, SAVE_A12
#else
	entry	sp, 32		// allow for call8 below
#endif
	// a2=dst, a3=src, a4=len
	mov	a5, a2			// copy dst so that a2 is return value
.Lcommon:
#ifdef __XTENSA_CALL0_ABI__
	/*
	 * have to restore the stack
	 */
	_bgeui	a4, 4, 1f
	mov	a12, a0		// preserve return address
	call0	xthal_copy123
	mov	a0, a12		// restore return address
	l32i    a12, a1, SAVE_A12
	addi    sp, sp, STKSIZE
	ret
1:
#else
	bltui	a4, 4, xthal_copy123_pastentry	// NOTE: sometimes relaxes
#endif

	extui	a6, a2, 0, 2		// destination unalignment offset
	bnez	a6, .Ldstunaligned	// align the destination
.Ldstaligned:				// return here once dst is aligned
	srli	a7, a4, 4		// number of loop iterations of 16-bytes each
	extui	a11, a3, 0, 2		// source unalignment offset
	_bnez	a11, .Lsrcunaligned	// if source not aligned, use shifting copy
	/*
	 * Destination and source are 32-bit aligned, use 32-bit copy.
	 */
#if XCHAL_HAVE_LOOPS
	loopnez	a7, .Loop1done
#else /* !XCHAL_HAVE_LOOPS */
	beqz	a7, .Loop1done
	slli	a8, a7, 4
	add	a8, a8, a3		// a8 = end of last 16B source chunk
#endif /* !XCHAL_HAVE_LOOPS */
.Loop1:
	l32i	a6, a3,  0
	l32i	a7, a3,  4
	s32i	a6, a5,  0
	l32i	a6, a3,  8
	s32i	a7, a5,  4
	l32i	a7, a3, 12
	s32i	a6, a5,  8
	addi	a3, a3, 16
	s32i	a7, a5, 12
	addi	a5, a5, 16
#if !XCHAL_HAVE_LOOPS
	blt	a3, a8, .Loop1
#endif /* !XCHAL_HAVE_LOOPS */
.Loop1done:
	bbci.l	a4, 3, .L2
	// copy 8 bytes
	l32i	a6, a3,  0
	l32i	a7, a3,  4
	addi	a3, a3,  8
	s32i	a6, a5,  0
	s32i	a7, a5,  4
	addi	a5, a5,  8
.L2:
	bbci.l	a4, 2, .L3
	// copy 4 bytes
	l32i	a6, a3,  0
	addi	a3, a3,  4
	s32i	a6, a5,  0
	addi	a5, a5,  4
.L3:
	//  Copy last 0 to 3 bytes using 32-bit accesses (aligned source and destination):
	extui	a4, a4, 0, 2	// any bytes to copy?
	beqz	a4, 1f		// if not, skip this to avoid extraneous loads/stores
	l32i	a6, a3, 0	// get source word
	l32i	a7, a5, 0	// get destination word
	ssa8f	a4		// shift from length (end of source)
	s2ll	a6, a6		// align source to last byte
	s2el	a7, a7		// align parts of a7 following modified bytes, to early byte
	ssa8t	a4		// shift to end of modified destination (length)
	srcel	a7, a6, a7	// combine source with late-dst to form last word
	s32i	a7, a5, 0	// update last word
1:

#ifdef __XTENSA_CALL0_ABI__
	l32i    a12, a1, SAVE_A12
	addi    sp, sp, STKSIZE
	ret
#else
	retw
#endif	

	.size	xthal_memcpy, . - xthal_memcpy


	//  void xthal_copy123(dst, src, len);
	//
	//  Copy from 0 to 3 bytes, using only 32-bit loads and stores,
	//  with arbitrarily aligned source and destination.
	//
	// arg1 = a2 = dst
	// arg2 = a3 = src
	// arg3 = a4 = len

	.global	xthal_copy123
	.type	xthal_copy123,@function
	.align	4
xthal_copy123:
	abi_entry

xthal_copy123_pastentry:
	_beqz	a4, cdone	// don't load or store if zero bytes
	//  First get the bytes:
	movi	a5, ~3
	and	a5, a3, a5	// align src address
	l32i	a6, a5, 0
	l32i	a7, a5, 4
	ssa8f	a3
	srcel	a3, a6, a7
	// a3 now contains source bytes, aligned to 1st byte (memory order)
	// (source address is no longer needed at this point)

	//  Does destination span two words?:
	extui	a10, a2, 0, 2	// destination alignment
	sub	a5, a2, a10	// align destination address
	l32i	a8, a5, 0	// get first destination word regardless
	add	a6, a10, a4	// dst_align + len
	ssa8f	a2		// shift from dst_align (to 1st or last byte)
	s2ll	a10, a8		// a10 = first part of destination, aligned to last byte
	bltui	a6, 4, oneword	// branch if destination contained in single word

	//  Two-word destination case:
	l32i	a8, a5, 4	// get second word
	ssa8t	a2		// shift to dst_align
	srcel	a10, a10, a3	// with a10 in early bytes, a3 in later bytes
	s32i	a10, a5, 0	// update first word
	addi	a5, a5, 4	// advance to last word for common code below
	//movi	a10, 0		// not needed, gets dropped

oneword:
	//  One-word (and two-word) destination case:
	//	a8 =  contents of last destination word
	//	a10 = early part of a8 preceding modified bytes, shifted towards last byte
	//
	ssa8f	a4		// shift from length (end of source)
	srcel	a3, a10, a3	// combine early-destination with source, aligned to last byte

	ssa8f	a6		// shift from end of modified destination (dst_align+len)
	s2el	a8, a8		// align parts of a8 following modified bytes, to early byte
	ssa8t	a6		// shift to end of modified destination (dst_align+len)
	srcel	a8, a3, a8	// combine early-dst+source with late-dst to form last word
	s32i	a8, a5, 0	// update last word
cdone:	abi_return		// return dst

/*
 * Destination is aligned, Source is unaligned
 */

	.align	4
.Lsrcunaligned:
	// Copy 16 bytes per iteration for word-aligned dst and unaligned src
	ssa8f	a3		// set shift amount from byte offset
#define SIM_CHECKS_ALIGNMENT	1	/* set to 1 when running on ISS (simulator) with the
					   lint or ferret client, or 0 to save a few cycles */
#if XCHAL_UNALIGNED_LOAD_EXCEPTION || SIM_CHECKS_ALIGNMENT
	extui	a11, a3, 0, 2	// save unalignment offset for below
	sub	a3, a3, a11	// align a3
#endif
	l32i	a6, a3, 0	// load first word
#if XCHAL_HAVE_LOOPS
	loopnez	a7, .Loop2done
#else /* !XCHAL_HAVE_LOOPS */
	beqz	a7, .Loop2done
	slli	a10, a7, 4
	add	a10, a10, a3	// a10 = end of last 16B source chunk
#endif /* !XCHAL_HAVE_LOOPS */
.Loop2:
	l32i	a7, a3,  4
	l32i	a8, a3,  8
	srcel	a6, a6, a7
	s32i	a6, a5,  0
	l32i	a9, a3, 12
	srcel	a7, a7, a8
	s32i	a7, a5,  4
	l32i	a6, a3, 16
	srcel	a8, a8, a9
	s32i	a8, a5,  8
	addi	a3, a3, 16
	srcel	a9, a9, a6
	s32i	a9, a5, 12
	addi	a5, a5, 16
#if !XCHAL_HAVE_LOOPS
	blt	a3, a10, .Loop2
#endif /* !XCHAL_HAVE_LOOPS */
.Loop2done:
	bbci.l	a4, 3, .L12
	// copy 8 bytes
	l32i	a7, a3,  4
	l32i	a8, a3,  8
	srcel	a6, a6, a7
	s32i	a6, a5,  0
	addi	a3, a3,  8
	srcel	a7, a7, a8
	s32i	a7, a5,  4
	addi	a5, a5,  8
	mov	a6, a8
.L12:
	bbci.l	a4, 2, .L13
	// copy 4 bytes
	l32i	a7, a3,  4
	addi	a3, a3,  4
	srcel	a6, a6, a7
	s32i	a6, a5,  0
	addi	a5, a5,  4
	mov	a6, a7
.L13:
	//  Copy last 0 to 3 bytes using 32-bit accesses (shifting source, aligned destination):
	//_beqz	a4[1:0], cdone	// don't load or store if zero bytes
	l32i	a7, a3, 4	// get source word
	l32i	a3, a5, 0	// get destination word
	srcel	a6, a6, a7	// source bytes, aligned to early (1st) byte
	ssa8f	a4		// shift from length (end of source)
	s2ll	a6, a6		// combine early-destination with source, aligned to last byte
	s2el	a3, a3		// align parts of a3 following modified bytes, to early byte
	ssa8t	a4		// shift to end of modified destination (length)
	srcel	a3, a6, a3	// combine early-dst+source with late-dst to form last word
	s32i	a3, a5, 0	// update last word
.Ldone:	
#ifdef __XTENSA_CALL0_ABI__
	l32i    a12, a1, SAVE_A12
	addi    sp, sp, STKSIZE
	ret
#else
	retw
#endif	

	.size	xthal_copy123, . - xthal_copy123

