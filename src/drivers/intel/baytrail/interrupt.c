/*
 * Copyright (c) 2018, Intel Corporation
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
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 *
 */

#include <sof/sof.h>
#include <sof/interrupt.h>

void platform_interrupt_init(void) {}

struct irq_desc *platform_irq_get_parent(uint32_t irq)
{
	return NULL;
}

void platform_interrupt_set(int irq)
{
	arch_interrupt_set(irq);
}

/* clear mask in PISR, bits are W1C in docs but some bits need preserved ?? */
void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PISR, mask << 3);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PISR, mask << 4);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PISR, mask << 5);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PISR, mask << 16);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PISR, mask << 24);
		arch_interrupt_clear(irq);
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PISRH, mask << 0);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PISRH, mask << 8);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PISRH, mask << 9);
		arch_interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PISRH, mask << 10);
		arch_interrupt_clear(irq);
		break;
#endif
	default:
		break;
	}
}

/* TODO: expand this to 64 bit - should we just return mask of IRQ numbers */
uint32_t platform_interrupt_get_enabled(void)
{
	return shim_read(SHIM_PIMR);
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PIMR, mask << 3);
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PIMR, mask << 4);
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PIMR, mask << 5);
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PIMR, mask << 16);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PIMR, mask << 24);
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PIMRH, mask << 8);
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PIMRH, mask << 0);
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PIMRH, mask << 1);
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PIMRH, mask << 2);
		break;
#endif
	default:
		break;
	}
}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_SSP0:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 3));
		break;
	case IRQ_NUM_EXT_SSP1:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 4));
		break;
	case IRQ_NUM_EXT_SSP2:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 5));
		break;
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 16));
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) & ~(mask << 24));
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 8));
		break;
	case IRQ_NUM_EXT_SSP3:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 0));
		break;
	case IRQ_NUM_EXT_SSP4:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 1));
		break;
	case IRQ_NUM_EXT_SSP5:
		shim_write(SHIM_PIMRH, shim_read(SHIM_PIMRH) & ~(mask << 2));
		break;
#endif
	default:
		break;
	}
}
