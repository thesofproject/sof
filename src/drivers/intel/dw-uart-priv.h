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

#ifndef __INCLUDE_DW_UART_PRIV_H__
#define __INCLUDE_DW_UART_PRIV_H__

/* uart register list */
#define SUE_UART_REG_THR	0
#define SUE_UART_REG_RBR	0
#define SUE_UART_REG_BRDL	0
#define SUE_UART_REG_BRDH	4
#define SUE_UART_REG_IER	4
#define SUE_UART_REG_FCR	8
#define SUE_UART_REG_IIR	8
#define SUE_UART_REG_LCR	0xC
#define SUE_UART_REG_LSR	0x14
#define SUE_UART_REG_USR	0x7C  /* UART Status reg. */
#define SUE_UART_REG_TFL	0x80  /* Transmit FIFO Level reg. */
#define SUE_UART_REG_CPR	0xF4  /* Component Parameter reg. */

#define FCR_FIFO_RX_1		0x00 /* 1 byte in RCVR FIFO */
#define FCR_FIFO_RX_4		0x40 /* RCVR FIFO 1/4 full  */
#define FCR_FIFO_RX_8		0x80 /* RCVR FIFO 1/2 full */
#define FCR_FIFO_RX_14		0xC0 /* RCVR FIFO 2 bytes below full */

/* TX FIFO interrupt levels: trigger interrupt with this many bytes in FIFO */
#define FCR_FIFO_TX_0		0x00 /* TX FIFO empty */
#define FCR_FIFO_TX_2		0x10 /* 2 bytes in TX FIFO  */
#define FCR_FIFO_TX_4		0x20 /* TX FIFO 1/4 full */
#define FCR_FIFO_TX_8		0x30 /* TX FIFO 1/2 full */

#define uart_read_common(dev, reg)\
	io_reg_read((dev)->port + reg)
#define uart_write_common(dev, reg, value)\
	io_reg_write((dev)->port + reg, value)

/* ier register */
#define IER_PTIME		0x80
#define IER_ETBEI		0x2

/* iir register */
#define IIR_THR_EMPTY		2 /* THR empty or TX FIFO below threshold */
#define IIR_RX_AVAILABLE	4
#define IIR_RX_STATUS		6 /* overrun, parity, framing, break */

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
#define FCR_MODE(x)	(x << 3)
#define FCR_RCVR_RST	0x2
#define FCR_XMIT_RST	0x4

/* lsr register */
#define LSR_TEMT	0x40	/* transmitter empty */

/* usr register */
#define USR_TFNF	0x2	/* transmitter FIFO not full */

struct dw_uart_device {
	uint32_t port;	/* register base address */
	uint32_t baud;	/* Baud rate */
	uint32_t retry;
};

void dw_uart_write_word_internal(struct dw_uart_device *dev, uint32_t word);

#endif
