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

#include <sof/alloc.h>
#include <sof/cpu.h>
#include <sof/io.h>
#include <sof/lock.h>
#include <sof/sof.h>
#include <sof/string.h>
#include <sof/uart.h>
#include <sof/wait.h>
#include <sof/math/numbers.h>

#include <platform/platform.h>

#include "uart-priv.h"

struct dw_uart_device_full {
	struct dw_uart_device base;
	completion_t complete;
	uint8_t *dw_uart_ring;
	bool dw_uart_ring_empty;
	/* Write to head, read from tail */
	unsigned int dw_uart_ring_head;
	unsigned int dw_uart_ring_tail;
	/* Protect the ring */
	spinlock_t dw_uart_ring_lock;
};

/* Actually the FIFO size can be read out */
#define DW_UART_FIFO_SIZE	64
/* Using a ring buffer only makes sense when using a TX underrun IRQ */
#define DW_UART_RING_SIZE	4096

#define uart_read_reg(d, r) dw_uart_read_common(&(d)->base, r)
#define uart_write_reg(d, r, x) dw_uart_write_common(&(d)->base, r, x)

static void dw_uart_irq_handler(void *data)
{
	struct dw_uart_device_full *dev = data;
	uint32_t iir = uart_read_reg(dev, SUE_UART_REG_IIR);
	unsigned int count = DW_UART_FIFO_SIZE, low = dev->dw_uart_ring_tail,
		high, i;

	/* Diisable all interrupts */
	uart_write_reg(dev, SUE_UART_REG_IER, 0);

	/*
	 * We're only interested in the "TX empty" interrupt and only if we have
	 * data to send
	 */
	if ((iir & 0xf) != IIR_THR_EMPTY || dev->dw_uart_ring_empty)
		return;

	spin_lock(&dev->dw_uart_ring_lock);

	/* Calculate data offsets, taking wrapping into account */
	if (dev->dw_uart_ring_tail < dev->dw_uart_ring_head)
		high = dev->dw_uart_ring_head;
	else
		high = DW_UART_RING_SIZE;

	if (high - low > count)
		high = low + count;
	else
		count = high - low;

	for (i = 0; i < count; i++) {
		uint32_t value = dev->dw_uart_ring[low + i];

		uart_write_reg(dev, SUE_UART_REG_THR, value);
	}

	dev->dw_uart_ring_tail += count;
	if (dev->dw_uart_ring_tail == DW_UART_RING_SIZE)
		dev->dw_uart_ring_tail = 0;

	if (dev->dw_uart_ring_tail == dev->dw_uart_ring_head) {
		dev->dw_uart_ring_empty = true;
		/* Sent the last data chunk, wake up the sender */
		wait_completed(&dev->complete);
		return;
	}

	/* more data in the ring buffer */

	if (count == DW_UART_FIFO_SIZE) {
		/* FIFO full, continue after the next IRQ */
		uart_write_reg(dev, SUE_UART_REG_IER, IER_PTIME | IER_ETBEI);
		goto unlock;
	}

	count = DW_UART_FIFO_SIZE - count;

	/* We wrapped, tail now == 0 */
	low = 0;
	high = dev->dw_uart_ring_head;

	if (high > count)
		high = count;
	else
		count = high;

	/* Send more data from the beginning of the ring buffer */
	for (i = 0; i < count; i++) {
		uint32_t value = dev->dw_uart_ring[i];

		uart_write_reg(dev, SUE_UART_REG_THR, value);
	}

	dev->dw_uart_ring_tail += count;
	if (dev->dw_uart_ring_tail == dev->dw_uart_ring_head) {
		dev->dw_uart_ring_empty = true;
		/* All sent, wake the sender */
		wait_completed(&dev->complete);
	} else {
		/* More to send, wait for the next IRQ */
		uart_write_reg(dev, SUE_UART_REG_IER, IER_PTIME | IER_ETBEI);
	}

unlock:
	spin_unlock(&dev->dw_uart_ring_lock);
}

static void dw_uart_write_word(struct uart *uart, uint32_t word)
{
	struct dw_uart_device_full * const dev = container_of(uart,
				struct dw_uart_device_full, base.common);
	uint32_t flags;

	spin_lock_irq(&dev->dw_uart_ring_lock, flags);
	dw_uart_write_word_internal(&dev->base, word);
	spin_unlock_irq(&dev->dw_uart_ring_lock, flags);
}

static int dw_uart_wait(struct uart *uart)
{
	struct dw_uart_device_full * const dev = container_of(uart,
				struct dw_uart_device_full, base.common);

	/* 100ms should be enough */
	dev->complete.timeout = 100000;
	return wait_for_completion_timeout(&dev->complete);
}

static int dw_uart_write_nowait(struct uart *uart, const char *data,
				uint32_t size)
{
	struct dw_uart_device_full * const dev = container_of(uart,
				struct dw_uart_device_full, base.common);
	unsigned int tail_room, head_room, count;
	uint32_t flags;

	if (!dev->dw_uart_ring)
		/* No buffer, abort */
		return -ENOBUFS;

	spin_lock_irq(&dev->dw_uart_ring_lock, flags);

	if (size <= 0 ||
	    (!dev->dw_uart_ring_empty &&
	     dev->dw_uart_ring_tail == dev->dw_uart_ring_head)) {
		/* No data or ring full */
		spin_unlock_irq(&dev->dw_uart_ring_lock, flags);
		return size;
	}

	if (dev->dw_uart_ring_tail <= dev->dw_uart_ring_head) {
		head_room = DW_UART_RING_SIZE - dev->dw_uart_ring_head;
		tail_room = dev->dw_uart_ring_tail;
	} else {
		head_room = dev->dw_uart_ring_tail - dev->dw_uart_ring_head;
		tail_room = 0;
	}

	count = MIN(head_room, size);
	arch_memcpy(dev->dw_uart_ring + dev->dw_uart_ring_head, data, count);

	dev->dw_uart_ring_head += count;
	if (dev->dw_uart_ring_head == DW_UART_RING_SIZE)
		dev->dw_uart_ring_head = 0;

	size -= count;

	if (size && tail_room) {
		count = MIN(tail_room, size);
		arch_memcpy(dev->dw_uart_ring, data + head_room, count);
		dev->dw_uart_ring_head = count;
		size -= count;
	}

	dev->dw_uart_ring_empty = false;

	wait_clear(&dev->complete);

	/* enable the TX empty interrupt */
	uart_write_reg(dev, SUE_UART_REG_IER, IER_PTIME | IER_ETBEI);

	spin_unlock_irq(&dev->dw_uart_ring_lock, flags);

	return size;
}

/* Block until all the data is at least in the ring buffer */
static int dw_uart_write(struct uart *uart, const char *data, uint32_t size)
{
	for (;;) {
		int ret = dw_uart_write_nowait(uart, data, size);
		if (ret <= 0)
			return ret;

		data += size - ret;
		size = ret;

		ret = dw_uart_wait(uart);
		if (ret < 0)
			/*
			 * The ring buffer is full and no TX-empty interrupt was
			 * received. Abort sending
			 */
			return ret;
	}
}

static struct uart_ops dw_uart_ops = {
	.write = dw_uart_write,
	.write_nowait = dw_uart_write_nowait,
	.write_word = dw_uart_write_word,
};

struct uart *dw_uart_init(const struct uart_platform_data *pdata)
{
	struct dw_uart_device_full *dev;
	unsigned int irq;

	dev = rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->base.common.pdata = pdata;
	dev->base.common.ops = &dw_uart_ops;
	dev->dw_uart_ring_empty = true;

	wait_init(&dev->complete);
	spinlock_init(&dev->dw_uart_ring_lock);

	/* first call - allocate the ring */
	dev->dw_uart_ring = rballoc(RZONE_BUFFER, SOF_MEM_CAPS_RAM,
				    DW_UART_RING_SIZE);
	if (dev->dw_uart_ring) {
		/* TODO: handle the interrupt controller */
		irq = pdata->irq;
		if (irq >= 0 && !interrupt_register(irq, IRQ_AUTO_UNMASK,
						    dw_uart_irq_handler, dev)) {
			interrupt_enable(irq);
		} else {
			/* disable trace_event(), keep trace_point() */
			rfree(dev->dw_uart_ring);
			dev->dw_uart_ring = NULL;
		}
	}

	return &dev->base.common;
}
