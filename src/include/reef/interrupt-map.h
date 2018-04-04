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

#ifndef __INCLUDE_INTERRUPT_MAP__
#define __INCLUDE_INTERRUPT_MAP__

#include <config.h>

#define REEF_IRQ_BIT_SHIFT	24
#define REEF_IRQ_LEVEL_SHIFT	16
#define REEF_IRQ_CPU_SHIFT	8
#define REEF_IRQ_NUM_SHIFT	0
#define REEF_IRQ_NUM_MASK	0xff
#define REEF_IRQ_LEVEL_MASK	0xff
#define REEF_IRQ_BIT_MASK	0x1f
#define REEF_IRQ_CPU_MASK	0xff

#define REEF_IRQ(_bit, _level, _cpu, _number) \
	((_bit << REEF_IRQ_BIT_SHIFT) \
	| (_level << REEF_IRQ_LEVEL_SHIFT)\
	| (_cpu << REEF_IRQ_CPU_SHIFT)\
	| (_number << REEF_IRQ_NUM_SHIFT))

#ifdef CONFIG_IRQ_MAP
/*
 * IRQs are mapped on 4 levels.
 *
 * 1. Peripheral Register bit offset.
 * 2. CPU interrupt level.
 * 3. CPU number.
 * 4. CPU interrupt number.
 */
#define REEF_IRQ_NUMBER(_irq) \
	((_irq >> REEF_IRQ_NUM_SHIFT) & REEF_IRQ_NUM_MASK)
#define REEF_IRQ_LEVEL(_level) \
	((_level >> REEF_IRQ_LEVEL_SHIFT & REEF_IRQ_LEVEL_MASK)
#define REEF_IRQ_BIT(_bit) \
	((_bit >> REEF_IRQ_BIT_SHIFT) & REEF_IRQ_BIT_MASK)
#define REEF_IRQ_CPU(_cpu) \
	((_cpu >> REEF_IRQ_CPU_SHIFT) & REEF_IRQ_CPU_MASK)
#else
/*
 * IRQs are directly mapped onto a single level, bit and level.
 */
#define REEF_IRQ_NUMBER(_irq)	(_irq)
#define REEF_IRQ_LEVEL(_level)	0
#define REEF_IRQ_BIT(_bit)	0
#define REEF_IRQ_CPU(_cpu)	0
#endif

#endif
