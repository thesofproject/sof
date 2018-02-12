/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Zhigang Wu <zhigang.wu@intel.com>
 */

#include <errno.h>
#include <reef/io.h>
#include <reef/dw-uart.h>

/* uart register list */
#define SUE_UART_REG_THR	0
#define SUE_UART_REG_RBR	0
#define SUE_UART_REG_BRDL	0
#define SUE_UART_REG_BRDH	4
#define SUE_UART_REG_FCR	8
#define SUE_UART_REG_LCR	12
#define SUE_UART_REG_LSR	20


#define uart_read(dev, reg)\
	io_reg_read(dev->port + reg)
#define uart_write(dev, reg, value)\
	io_reg_write(dev->port + reg, value)



/* lcr register */
/*
 *	0x0 -- 5bits
 *	0x1 -- 6bits
 *	0x2 -- 7bits
 *	0x3 -- 8bits
 */
#define LCR_DLS(x)	(x << 0)

/* 0-1stop, 1-1.5stop */
#define LCR_STOP(x)	(x << 2)

/* 0-parity disabled, 1-parity enabled */
#define LCR_PEN(x)	(x << 3)

#define LCR_DLAB_BIT	0x80


/* fcr register */
/* 0-fifo disabled; 1-enabled */
#define FCR_FIFOE(x)	(x << 0)
/* 0-mode0, 1-mode1 */
#define FCR_MODE(x)		(x << 3)
#define FCR_RCVR_RST	0x2
#define FCR_XMIT_RST	0x4

/* lsr register */
#define LSR_TEMT	0x40	/* transmitter empty */


struct dw_uart_device {
	uint32_t port;	/* register base address */
	uint32_t baud;	/* Baud rate */
	uint32_t timeout;
};

static struct dw_uart_device uart_dev = {
	.port = SUE_UART_REG_BASEADDR,
	.baud = SUE_UART_BAUDRATE,
	.timeout = SUE_UART_TIMEOUT,
};


void dw_uart_init(uint32_t baud, uint32_t format)
{
	struct dw_uart_device *dev;
	uint32_t divisor, tmp;

	dev = (struct dw_uart_device *)&uart_dev;

	if (baud != 0) {
		divisor = (SUE_SYS_CLK_FREQ / baud) >> 4;

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

	/* 8Bit-data, 1-stop bit, no parity, clear DLAB */
	uart_write(dev, SUE_UART_REG_LCR,
		LCR_DLS(3) | LCR_STOP(0) | LCR_PEN(0));

	/* FIFO enalbe, mode0, Tx/Rx FIFO reset */
	uart_write(dev, SUE_UART_REG_FCR,
		FCR_FIFOE(1) | FCR_MODE(0) | FCR_RCVR_RST | FCR_XMIT_RST);

	/* reset port */
	uart_write(dev, SUE_UART_REG_RBR, 0);
}


void dw_uart_write_word(uint32_t word)
{
	struct dw_uart_device *dev;
	uint8_t bytes[9];
	uint32_t outchar;
	int i, j;
	uint32_t timeout;

	dev = (struct dw_uart_device *)&uart_dev;

	for (i = 28, j = 0; i >= 0 && j < 8 ; i -= 4, j++)
		bytes[j] = (word >> i) & 0xF;

	/* add '\n' to jump to the new line after each output */
	bytes[8] = 0x0A;

	for (i = 0; i < 9; i++) {
		if (i < 8)
			outchar = (bytes[i] > 9) ?
				(bytes[i] + 0x37) : (bytes[i] + 0x30);
		else
			outchar = bytes[i];

		/* wait for transmitter to ready to accept a character */
		timeout = dev->timeout;
		while ((uart_read(dev, SUE_UART_REG_LSR) & LSR_TEMT) == 0) {
			if (timeout-- == 0)	/* donot wait too long time */
				break;
		}

		/* write to output reg */
		uart_write(dev, SUE_UART_REG_THR, outchar);
	}
}
