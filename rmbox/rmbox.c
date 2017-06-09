/*
 * mbox dump to debug log convertor.
 *
 * Copyright (c) 2015, Intel Corporation.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

// TODO: include all this stuff

#define TRACE_CLASS_IRQ		(1 << 24)
#define TRACE_CLASS_IPC		(2 << 24)
#define TRACE_CLASS_PIPE	(3 << 24)
#define TRACE_CLASS_HOST	(4 << 24)
#define TRACE_CLASS_DAI		(5 << 24)
#define TRACE_CLASS_DMA		(6 << 24)
#define TRACE_CLASS_SSP		(7 << 24)
#define TRACE_CLASS_COMP	(8 << 24)
#define TRACE_CLASS_WAIT	(9 << 24)
#define TRACE_CLASS_LOCK	(10 << 24)
#define TRACE_CLASS_MEM		(11 << 24)
#define TRACE_CLASS_MIXER	(12 << 24)
#define TRACE_CLASS_BUFFER	(13 << 24)
#define TRACE_CLASS_VOLUME	(14 << 24)
#define TRACE_CLASS_SWITCH	(15 << 24)
#define TRACE_CLASS_MUX		(16 << 24)

#define MAILBOX_HOST_OFFSET	0x144000

#define MAILBOX_OUTBOX_OFFSET	0x0
#define MAILBOX_OUTBOX_SIZE	0x400
#define MAILBOX_OUTBOX_BASE \
	(MAILBOX_BASE + MAILBOX_OUTBOX_OFFSET)

#define MAILBOX_INBOX_OFFSET	MAILBOX_OUTBOX_SIZE
#define MAILBOX_INBOX_SIZE	0x400
#define MAILBOX_INBOX_BASE \
	(MAILBOX_BASE + MAILBOX_INBOX_OFFSET)

#define MAILBOX_EXCEPTION_OFFSET \
	(MAILBOX_INBOX_SIZE + MAILBOX_OUTBOX_SIZE)
#define MAILBOX_EXCEPTION_SIZE	0x100
#define MAILBOX_EXCEPTION_BASE \
	(MAILBOX_BASE + MAILBOX_EXCEPTION_OFFSET)

#define MAILBOX_DEBUG_OFFSET \
	(MAILBOX_EXCEPTION_SIZE + MAILBOX_EXCEPTION_OFFSET)
#define MAILBOX_DEBUG_SIZE	0x100
#define MAILBOX_DEBUG_BASE \
	(MAILBOX_BASE + MAILBOX_DEBUG_OFFSET)

#define MAILBOX_STREAM_OFFSET \
	(MAILBOX_DEBUG_SIZE + MAILBOX_DEBUG_OFFSET)
#define MAILBOX_STREAM_SIZE	0x200
#define MAILBOX_STREAM_BASE \
	(MAILBOX_BASE + MAILBOX_STREAM_OFFSET)

#define MAILBOX_TRACE_OFFSET \
	(MAILBOX_STREAM_SIZE + MAILBOX_STREAM_OFFSET)
#define MAILBOX_TRACE_SIZE	0x380
#define MAILBOX_TRACE_BASE \
	(MAILBOX_BASE + MAILBOX_TRACE_OFFSET)

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static inline char get_char(uint32_t val, int idx)
{
	char c = (val >> (idx * 8)) & 0xff;
	if (c < '0' || c > 'z')
		return '.';
	else
		return c;
}

static void usage(char *name)
{
	fprintf(stdout, "%s:\t -i infile -o outfile\n", name);
	exit(0);
}

static inline float clk_to_usecs(uint32_t time)
{
	/* trace timestamp uses SSP system clock at default 19.2MHz ticks */
	// TODO: support variable clock rates
	return (float)time / 19.2;
}

static void show_trace(uint32_t val, uint32_t addr, uint32_t *timestamp)
{
	const char *trace;
	uint32_t class;

	/* timestamp or value ? */
	if ((addr % 8) == 0) {
		printf("trace.io: timestamp 0x%8.8x (%2.2f us) \tdelta 0x%8.8x (%2.2f us)\t",
			(uint32_t)val, clk_to_usecs(val),
			(uint32_t)val - *timestamp, clk_to_usecs(val - *timestamp));
		*timestamp = val;
		return;
	}

	class = val & 0xff000000;
	if (class == TRACE_CLASS_IRQ)
		trace = "irq";
	else if (class == TRACE_CLASS_IPC)
		trace = "ipc";
	else if (class == TRACE_CLASS_PIPE)
		trace = "pipe";
	else if (class == TRACE_CLASS_HOST)
		trace = "host";
	else if (class == TRACE_CLASS_DAI)
		trace = "dai";
	else if (class == TRACE_CLASS_DMA)
		trace = "dma";
	else if (class == TRACE_CLASS_SSP)
		trace = "ssp";
	else if (class == TRACE_CLASS_COMP)
		trace = "comp";
	else if (class == TRACE_CLASS_WAIT)
		trace = "wait";
	else if (class == TRACE_CLASS_LOCK)
		trace = "lock";
	else if (class == TRACE_CLASS_MEM)
		trace = "mem";
	else if (class == TRACE_CLASS_MIXER)
		trace = "mixer";
	else if (class == TRACE_CLASS_BUFFER)
		trace = "buffer";
	else if (class == TRACE_CLASS_VOLUME)
		trace = "volume";
	else if (class == TRACE_CLASS_SWITCH)
		trace = "switch";
	else if (class == TRACE_CLASS_MUX)
		trace = "mux";
	else {
		printf("value 0x%8.8x\n", (uint32_t)val);
		return;
	}

	printf("%s %c%c%c\n", trace,
		(char)(val >> 16), (char)(val >> 8), (char)val);
}

static void show_debug(uint32_t val, uint32_t addr)
{
	printf("debug: 0x%x (%2.2d) = \t0x%8.8x \t(%8.8d) \t|%c%c%c%c|\n", 
					(unsigned int)addr - MAILBOX_DEBUG_OFFSET,
					((unsigned int)addr - MAILBOX_DEBUG_OFFSET) / 4, 
					val, val,
					get_char(val, 3), get_char(val, 2),
					get_char(val, 1), get_char(val, 0));
}

static void show_exception(uint32_t val, uint32_t addr)
{
	printf("exp: 0x%x (%2.2d) = \t0x%8.8x \t(%8.8d) \t|%c%c%c%c|\n", 
					(unsigned int)addr - MAILBOX_EXCEPTION_OFFSET,
					((unsigned int)addr - MAILBOX_EXCEPTION_OFFSET) / 4, 
					val, val,
					get_char(val, 3), get_char(val, 2),
					get_char(val, 1), get_char(val, 0));
}


static const char *debugfs[] = {
	"dmac0","dmac1", "ssp0", "ssp1", "ssp2", "iram", "dram", "shim", "mbox"
};

static int snapshot(const char *name)
{
	const char *path = "/sys/kernel/debug";
	uint32_t val, addr;
	char pinname[64], poutname[64], buffer[128];
	FILE *in_fd, *out_fd;
	int i, count;

	if (name == NULL) {
		fprintf(stderr, "error: need snapshot name\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(debugfs); i++) {

		sprintf(pinname, "%s/%s", path, debugfs[i]);
		sprintf(poutname, "%s.%s.txt", name, debugfs[i]);

		/* open debugfs for reading */
		in_fd = fopen(pinname, "r");
		if (in_fd == NULL) {
			fprintf(stderr, "error: unable to open %s for reading %d\n",
				pinname, errno);
			continue;
		}

		/* open outfile for reading */
		out_fd = fopen(poutname, "w");
		if (out_fd == NULL) {
			fprintf(stderr, "error: unable to open %s for writing %d\n",
				poutname, errno);
			fclose(in_fd);
			continue;
		}

		fprintf(stdout, "processing %s...\n", pinname);
		addr = 0;

		while (1) {
			count = fread(&val, 1, 4, in_fd);
			if (count != 4)
				break;

			sprintf(buffer, "0x%6.6x: 0x%8.8x\n", addr, val);

			count = fwrite(buffer, 1, strlen(buffer), out_fd);

			addr += 4;
		}

	}

	return 0;
}

int main(int argc, char *argv[])
{
	int opt, count;
	const char * out_file = NULL, *in_file = "/sys/kernel/debug/mbox";
	FILE *in_fd = NULL, *out_fd = NULL;
	char c, tmp[4] = {0};
	uint32_t addr = 0, val, timestamp = 0;

	while ((opt = getopt(argc, argv, "ho:i:s:m:")) != -1) {
		switch (opt) {
		case 'o':
			out_file = optarg;
			break;
		case 'i':
			in_file = optarg;
			break;
		case 's':
			return snapshot(optarg);
		case 'h':
		default: /* '?' */
			usage(argv[0]);
		}
	}

	if (argc > 1 && (in_file == NULL || out_file == NULL))
		usage(argv[0]);

	/* open infile for reading */
	in_fd = fopen(in_file, "r");
	if (in_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for reading %d\n",
			in_file, errno);
	}

	/* open outfile for writing */
	if (out_file == NULL)
		goto convert;

	out_fd = fopen(out_file, "w");
	if (out_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			out_file, errno);
	}

	/* start to converting mailbox */
convert:
	while (1) {
		count = fread(&tmp[0], 1, 4, in_fd);
		if (count != 4)
			break;

		val = *((uint32_t*)tmp);

		c = tmp[0];
		tmp[0] = tmp[3];
		tmp[3] = c;
		c = tmp[1];
		tmp[1] = tmp[2];
		tmp[2] = c;

		if (addr >= MAILBOX_TRACE_OFFSET &&
			addr < MAILBOX_TRACE_OFFSET + MAILBOX_TRACE_SIZE)
			show_trace(val, addr, &timestamp);
		else if (addr >= MAILBOX_DEBUG_OFFSET &&
			addr < MAILBOX_DEBUG_OFFSET + MAILBOX_DEBUG_SIZE)
			show_debug(val, addr);
		else if (addr >= MAILBOX_EXCEPTION_OFFSET &&
			addr < MAILBOX_EXCEPTION_OFFSET + MAILBOX_EXCEPTION_SIZE)
			show_exception(val, addr);

		if (out_fd) {
			count = fwrite(&tmp[0], 1, 4, out_fd);
			if (count != 4)
				break;
		}

		addr += 4;
	}

	/* close files */
	fclose(in_fd);
	if (out_fd)
		fclose(out_fd);

	return 0;
}
