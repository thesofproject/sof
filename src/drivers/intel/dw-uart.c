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
#include <sof/dw-uart.h>
#include <sof/io.h>
#include <sof/sof.h>
#include <sof/string.h>
#include <sof/wait.h>

#include <platform/platform.h>

#include "dw-uart-priv.h"

struct dw_uart_device_full {
	struct dw_uart_device common;
	uint32_t irq;
	completion_t complete;
};

static struct dw_uart_device_full uart_dev = {
	.common = {
		.port = PLATFORM_LL_UART_REG_BASEADDR,
		.retry = PLATFORM_LL_UART_RETRY,
	},
	/* Keep the CPU field 0 to be able to set it to the CPU, we're running on */
	.irq = IRQ_EXT_HOST_UART(0),
};

/* Actually the FIFO size can be read out */
#define DW_UART_FIFO_SIZE	64
/* Using a ring buffer only makes sense when using a TX underrun IRQ */
#define DW_UART_RING_SIZE 4096

static uint8_t *dw_uart_ring;
static bool dw_uart_ring_empty = true;
/* Write to head, read from tail */
static unsigned int dw_uart_ring_head;
static unsigned int dw_uart_ring_tail;
/* Protect the ring */
static spinlock_t dw_uart_ring_lock;

#define uart_read(d, r) uart_read_common(&d->common, r)
#define uart_write(d, r, x) uart_write_common(&(d)->common, r, x)

static void dw_uart_irq_handler(void *data)
{
	struct dw_uart_device_full *dev = data;
	uint32_t iir = uart_read(dev, SUE_UART_REG_IIR);
	unsigned int count = DW_UART_FIFO_SIZE, low = dw_uart_ring_tail, high, i;

	uart_write(dev, SUE_UART_REG_IER, 0);

	if ((iir & 0xf) != IIR_THR_EMPTY || dw_uart_ring_empty)
		return;

	spin_lock(&dw_uart_ring_lock);

	if (dw_uart_ring_tail < dw_uart_ring_head)
		high = dw_uart_ring_head;
	else
		high = DW_UART_RING_SIZE;

	if (high - low > count)
		high = low + count;
	else
		count = high - low;

	for (i = 0; i < count; i++) {
		uint32_t value = dw_uart_ring[low + i];

		uart_write(dev, SUE_UART_REG_THR, value);
	}

	dw_uart_ring_tail += count;
	if (dw_uart_ring_tail == DW_UART_RING_SIZE)
		dw_uart_ring_tail = 0;

	if (dw_uart_ring_tail == dw_uart_ring_head) {
		dw_uart_ring_empty = true;
		wait_completed(&dev->complete);
		return;
	}

	/* more data in the ring buffer */

	if (count == DW_UART_FIFO_SIZE) {
		/* FIFO full, continue after the next IRQ */
		uart_write(dev, SUE_UART_REG_IER, IER_PTIME | IER_ETBEI);
		goto unlock;
	}

	count = DW_UART_FIFO_SIZE - count;

	/* We wrapped, tail now == 0 */
	low = 0;
	high = dw_uart_ring_head;

	if (high > count)
		high = count;
	else
		count = high;

	for (i = 0; i < count; i++) {
		uint32_t value = dw_uart_ring[i];

		uart_write(dev, SUE_UART_REG_THR, value);
	}

	dw_uart_ring_tail += count;
	if (dw_uart_ring_tail == dw_uart_ring_head) {
		dw_uart_ring_empty = true;
		wait_completed(&dev->complete);
	} else {
		uart_write(dev, SUE_UART_REG_IER, IER_PTIME | IER_ETBEI);
	}

unlock:
	spin_unlock(&dw_uart_ring_lock);
}

void dw_uart_init(void)
{
	struct dw_uart_device_full *dev = &uart_dev;
	unsigned int irq;

	/* first call - allocate the ring */
	dw_uart_ring = rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
			       DW_UART_RING_SIZE);
	if (!dw_uart_ring)
		return;

	wait_init(&dev->complete);
	spinlock_init(&dw_uart_ring_lock);

	/* Register interrupt for the same core, where we're running */
	irq = dev->irq | (cpu_get_id() << SOF_IRQ_CPU_SHIFT);
	if (!interrupt_register(irq, IRQ_AUTO_UNMASK, dw_uart_irq_handler, dev))
		interrupt_enable(irq);
}

void dw_uart_write_word(uint32_t word)
{
	uint32_t flags;

	spin_lock_irq(&dw_uart_ring_lock, flags);
	dw_uart_write_word_internal(&uart_dev.common, word);
	spin_unlock_irq(&dw_uart_ring_lock, flags);
}

static void dw_uart_wait(void)
{
	struct dw_uart_device_full * const dev = &uart_dev;
	int ret;

	/* 100ms should be enough */
	dev->complete.timeout = 100000;
	ret = wait_for_completion_timeout(&dev->complete);
	if (ret < 0) {
		/* Timeout... */
		dw_uart_write_word(0x6f000000);
		dw_uart_write_word(uart_read(dev, SUE_UART_REG_IIR));
	}
}

int dw_uart_write_nowait(const char *data, uint32_t size)
{
	struct dw_uart_device_full * const dev = &uart_dev;
	unsigned int tail_room, head_room, count;
	uint32_t flags;

	if (!dw_uart_ring)
		/* No buffer, abort */
		return -ENOBUFS;

	spin_lock_irq(&dw_uart_ring_lock, flags);

	if (size <= 0 ||
	    (!dw_uart_ring_empty &&
	     dw_uart_ring_tail == dw_uart_ring_head)) {
		/* No data or ring full */
		spin_unlock_irq(&dw_uart_ring_lock, flags);
		return size;
	}

	if (dw_uart_ring_tail <= dw_uart_ring_head) {
		head_room = DW_UART_RING_SIZE - dw_uart_ring_head;
		tail_room = dw_uart_ring_tail;
	} else {
		head_room = dw_uart_ring_tail - dw_uart_ring_head;
		tail_room = 0;
	}

	count = min(head_room, size);
	arch_memcpy(dw_uart_ring + dw_uart_ring_head, data, count);

	dw_uart_ring_head += count;
	if (dw_uart_ring_head == DW_UART_RING_SIZE)
		dw_uart_ring_head = 0;

	size -= count;

	if (size && tail_room) {
		count = min(tail_room, size);
		arch_memcpy(dw_uart_ring, data + head_room, count);
		dw_uart_ring_head = count;
		size -= count;
	}

	dw_uart_ring_empty = false;

	wait_clear(&dev->complete);

	/* enable the TX empty interrupt */
	uart_write(dev, SUE_UART_REG_IER, IER_PTIME | IER_ETBEI);

	spin_unlock_irq(&dw_uart_ring_lock, flags);

	return size;
}

/* Block until all the data is at least in the ring buffer */
int dw_uart_write(const char *data, uint32_t size)
{
	for (;;) {
		int ret = dw_uart_write_nowait(data, size);
		if (ret <= 0)
			return ret;

		data += size - ret;
		size = ret;

		dw_uart_wait();
	}

	return 0;
}
