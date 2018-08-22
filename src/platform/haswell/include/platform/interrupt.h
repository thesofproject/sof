/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_PLATFORM_INTERRUPT__
#define __INCLUDE_PLATFORM_INTERRUPT__

#include <stdint.h>
#include <stddef.h>
#include <sof/interrupt-map.h>

#define PLATFORM_IRQ_CHILDREN	0

/* IRQ numbers */
#define IRQ_NUM_EXT_SSP0	0	/* Level 1 */
#define IRQ_NUM_EXT_SSP1	1	/* Level 1 */
#define IRQ_NUM_EXT_OBFF	2	/* Level 1 */
#define IRQ_NUM_EXT_IA		4	/* Level 1 */
#define IRQ_NUM_TIMER1		6	/* Level 1 */
#define IRQ_NUM_SOFTWARE1	7	/* Level 1 */
#define IRQ_NUM_EXT_DMAC0	8	/* Level 2 */
#define IRQ_NUM_EXT_DMAC1	9	/* Level 3 */
#define IRQ_NUM_TIMER2		10	/* Level 3 */
#define IRQ_NUM_SOFTWARE2	11	/* Level 3 */
#define IRQ_NUM_EXT_PARITY	12	/* Level 4 */
#define IRQ_NUM_TIMER3		13	/* Level 5 */
#define IRQ_NUM_NMI		14	/* Level 7 */

/* IRQ Masks */
#define IRQ_MASK_EXT_SSP0	(1 << IRQ_NUM_EXT_SSP0)
#define IRQ_MASK_EXT_SSP1	(1 << IRQ_NUM_EXT_SSP1)
#define IRQ_MASK_EXT_OBFF	(1 << IRQ_NUM_EXT_OBFF)
#define IRQ_MASK_EXT_IA		(1 << IRQ_NUM_EXT_IA)
#define IRQ_MASK_TIMER1		(1 << IRQ_NUM_TIMER1)
#define IRQ_MASK_SOFTWARE1	(1 << IRQ_NUM_SOFTWARE1)
#define IRQ_MASK_EXT_DMAC0	(1 << IRQ_NUM_EXT_DMAC0)
#define IRQ_MASK_EXT_DMAC1	(1 << IRQ_NUM_EXT_DMAC1)
#define IRQ_MASK_TIMER2		(1 << IRQ_NUM_TIMER2)
#define IRQ_MASK_SOFTWARE2	(1 << IRQ_NUM_SOFTWARE2)
#define IRQ_MASK_EXT_PARITY	(1 << IRQ_NUM_EXT_PARITY)
#define IRQ_MASK_TIMER3		(1 << IRQ_NUM_TIMER3)

#endif
