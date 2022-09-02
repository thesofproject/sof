// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>

#include <sof/common.h>
#include <rtos/bit.h>
#include <rtos/interrupt.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* fa00558c-d653-4851-a03a-b21f125a9524 */
DECLARE_SOF_UUID("irq-mt8195", irq_mt8195_uuid, 0xfa00558c, 0xd653, 0x4851,
		 0xa0, 0x3a, 0xb2, 0x1f, 0x12, 0x5a, 0x95, 0x24);

DECLARE_TR_CTX(int_tr, SOF_UUID(irq_mt8195_uuid), LOG_LEVEL_INFO);

/* os timer reg value * 77ns 13M os timer
 * 1  ms: 12987* 1.5ms: 19436
 * 2  ms: 25974* 3  ms: 38961
 */

static void irq_mask_all(void)
{
	io_reg_update_bits(RG_DSP_IRQ_EN, 0xffffffff, 0x0);
	io_reg_update_bits(DSP_IRQ_EN, 0xffffffff, 0x0);
}

static void mtk_irq_mask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	int32_t in_irq, level;

	if (!desc) {
		level = GET_INTLEVEL(irq);
		in_irq = GET_INTERRUPT_ID(irq);
		if (level > MAX_IRQ_NUM) {
			tr_err(&int_tr, "Invalid interrupt %d", irq);
			return;
		}

		io_reg_update_bits(RG_DSP_IRQ_EN, BIT(in_irq), 0x0);
	} else {
		switch (desc->irq) {
		case IRQ_EXT_DOMAIN0:
			io_reg_update_bits(RG_DSP_IRQ_EN, BIT(irq + IRQ_EXT_DOMAIN0_OFFSET), 0x0);
			break;
		case IRQ_EXT_DOMAIN1:
			io_reg_update_bits(DSP_IRQ_EN, BIT(irq), 0x0);
			break;
		default:
			tr_err(&int_tr, "Invalid interrupt %d", desc->irq);
			return;
		}
	}
}

static void mtk_irq_unmask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	int32_t in_irq, level;

	if (!desc) {
		level = GET_INTLEVEL(irq);
		in_irq = GET_INTERRUPT_ID(irq);
		if (level > MAX_IRQ_NUM) {
			tr_err(&int_tr, "Invalid interrupt %d", irq);
			return;
		}

		io_reg_update_bits(RG_DSP_IRQ_EN, BIT(in_irq), BIT(in_irq));
	} else {
		switch (desc->irq) {
		case IRQ_EXT_DOMAIN0:
			io_reg_update_bits(RG_DSP_IRQ_EN, BIT(irq + IRQ_EXT_DOMAIN0_OFFSET),
					   BIT(irq + IRQ_EXT_DOMAIN0_OFFSET));
			break;
		case IRQ_EXT_DOMAIN1:
			io_reg_update_bits(DSP_IRQ_EN, BIT(irq), BIT(irq));
			break;
		default:
			tr_err(&int_tr, "Invalid interrupt %d", desc->irq);
			return;
		}
	}
}

static uint64_t mtk_get_irq_interrupts(uint32_t irq)
{
	uint32_t irq_status;

	switch (irq) {
	case IRQ_NUM_EXT_LEVEL23:
		irq_status = io_reg_read(DSP_IRQ_STATUS);
		irq_status &= IRQ_EXT_DOMAIN2_MASK;
		break;
	default:
		irq_status = io_reg_read(RG_DSP_IRQ_STATUS);
		irq_status &= IRQ_EXT_DOMAIN1_MASK;
		break;
	}
	return irq_status;
}

static int get_first_irq(uint64_t ints)
{
	return ffs(ints) - 1;
}

static inline void mtk_handle_irq(struct irq_cascade_desc *cascade,
				  uint32_t line_index, uint64_t status)
{
	int core = cpu_get_id();
	struct list_item *clist;
	struct irq_desc *child = NULL;
	k_spinlock_key_t key;
	int bit;
	bool handled;

	while (status) {
		bit = get_first_irq(status);
		handled = false;
		status &= ~(1ull << bit);

		key = k_spin_lock(&cascade->lock);

		list_for_item(clist, &cascade->child[bit].list) {
			child = container_of(clist, struct irq_desc, irq_list);

			if (child->handler && (child->cpu_mask & 1 << core)) {
				child->handler(child->handler_arg);
				handled = true;
			}
		}

		k_spin_unlock(&cascade->lock, key);

		if (!handled) {
			tr_err(&int_tr, "irq_handler(): not handled, bit %d", bit);
			if (line_index == IRQ_NUM_EXT_LEVEL23)
				io_reg_update_bits(DSP_IRQ_EN, BIT(bit), 0x0);
			else
				io_reg_update_bits(RG_DSP_IRQ_EN, BIT(bit), 0x0);
		}
	}
}

static inline void irq_handler(void *data, uint32_t line_index)
{
	struct irq_desc *parent = data;
	struct irq_cascade_desc *cascade =
		container_of(parent, struct irq_cascade_desc, desc);
	uint64_t status;

	status = mtk_get_irq_interrupts(line_index);

	if (status)
		/* Handle current interrupts */
		mtk_handle_irq(cascade, line_index, status);
	else
		tr_err(&int_tr, "invalid interrupt status");
}

#define DEFINE_IRQ_HANDLER(n) \
	static void irqhandler_##n(void *arg) \
	{ \
		irq_handler(arg, n); \
	}

DEFINE_IRQ_HANDLER(1)
DEFINE_IRQ_HANDLER(23)

static const char mtk_irq_ext_domain0[] = "mtk_irq_ext_domain0";
static const char mtk_irq_ext_domain1[] = "mtk_irq_ext_domain1";

static const struct irq_cascade_ops irq_ops = {
	.mask = mtk_irq_mask,
	.unmask = mtk_irq_unmask,
};

static const struct irq_cascade_tmpl dsp_irq[] = {
	{
		.name = mtk_irq_ext_domain0,
		.irq = IRQ_NUM_EXT_LEVEL01,
		.handler = irqhandler_1,
		.ops = &irq_ops,
		.global_mask = false,
	},
	{
		.name = mtk_irq_ext_domain1,
		.irq = IRQ_NUM_EXT_LEVEL23,
		.handler = irqhandler_23,
		.ops = &irq_ops,
		.global_mask = false,
	},
};

uint32_t mtk_get_irq_domain_id(int32_t irq)
{
	uint32_t in_irq = GET_INTERRUPT_ID(irq);
	int32_t level = GET_INTLEVEL(irq);

	if (in_irq >= DOMAIN1_MAX_IRQ_NUM)
		in_irq -= DOMAIN1_MAX_IRQ_NUM;

	if (level == IRQ_EXT_DOMAIN0)
		return interrupt_get_irq(in_irq, dsp_irq[0].name);
	else
		return interrupt_get_irq(in_irq, dsp_irq[1].name);
}

void platform_interrupt_init(void)
{
	int i;

	irq_mask_all();

	for (i = 0; i < ARRAY_SIZE(dsp_irq); i++)
		interrupt_cascade_register(dsp_irq + i);
}

void platform_interrupt_set(uint32_t irq)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_set(irq);
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_clear(irq);
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	struct irq_cascade_desc *cascade = interrupt_get_parent(irq);

	if (cascade && cascade->ops->mask)
		cascade->ops->mask(&cascade->desc, irq - cascade->irq_base,
				   cpu);
	else
		mtk_irq_mask(NULL, irq, 0);
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	struct irq_cascade_desc *cascade = interrupt_get_parent(irq);

	if (cascade && cascade->ops->unmask)
		cascade->ops->unmask(&cascade->desc, irq - cascade->irq_base,
				     cpu);
	else
		mtk_irq_unmask(NULL, irq, 0);
}
