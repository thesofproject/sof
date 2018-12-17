/*
 * Copyright (c) 2017-2018, Intel Corporation
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
 */

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <sof/cpu.h>
#include <sof/dw-uart.h>
#include <sof/io.h>
#include <sof/sof.h>
#include <sof/string.h>
#include <sof/wait.h>

#include <platform/platform.h>

#include "dw-uart-priv.h"

static struct dw_uart_device uart_dev = {
	.port = PLATFORM_LL_UART_REG_BASEADDR,
	.retry = PLATFORM_LL_UART_RETRY,
};

void dw_uart_write_word(uint32_t word)
{
	dw_uart_write_word_internal(&uart_dev, word);
}

#define uart_write uart_write_common
#define uart_read uart_read_common

void dw_uart_ll_init(uint32_t baud)
{
	struct dw_uart_device *dev = &uart_dev;
	uint32_t divisor, tmp;

	if (baud != 0) {
		divisor = (PLATFORM_LL_UART_CLK_FREQ / baud) >> 4;

		/* configure the baudrate */
		tmp = uart_read(dev, SUE_UART_REG_LCR);
		uart_write(dev, SUE_UART_REG_LCR, LCR_DLAB_BIT);
		uart_write(dev, SUE_UART_REG_BRDL,
					(uint8_t)(divisor & 0xFF));
		uart_write(dev, SUE_UART_REG_BRDH,
					(uint8_t)((divisor >> 8) & 0xFF));

		/* restore to access baudrate divisor registers */
		uart_write(dev, SUE_UART_REG_LCR, tmp);
	}

	/* 8-bit data, 1 stop-bit, no parity, clear DLAB */
	uart_write(dev, SUE_UART_REG_LCR,
		LCR_DLS(3) | LCR_STOP(0) | LCR_PEN(0));

	/* FIFO enalbe, mode0, Tx/Rx FIFO reset */
	uart_write(dev, SUE_UART_REG_FCR, FCR_FIFO_RX_8 | FCR_FIFO_TX_0 |
		FCR_FIFOE(1) | FCR_MODE(0) | FCR_RCVR_RST | FCR_XMIT_RST);

	/* reset port */
	uart_write(dev, SUE_UART_REG_RBR, 0);

	/* disable all interrupts */
	uart_write(dev, SUE_UART_REG_IER, 0);

	/* clear possibly pending interrupts */
	tmp = uart_read(dev, SUE_UART_REG_LSR);
	tmp = uart_read(dev, SUE_UART_REG_IIR);
}
