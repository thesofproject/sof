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

#include <stdint.h>

#include <sof/io.h>
#include <sof/sof.h>

#include "uart-priv.h"

#define uart_write_reg dw_uart_write_common
#define uart_read_reg dw_uart_read_common

void dw_uart_write_word_internal(struct dw_uart_device *dev, uint32_t word)
{
	uint8_t bytes[8];
	uint32_t outchar;
	int i, j;
	uint32_t retry;

	/* store 8 nibbles of a 32-bit word in an array */
	for (i = 28, j = 0; j < ARRAY_SIZE(bytes); i -= 4, j++)
		bytes[j] = (word >> i) & 0xF;

	for (i = 0; i < ARRAY_SIZE(bytes) + 1; i++) {
		if (i < ARRAY_SIZE(bytes))
			outchar = bytes[i] > 9 ?
				bytes[i] - 10 + 'A' : bytes[i] + '0';
		else
			/* add '\n' to jump to the new line after each output */
			outchar = '\n';

		/* wait for transmitter to become ready to accept a character */
		retry = dev->retry;
		while ((uart_read_reg(dev, SUE_UART_REG_LSR) & LSR_TEMT) == 0)
			if (retry-- == 0)	/* don't wait too long time */
				break;

		/* write to output reg */
		uart_write_reg(dev, SUE_UART_REG_THR, outchar);
	}
}
