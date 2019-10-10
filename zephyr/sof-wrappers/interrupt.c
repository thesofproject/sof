#include <device.h>

#include <sof/interrupt.h>
#include <sof/interrupt-map.h>

static uint32_t to_zephyr_irq(uint32_t sof_irq) {
	return SOC_AGGREGATE_IRQ(SOF_IRQ_BIT(sof_irq),
				 SOF_IRQ_NUMBER(sof_irq));
}

int interrupt_register(uint32_t irq, int unmask, void (*handler)(void *arg),
		       void *arg)
{
	uint32_t zephyr_irq = to_zephyr_irq(irq);
	int ret;

	ret = irq_connect_dynamic(zephyr_irq, 0, handler, arg, 0);

	if ((ret == 0) && (IRQ_AUTO_UNMASK == unmask)) {
		irq_enable(zephyr_irq);
	}

	return ret;
}

void interrupt_unregister(uint32_t irq)
{
	uint32_t zephyr_irq = to_zephyr_irq(irq);

	/*
	 * There is no "unregister" (or "disconnect") for
	 * interrupts in Zephyr. So just disable it.
	 */
	irq_disable(zephyr_irq);
}

uint32_t interrupt_enable(uint32_t irq)
{
	irq_enable(to_zephyr_irq(irq));

	return 0;
}

uint32_t interrupt_disable(uint32_t irq)
{
	irq_disable(to_zephyr_irq(irq));

	return 0;
}
