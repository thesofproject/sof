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
 *
 *****************************************************************/

#include <sof/drivers/peripheral.h>
#include <sof/drivers/printf.h>
#include <stdarg.h>

static void lpuart_putc(struct nxp_lpuart *base, const char c)
{
	if (c == '\n')
		lpuart_putc(base, '\r');

	while (!(base->stat & LPUART_STAT_TDRE))
		;

	base->data = c;
}

static int lpuart_init(struct nxp_lpuart *base, int *x)
{
	uint32_t tmp;
	uint32_t sbr, osr, baud_diff, tmp_osr, tmp_sbr, tmp_diff;
	uint32_t clk = UART_CLK_ROOT;
	uint32_t baudrate = BAUDRATE;
	*x = BAUDRATE;
	*x = base->ctrl;
	*x = 0x42;
	tmp = base->ctrl;
	tmp &= ~(LPUART_CTRL_TE | LPUART_CTRL_RE);
	base->ctrl = tmp;
	base->modir = 0;
	base->fifo = ~(LPUART_FIFO_TXFE | LPUART_FIFO_RXFE);
	base->match = 0;

	baud_diff = BAUDRATE;
	osr = 0;
	sbr = 0;
	for (tmp_osr = 4; tmp_osr <= 32; tmp_osr++) {
		tmp_sbr = (clk / (baudrate * tmp_osr));
		if (tmp_sbr == 0)
			tmp_sbr = 1;

		tmp_diff = (clk / (tmp_osr * tmp_sbr));
		tmp_diff = tmp_diff - baudrate;

		if (tmp_diff > (baudrate - (clk / (tmp_osr * (tmp_sbr + 1))))) {
			tmp_diff = baudrate - (clk / (tmp_osr * (tmp_sbr + 1)));
			tmp_sbr++;
		}

		if (tmp_diff <= baud_diff) {
			baud_diff = tmp_diff;
			osr = tmp_osr;
			sbr = tmp_sbr;
		}
	}

	tmp = base->baud;

	if ((osr > 3) && (osr < 8))
		tmp |= LPUART_BAUD_BOTHEDGE_MASK;

	tmp &= ~LPUART_BAUD_OSR_MASK;
	tmp |= LPUART_BAUD_OSR(osr - 1);

	tmp &= ~LPUART_BAUD_SBR_MASK;
	tmp |= LPUART_BAUD_SBR(sbr);
	tmp &= ~(LPUART_BAUD_M10_MASK | LPUART_BAUD_SBNS_MASK);

	base->baud = tmp;

	tmp = base->ctrl;
	tmp &= ~(LPUART_CTRL_PE_MASK |
			LPUART_CTRL_PT_MASK  |
			LPUART_CTRL_M_MASK);
	base->ctrl = tmp;

	base->ctrl = LPUART_CTRL_RE | LPUART_CTRL_TE;

	return 0;
}

int enable_log(int *x)
{
	volatile int *lpuart1 = (volatile int *)LPUART_BASE;
	struct nxp_lpuart *base = (struct nxp_lpuart *)lpuart1;

	lpuart_init(base, x);

	return 0;
}

void dsp_putc(const char c)
{
	volatile int *lpuart1 = (volatile int *)LPUART_BASE;
	struct nxp_lpuart *base = (struct nxp_lpuart *)lpuart1;

	lpuart_putc(base, c);
}

void dsp_puts(const char *s)
{
	char ch;

	while (*s) {
		ch = *s++;
		dsp_putc(ch);
	}
}
