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
#include <sof/io.h>
#include <sof/sof.h>
#include <sof/string.h>
#include <sof/uart.h>
#include <sof/wait.h>

#include <platform/platform.h>

#include "uart-priv.h"

static void dw_uart_write_word(struct uart *uart, uint32_t word)
{
	struct dw_uart_device *dev = container_of(uart, struct dw_uart_device,
						  common);

	dw_uart_write_word_internal(dev, word);
}

#define uart_write_reg dw_uart_write_common
#define uart_read_reg dw_uart_read_common

static struct uart_ops dw_uart_ll_ops = {
	.write_word = dw_uart_write_word,
};

static struct dw_uart_device trace_uart_dev = {
	.common.ops = &dw_uart_ll_ops,
	.retry = PLATFORM_UART_RETRY,
};

struct uart *dw_uart_init(const struct uart_platform_data *pdata)
{
	struct dw_uart_device *dev = &trace_uart_dev;
	uint32_t divisor, tmp;

	dev->common.pdata = pdata;

	if (pdata->baud != 0) {
		divisor = (PLATFORM_UART_CLK_FREQ / pdata->baud) >> 4;

		/* configure the baudrate */
		tmp = uart_read_reg(dev, SUE_UART_REG_LCR);
		uart_write_reg(dev, SUE_UART_REG_LCR, LCR_DLAB_BIT);
		uart_write_reg(dev, SUE_UART_REG_BRDL,
			       (uint8_t)(divisor & 0xFF));
		uart_write_reg(dev, SUE_UART_REG_BRDH,
			       (uint8_t)((divisor >> 8) & 0xFF));

		/* restore to access baudrate divisor registers */
		uart_write_reg(dev, SUE_UART_REG_LCR, tmp);
	}

	/* 8-bit data, 1 stop-bit, no parity, clear DLAB */
	uart_write_reg(dev, SUE_UART_REG_LCR,
		LCR_DLS(3) | LCR_STOP(0) | LCR_PEN(0));

	/* FIFO enalbe, mode0, Tx/Rx FIFO reset */
	uart_write_reg(dev, SUE_UART_REG_FCR, FCR_FIFO_RX_8 | FCR_FIFO_TX_0 |
		FCR_FIFOE(1) | FCR_MODE(0) | FCR_RCVR_RST | FCR_XMIT_RST);

	/* reset port */
	uart_write_reg(dev, SUE_UART_REG_RBR, 0);

	/* disable all interrupts */
	uart_write_reg(dev, SUE_UART_REG_IER, 0);

	/* clear possibly pending interrupts */
	tmp = uart_read_reg(dev, SUE_UART_REG_LSR);
	tmp = uart_read_reg(dev, SUE_UART_REG_IIR);

	return &dev->common;
}
