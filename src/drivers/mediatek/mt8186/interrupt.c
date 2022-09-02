// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek. All rights reserved.
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
//         Tinghan Shen <tinghan.shen@mediatek.com>

#include <errno.h>
#include <inttypes.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <rtos/interrupt.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PENDING_IRQ_INDEX_MAX 32

/* d2e3f730-df39-42ee-81a8-39bfb4d024c2 */
DECLARE_SOF_UUID("irq-mt8186", irq_mt8186_uuid, 0xd2e3f730, 0xdf39, 0x42ee,
		 0x81, 0xa8, 0x39, 0xbf, 0xb4, 0xd0, 0x24, 0xc2);

DECLARE_TR_CTX(int_tr, SOF_UUID(irq_mt8186_uuid), LOG_LEVEL_INFO);

static void mtk_irq_init(void)
{
	/* disable all ADSP IRQ */
	io_reg_write(MTK_ADSP_IRQ_EN, 0);

	 /* mask all IRQs between ADSP and other subsys */
	io_reg_write(MTK_ADSP_IRQ_MASK, MTK_DSP_OUT_IRQ_MASK);
}

static void mtk_irq_mask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	if (!desc) {
		io_reg_update_bits(MTK_ADSP_IRQ_EN, BIT(irq), 0);
	} else {
		switch (desc->irq) {
		case MTK_DSP_IRQ_MAILBOX:
			io_reg_update_bits(MTK_ADSP_IRQ_EN, BIT(desc->irq), 0);
			break;
		default:
			tr_err(&int_tr, "Invalid interrupt %d", desc->irq);
			return;
		}
	}
}

static void mtk_irq_unmask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	if (!desc) {
		io_reg_update_bits(MTK_ADSP_IRQ_EN, BIT(irq), BIT(irq));
	} else {
		switch (desc->irq) {
		case MTK_DSP_IRQ_MAILBOX:
			io_reg_update_bits(MTK_ADSP_IRQ_EN, BIT(desc->irq), BIT(desc->irq));
			break;
		default:
			tr_err(&int_tr, "Invalid interrupt %d", desc->irq);
			return;
		}
	}
}

static uint32_t mtk_irq_group_pending_status(uint32_t irq)
{
	uint32_t irq_status = 0;

	if (irq == MTK_DSP_IRQ_MAILBOX &&
	    (io_reg_read(MTK_ADSP_IRQ_STATUS) & BIT(MTK_DSP_IRQ_MAILBOX))) {
		irq_status = io_reg_read(MTK_MBOX_IRQ_IN) & MTK_DSP_MBOX_MASK;
	}

	return irq_status;
}

static uint32_t mtk_get_pending_index(uint32_t current, uint32_t *next)
{
	uint32_t index;

	if (current == 0)
		return PENDING_IRQ_INDEX_MAX;

	/* ffs returns one plus the index of the least significant 1-bit of input int */
	index = ffs(current) - 1;

	/* remove the handling index from current pending status */
	*next = current & ~(1ull << index);

	return index;
}

static inline void mtk_handle_group_pending_irq(struct irq_cascade_desc *cascade,
						uint32_t line_index, uint32_t status)
{
	int core = cpu_get_id();
	struct list_item *clist;
	struct irq_desc *child = NULL;
	uint32_t idx;
	uint32_t next_status;
	bool handled;
	k_spinlock_key_t key;

	idx = mtk_get_pending_index(status, &next_status);
	while (idx < PENDING_IRQ_INDEX_MAX) {
		handled = false;

		key = k_spin_lock(&cascade->lock);
		list_for_item(clist, &cascade->child[idx].list) {
			child = container_of(clist, struct irq_desc, irq_list);

			if (child->handler && (child->cpu_mask & 1 << core)) {
				child->handler(child->handler_arg);
				handled = true;
			}
		}
		k_spin_unlock(&cascade->lock, key);

		if (!handled) {
			tr_err(&int_tr, "Not handle irq %u in group %u",
			       idx, line_index);
		}

		idx = mtk_get_pending_index(next_status, &next_status);
	}
}

static inline void mtk_irq_group_handler(void *data, uint32_t line_index)
{
	struct irq_desc *parent = data;
	struct irq_cascade_desc *cascade =
		container_of(parent, struct irq_cascade_desc, desc);
	uint32_t status;

	status = mtk_irq_group_pending_status(line_index);
	if (status)
		mtk_handle_group_pending_irq(cascade, line_index, status);
	else
		tr_err(&int_tr, "No pending irq in group %d", line_index);
}

#define DEFINE_IRQ_HANDLER(n) \
	static void irqhandler_##n(void *arg) \
	{ \
		mtk_irq_group_handler(arg, n); \
	}

DEFINE_IRQ_HANDLER(2)

static const char mtk_irq_mailbox[] = "mtk_irq_mailbox";

static const struct irq_cascade_ops irq_ops = {
	.mask = mtk_irq_mask,
	.unmask = mtk_irq_unmask,
};

static const struct irq_cascade_tmpl dsp_irq[] = {
	{
		.name = mtk_irq_mailbox,
		.irq = MTK_DSP_IRQ_MAILBOX,
		.handler = irqhandler_2,
		.ops = &irq_ops,
		.global_mask = false,
	},
};

int mtk_irq_group_id(uint32_t in_irq)
{
	if (in_irq >= MTK_MAX_IRQ_NUM)
		in_irq -= MTK_MAX_IRQ_NUM;

	return interrupt_get_irq(in_irq, dsp_irq[0].name);
}

void platform_interrupt_init(void)
{
	int i;

	mtk_irq_init();
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
