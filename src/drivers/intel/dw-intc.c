#include <stdint.h>

#include <sof/dw-intc.h>
#include <sof/interrupt.h>
#include <sof/io.h>

#include <platform/interrupt.h>

#define ffs(i) __builtin_ffs(i)

static inline void intc_write(uint32_t reg, uint32_t value)
{
	io_reg_write(reg, value);
}

static inline uint32_t intc_read(uint32_t reg)
{
	return io_reg_read(reg);
}

static void dw_intc_irq_mask(uint32_t irq, uint32_t mask)
{
	struct irq_desc *parent = platform_irq_get_parent(irq);
	if (!parent)
		return;

	switch (parent->id) {
	case IRQ_DW_INTC_HIGH_ID:
		intc_write(SUE_DW_ICTL_IRQ_INTEN_H,
			   intc_read(SUE_DW_ICTL_IRQ_INTEN_H) & ~(1 << SOF_IRQ_BIT(irq)));
		break;
	case IRQ_DW_INTC_LOW_ID:
		intc_write(SUE_DW_ICTL_IRQ_INTEN_L,
			   intc_read(SUE_DW_ICTL_IRQ_INTEN_L) & ~(1 << SOF_IRQ_BIT(irq)));
	}
}

static void dw_intc_irq_unmask(uint32_t irq, uint32_t mask)
{
	struct irq_desc *parent = platform_irq_get_parent(irq);
	if (!parent)
		return;

	switch (parent->id) {
	case IRQ_DW_INTC_HIGH_ID:
		intc_write(SUE_DW_ICTL_IRQ_INTEN_H,
			   intc_read(SUE_DW_ICTL_IRQ_INTEN_H) | (1 << SOF_IRQ_BIT(irq)));
		break;
	case IRQ_DW_INTC_LOW_ID:
		intc_write(SUE_DW_ICTL_IRQ_INTEN_L,
			   intc_read(SUE_DW_ICTL_IRQ_INTEN_L) | (1 << SOF_IRQ_BIT(irq)));
	}
}

static void dw_intc_irq_handler(const struct cavs_irq *arg)
{
	struct irq_desc *child, *desc = arg->desc;
	struct list_item *item;
	uint32_t mask;

	switch (desc->id) {
	case IRQ_DW_INTC_HIGH_ID:
		mask = intc_read(SUE_DW_ICTL_IRQ_MASKSTATUS_H);
		break;
	case IRQ_DW_INTC_LOW_ID:
		mask = intc_read(SUE_DW_ICTL_IRQ_MASKSTATUS_L);
		break;
	default:
		return;
	}

	while (mask) {
		unsigned int bit = ffs(mask) - 1;

		mask &= ~(1 << bit);

		list_for_item (item, desc->child + bit) {
			child = container_of(item, struct irq_desc, irq_list);
			child->handler(child->handler_arg);
		}
	}
}

static struct cavs_irq_ops dw_intc_ops = {
	.mask		= dw_intc_irq_mask,
	.unmask		= dw_intc_irq_unmask,
	.handler	= dw_intc_irq_handler,
};

/* INTC is connected to bit 6 on level2 */
#define SUE_DW_INTC_LOW_IRQ  SOF_ID_IRQ(IRQ_DW_INTC_LOW_ID,  6, 0, cpu_get_id(), IRQ_NUM_EXT_LEVEL2)
#define SUE_DW_INTC_HIGH_IRQ SOF_ID_IRQ(IRQ_DW_INTC_HIGH_ID, 6, 0, cpu_get_id(), IRQ_NUM_EXT_LEVEL2)

int dw_intc_irq_init(void)
{
	int ret;

	/* Disable all interrupts - we will enable them individually */
	intc_write(SUE_DW_ICTL_IRQ_INTEN_H, 0);
	intc_write(SUE_DW_ICTL_IRQ_INTEN_L, 0);
	/* Unmask all interrupts - they stay unmasked all the time */
	intc_write(SUE_DW_ICTL_IRQ_INTMASK_H, 0);
	intc_write(SUE_DW_ICTL_IRQ_INTMASK_L, 0);

	/*
	 * Need two instances - for low and high 32 bits, they will get
	 * different IDs
	 */
	ret = platform_register_interrupt_controller(SUE_DW_INTC_LOW_IRQ,
						     &dw_intc_ops, NULL);
	if (ret < 0)
		return ret;

	return platform_register_interrupt_controller(SUE_DW_INTC_HIGH_IRQ,
						      &dw_intc_ops, NULL);
}
