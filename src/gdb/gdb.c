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
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 * GDB Stub - parse incoming GDB packets, control FW accordingly and provide
 * a reply to the GDB server.
 *
 */

#include <sof/gdb/gdb.h>
#include <sof/gdb/ringbuffer.h>
#include <arch/gdb/xtensa-defs.h>
#include <arch/gdb/utilities.h>
#include <sof/string.h>
#include <signal.h>
#include <stdint.h>
#include <xtensa/xtruntime.h>
#include <sof/interrupt.h>

/* local functions */
static int get_hex(unsigned char ch);
static int hex_to_int(unsigned char **ptr, int *int_value);
static void put_packet(unsigned char *buffer);
static void parse_request(void);
static unsigned char *get_packet(void);
static void gdb_log_exception(char *message);
static unsigned char *mem_to_hex(void *mem_,
				 unsigned char *buf, int count);
static unsigned char *hex_to_mem(const unsigned char *buf, void *mem_,
							int count);

/* main buffers */
static unsigned char remcom_in_buffer[GDB_BUFMAX];
static unsigned char remcom_out_buffer[GDB_BUFMAX];

static const char hex_chars[] = "0123456789abcdef";

/* registers backup conatiners */
int sregs[256];
int aregs[64];

void gdb_init(void)
{
	init_buffers();
}

/* scan for the GDB packet sequence $<data>#<check_sum> */
unsigned char *get_packet(void)
{
	unsigned char *buffer = &remcom_in_buffer[0];
	unsigned char check_sum;
	unsigned char xmitcsum;
	int count;
	unsigned char ch;

	/* wait around for the start character, ignore all other characters */
	while ((ch = get_debug_char()) != '$')
		;
retry:
	check_sum = 0;
	xmitcsum = -1;
	count = 0;

	/* now, read until a # or end of buffer is found */
	while (count < GDB_BUFMAX - 1) {
		ch = get_debug_char();

		if (ch == '$')
			goto retry;
		if (ch == '#')
			break;
		check_sum = check_sum + ch;
		buffer[count] = ch;
		count++;
	}
	/* mark end of the sequence */
	buffer[count] = 0x00;
	if (ch == '#') {
		/* We have request already, now fetch its check_sum */
		ch = get_debug_char();
		xmitcsum = get_hex(ch) << 4;
		ch = get_debug_char();
		xmitcsum += get_hex(ch);

		if (check_sum != xmitcsum) {
			/* TODO: handle wrong check_sums */
			put_debug_char('+');
		} else {
			/* successful transfer */
			put_debug_char('+');
	/*
	 * if a sequence char is present
	 * reply the sequence ID
	 */
			if (buffer[2] == ':') {
				put_debug_char(buffer[0]);
				put_debug_char(buffer[1]);

				return &buffer[3];
			}
		}
	}
	return buffer;
}

/* Convert ch from a request to an int */
static int get_hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

void gdb_handle_exception(void)
{
	gdb_log_exception("Hello from GDB!");
	parse_request();
}

void parse_request(void)
{
	unsigned char *request;
	unsigned int i;
	int addr;
	int length;
	unsigned int windowbase = (4 * sregs[WINDOWBASE]);


while (1) {
	request = get_packet();
	/* Log any exception caused by debug exception */
	gdb_debug_info(request);
	/* Pick incoming request handler */
	unsigned char command = *request++;

	switch (command) {
	/* Continue normal program execution and leave debug handler */
	case 'c':
		if (hex_to_int(&request, &addr))
			sregs[DEBUG_PC] = addr;

		/* return from exception */
		return;
	/* insert breakpoint */
	case 'Z':
		switch (*request++) {
		/* HW breakpoint */
		case '1':
			if (*request++ == ',' && hex_to_int(&request, &addr) &&
			    *request++ == ',' && hex_to_int(&request, &length)
			    && *request == 0) {
				for (i = 0; i < XCHAL_NUM_IBREAK; ++i) {
					if (!(sregs[IBREAKENABLE] & (1 << i)) ||
					sregs[IBREAKA + i] == addr) {
						sregs[IBREAKA + i] = addr;
						sregs[IBREAKENABLE] |= (1 << i);
						arch_gdb_write_sr((IBREAKA+i),
								  sregs);
						arch_gdb_write_sr(IBREAKENABLE,
								  sregs);
						break;
					}
				}

				if (i == XCHAL_NUM_IBREAK) {
					strcpy((char *) remcom_out_buffer,
					 "E02");
				} else {
					strcpy((char *)remcom_out_buffer, "OK");
					sregs[INTENABLE]  &=
					DISABLE_LOWER_INTERRUPTS_MASK;
					arch_gdb_write_sr(INTENABLE, sregs);
				}
			} else {
				strcpy((char *)remcom_out_buffer, "E01");
			}
			break;
		/* SW breakpoints */
		default:
			/* send empty response to indicate thet SW breakpoints
			 *  are not supported
			 */
			strcpy((char *)remcom_out_buffer, "");
			break;
		}
		break;
	/* remove HW breakpoint */
	case 'z':
		switch (*request++) {
		/* remove HW breakpoint */
		case '1':
			if (*request++ == ',' && hex_to_int(&request, &addr) &&
			*request++ == ',' && hex_to_int(&request, &length)) {
				for (i = 0; i < XCHAL_NUM_IBREAK; ++i) {
					if (sregs[IBREAKENABLE] & (1 << i) &&
					    sregs[IBREAKA + i] == addr) {
						sregs[IBREAKENABLE]
						&= ~(1 << i);
						arch_gdb_write_sr(IBREAKENABLE,
								  sregs);
						break;
					}
				}
				if (i == XCHAL_NUM_IBREAK)
					strcpy((char *)remcom_out_buffer,
					"E02");
				else
					strcpy((char *)remcom_out_buffer, "OK");
			} else {
				strcpy((char *)remcom_out_buffer, "E01");
			}
			break;
		/* SW breakpoints */
		default:
			/* send empty response to indicate thet SW breakpoints
			 *  are not supported
			 */
			strcpy((char *)remcom_out_buffer, "");
			break;
		}
		break;
	/* single step in the code */
	case 's':
		if (hex_to_int(&request, &addr))
			sregs[DEBUG_PC] = addr;
		arch_gdb_single_step(sregs);
		return;
	/* read register */
	case 'p':
		if (hex_to_int(&request, &addr)) {
			/* read address register in the current window */
			if (addr < 0x10) {
				mem_to_hex(aregs + addr, remcom_out_buffer, 4);
			} else if (addr == 0x20) { /* read PC */
				mem_to_hex(sregs + DEBUG_PC,
					remcom_out_buffer, 4);
			} else if (addr >= 0x100 &&
					addr < (0x100 + XCHAL_NUM_AREGS)) {
				mem_to_hex(aregs + ((addr - windowbase) &
					REGISTER_MASK), remcom_out_buffer, 4);
			} else if (addr >= 0x200 && addr < 0x300) {
				addr &= REGISTER_MASK;
				arch_gdb_read_sr(addr);
				mem_to_hex(sregs + addr, remcom_out_buffer, 4);
			} else if (addr >= 0x300 && addr < 0x400) {
				strcpy((char *)remcom_out_buffer,
						"deadbabe");
			} else { /* unexpected register number */
				strcpy((char *)remcom_out_buffer, "E00");
			}
		}
		break;
	/* write register */
	case 'P':
		if (hex_to_int(&request, &addr) && *(request++) == '=') {
			int ok = 1;

			if (addr < 0x10) {
				hex_to_mem(request, aregs + addr, 4);
			} else if (addr == 0x20) {
				hex_to_mem(request, sregs + DEBUG_PC, 4);
			} else if (addr >= 0x100 && addr <
				0x100 + XCHAL_NUM_AREGS) {
				hex_to_mem(request, aregs +
				((addr - windowbase) &	REGISTER_MASK), 4);
			} else if (addr >= 0x200 && addr < 0x300) {
				addr &= REGISTER_MASK;
				hex_to_mem(request, sregs + addr, 4);
			} else {
				ok = 0;
				strcpy((char *)remcom_out_buffer, "E00");
			}
			if (ok)
				strcpy((char *)remcom_out_buffer, "OK");
		}
		break;
	/* read memory */
	case 'm':
		i = hex_to_int(&request, &addr);
		if (i == VALID_MEM_ADDRESS_LEN &&
		((addr & FIRST_BYTE_MASK) >> 28) == VALID_MEM_START_BYTE &&
		*request++ == ',' && hex_to_int(&request, &length)) {
			if (mem_to_hex((void *)addr, remcom_out_buffer, length))
				break;
			strcpy((char *)remcom_out_buffer, "E03");
		} else {
			strcpy((char *)remcom_out_buffer, "E01");
		}
		break;
	/* write memory */
	case 'X': /* binary mode */
	case 'M':
		if (hex_to_int(&request, &addr) && *request++ == ',' &&
		    hex_to_int(&request, &length) && *request++ == ':') {
			if (hex_to_mem(request, (void *)addr, length))
				strcpy((char *)remcom_out_buffer, "OK");
			else
				strcpy((char *)remcom_out_buffer, "E03");
		} else {
			strcpy((char *)remcom_out_buffer, "E02");
		}
		break;
	default:
		gdb_log_exception("Unknown GDB command.");
		break;

	}
	/* reply to the request */
	put_packet(remcom_out_buffer);
}
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int hex_to_int(unsigned char **ptr, int *int_value)
{
	int num_chars = 0;
	int hex_value;

	*int_value = 0;
	if (NULL == ptr)
		return 0;

	while (**ptr) {
		hex_value = get_hex(**ptr);
		if (hex_value < 0)
			break;

		*int_value = (*int_value << 4) | hex_value;
		num_chars++;
		(*ptr)++;
	}
	return num_chars;
}


/* Send the packet to the buffer */
static void put_packet(unsigned char *buffer)
{
	unsigned char check_sum;
	int count;
	unsigned char ch;

	/* $<packet_info>#<check_sum> */
	do {
		put_debug_char('$');
		check_sum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			put_debug_char(ch);
			buffer[count] = 0;
			check_sum += ch;
			count += 1;
		}

		put_debug_char('#');
		put_debug_char(hex_chars[check_sum >> 4]);
		put_debug_char(hex_chars[check_sum & 0xf]);

	} while (get_debug_char() != '+');
}


static void gdb_log_exception(char *message)
{
	while (*message)
		put_exception_char(*message++);

}

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null), in case of mem fault,
 * return 0.
 */
static unsigned char *mem_to_hex(void *mem_, unsigned char *buf,
				 int count)
{
	unsigned char *mem = mem_;
	unsigned char ch;

	if ((mem == NULL) || (buf == NULL))
		return NULL;
	while (count-- > 0) {
		ch = arch_gdb_load_from_memory(mem);
		mem++;
		*buf++ = hex_chars[ch >> 4];
		*buf++ = hex_chars[ch & 0xf];
	}

	*buf = 0;
	return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character after the last byte written
 */
static unsigned char *hex_to_mem(const unsigned char *buf, void *mem_,
							int count)
{
	unsigned char *mem = mem_;
	int i;
	unsigned char ch;

	if ((mem == NULL) || (buf == NULL))
		return NULL;
	for (i = 0; i < count; i++) {
		ch = get_hex(*buf++) << 4;
		ch |= get_hex(*buf++);
		arch_gdb_memory_load_and_store(mem, ch);
		mem++;
	}

	dcache_writeback_region((void *)mem, count);
	return mem;
}
