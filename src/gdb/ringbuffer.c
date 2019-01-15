#include <sof/gdb/ringbuffer.h>

volatile struct ring * const rx = (void *) 0x9e008000;
volatile struct ring * const tx = (void *) 0x9e008120;
volatile struct ring * const debug = (void *) 0x9e008220;

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
