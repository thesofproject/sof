/*****************************************************************
 * Copyright 2018 NXP
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:

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
 *
 *****************************************************************/

#ifndef PERIPHERAL_HEADER_H
#define PERIPHERAL_HEADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>


#define MU_CR_NMI_MASK                           0x8u
#define MU_CR_GIRn_MASK                          0xF0000u
#define MU_CR_GIRn_NMI_MASK		(MU_CR_GIRn_MASK | MU_CR_NMI_MASK)

#define MU_SR_RF0_MASK				(1U << 27U)
#define MU_SR_TE0_MASK				(1U << 23U)
#define MU_CR_RIE0_MASK				(1U << 27U)
#define MU_CR_GIE0_MASK				(1U << 31U)

#define XSHAL_MU13_SIDEB_BYPASS_PADDR 0x5D310000
#define MU_PADDR  XSHAL_MU13_SIDEB_BYPASS_PADDR

struct mu_regs {
	volatile uint32_t		MU_TR[4];
	volatile const  uint32_t	MU_RR[4];
	volatile uint32_t		MU_SR;
	volatile uint32_t		MU_CR;
};

void mu_enableinterrupt_rx(struct mu_regs *regs, uint32_t idx);
void mu_enableGIE(struct mu_regs *regs, uint32_t idx);
void mu_disableGIE(struct mu_regs *regs, uint32_t idx);
void mu_clearGIP(struct mu_regs *regs, uint32_t idx);
void mu_triggerGIR(struct mu_regs *regs, uint32_t idx);
void mu_enableinterrupt_gir(struct mu_regs *regs, uint32_t idx);


void mu_msg_receive(struct mu_regs *regs, uint32_t regidx, uint32_t *msg);
void mu_msg_send(struct mu_regs *regs, uint32_t regidx, uint32_t msg);

#define LPUART_STAT_TDRE		(1 << 23)
#define LPUART_FIFO_TXFE		0x80
#define LPUART_FIFO_RXFE		0x40

#define LPUART_BAUD_SBR_MASK                     (0x1FFFU)
#define LPUART_BAUD_SBR_SHIFT                    (0U)
#define LPUART_BAUD_SBR(x)                       (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_SBR_SHIFT)) & LPUART_BAUD_SBR_MASK)
#define LPUART_BAUD_SBNS_MASK                    (0x2000U)
#define LPUART_BAUD_SBNS_SHIFT                   (13U)
#define LPUART_BAUD_SBNS(x)                      (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_SBNS_SHIFT)) & LPUART_BAUD_SBNS_MASK)
#define LPUART_BAUD_BOTHEDGE_MASK                (0x20000U)
#define LPUART_BAUD_BOTHEDGE_SHIFT               (17U)
#define LPUART_BAUD_BOTHEDGE(x)                  (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_BOTHEDGE_SHIFT)) & LPUART_BAUD_BOTHEDGE_MASK)
#define LPUART_BAUD_OSR_MASK                     (0x1F000000U)
#define LPUART_BAUD_OSR_SHIFT                    (24U)
#define LPUART_BAUD_OSR(x)                       (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_OSR_SHIFT)) & LPUART_BAUD_OSR_MASK)
#define LPUART_BAUD_M10_MASK                     (0x20000000U)
#define LPUART_BAUD_M10_SHIFT                    (29U)
#define LPUART_BAUD_M10(x)                       (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_M10_SHIFT)) & LPUART_BAUD_M10_MASK)

#define LPUART_CTRL_TE				(1 << 19)
#define LPUART_CTRL_RE				(1 << 18)
#define LPUART_CTRL_PT_MASK			(0x1U)
#define LPUART_CTRL_PE_MASK			(0x2U)
#define LPUART_CTRL_M_MASK			(0x10U)

struct nxp_lpuart {
	volatile uint32_t verid;
	volatile uint32_t param;
	volatile uint32_t global;
	volatile uint32_t pincfg;
	volatile uint32_t baud;
	volatile uint32_t stat;
	volatile uint32_t ctrl;
	volatile uint32_t data;
	volatile uint32_t match;
	volatile uint32_t modir;
	volatile uint32_t fifo;
	volatile uint32_t water;
};

#define UART_CLK_ROOT (80000000)
#define BAUDRATE (115200)
#define LPUART_BASE  (0x5a080000)

void dsp_putc(const char c);
void dsp_puts(const char *s);

int enable_log(int *x);

#endif
