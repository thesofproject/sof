/*
* debug log converter, using old rmbox format.
*
* Copyright (c) 2018, Intel Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "convert.h"

#define TRACE_BLOCK_SIZE	8

#define CASE(x) case(TRACE_CLASS_##x): trace = #x; break

static void show_trace(FILE *out_fd, uint64_t val, uint64_t addr, 
	uint64_t *timestamp, double clk)
{
	const char *trace;
	uint32_t class;
	uint64_t delta = val - *timestamp;
	double fdelta = to_usecs(delta, clk);
	double us = 0.0f;

	/* timestamp or value ? */
	if ((addr % (TRACE_BLOCK_SIZE * 2)) == 0) {

		delta = val - *timestamp;
		fdelta = to_usecs(delta, clk);

		/* 64-bit timestamp */
		us = to_usecs(val, clk);

		/* empty data ? */
		if (val == 0) {
			*timestamp = 0;
			return;
		}

		/* detect wrap around */
		if (fdelta < 1000.0 * 1000.0 * 1000.0)
			fprintf(out_fd, "0x%lx [%6.6f]\tdelta [%6.6f]\t", addr,
				us / 1000000.0, fdelta / 1000000.0);
		else
			fprintf(out_fd, "0x%lx [%6.6f]\tdelta [********]\t", addr,
				us / 1000000.0);

		*timestamp = val;
		return;
	}
	else if (*timestamp == 0)
		return;

	/* check for printable values - otherwise it's a value */
	if (!isprint((char)(val >> 16)) || !isprint((char)(val >> 8)) || !isprint((char)val)) {
		fprintf(out_fd, "value 0x%16.16lx\n", val);
		return;
	}

	class = val & 0xff000000;
	switch (class)
	{
		CASE(IRQ);
		CASE(IPC);
		CASE(PIPE);
		CASE(HOST);
		CASE(DAI);
		CASE(DMA);
		CASE(SSP);
		CASE(COMP);
		CASE(WAIT);
		CASE(LOCK);
		CASE(MEM);
		CASE(MIXER);
		CASE(BUFFER);
		CASE(VOLUME);
		CASE(SWITCH);
		CASE(MUX);
		CASE(SRC);
		CASE(TONE);
		CASE(EQ_FIR);
		CASE(EQ_IIR);
		CASE(SA);
		CASE(DMIC);
		CASE(POWER);
	default:
		fprintf(out_fd, "value 0x%8.8x\n", (uint32_t)val);
		return;
	}

	switch ((char)(val >> 16)) {
	case 'e':
	case 'E':
	case 'x':
	case 'X':
		fprintf(out_fd, "%s%s %c%c%c%s\n", KRED, trace,
			(char)(val >> 16), (char)(val >> 8), (char)val, KNRM);
		break;
	default:
		fprintf(out_fd, "%s %c%c%c\n", trace,
			(char)(val >> 16), (char)(val >> 8), (char)val);
		break;
	}
}

int convert(struct convert_config *config)
{
	int count, i;
	char c, tmp[TRACE_BLOCK_SIZE] = { 0 };
	uint64_t addr = 0, val, timestamp = 0;

	fprintf(stdout, "using %2.2fMHz timestamp clock\n", config->clock);

	while (1) {
		count = fread(&tmp[0], 1, TRACE_BLOCK_SIZE, config->in_fd);
		if (count != TRACE_BLOCK_SIZE)
			break;

		val = *((uint64_t*)tmp);

		for (i = 0; i < TRACE_BLOCK_SIZE / 2; i++) {
			c = tmp[i];
			tmp[i] = tmp[TRACE_BLOCK_SIZE - i - 1];
			tmp[TRACE_BLOCK_SIZE - i - 1] = c;
		}

		show_trace(config->out_fd, val, addr, &timestamp, config->clock);

		addr += TRACE_BLOCK_SIZE;
	}

	return 0;
}
