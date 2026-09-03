// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

#include <sof/debug/gdb/ringbuffer.h>
#include <sof/lib/mailbox.h>

#define BUFFER_OFFSET 0x120

volatile struct ring * const rx = (void *)mailbox_get_debug_base();
volatile struct ring * const tx = (void *)(mailbox_get_debug_base() +
					   BUFFER_OFFSET);
volatile struct ring * const debug = (void *)(mailbox_get_debug_base() +
					      (2 * BUFFER_OFFSET));

void init_buffers(void)
{
	rx->head = rx->tail = 0;
	tx->head = tx->tail = 0;
	debug->head = debug->tail = 0;
}

void put_debug_char(unsigned char c)
{
	while (!ring_have_space(tx))
		;

	tx->data[tx->head] = c;
	tx->head = ring_next_head(tx);
}

unsigned char get_debug_char(void)
{
	unsigned char v;

	while (!ring_have_data(rx))
		;

	v = rx->data[rx->tail];
	rx->tail = ring_next_tail(rx);

	return v;
}

void put_exception_char(char c)
{
	debug->data[debug->head] = c;
	debug->head = ring_next_head(debug);
}
