// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#include <rtos/interrupt.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <sof/lib/uuid.h>
#include <platform/drivers/interrupt.h>
#include <platform/drivers/intc.h>
#include <errno.h>
#include <stdint.h>

SOF_DEFINE_REG_UUID(intc_mt8196);
DECLARE_TR_CTX(intc_tr, SOF_UUID(intc_mt8196_uuid), LOG_LEVEL_INFO);

static struct intc_desc_t intc_desc;

void intc_init(void)
{
	uint32_t word, group, irq;

	for (group = 0; group < INTC_GRP_NUM; group++) {
		for (word = 0; word < INTC_GRP_LEN; word++)
			intc_desc.grp_irqs[group][word] = 0x0;
	}

	for (word = 0; word < INTC_GRP_LEN; word++)
		intc_desc.int_en[word] = 0x0;

	for (irq = 0; irq < IRQ_MAX_CHANNEL; irq++) {
		intc_desc.irqs[irq].id = irq;
		intc_desc.irqs[irq].group = irq2grp_map[irq];
		intc_desc.irqs[irq].pol = INTC_POL_LOW;
	}

	for (word = 0; word < INTC_GRP_LEN; word++) {
		io_reg_write(INTC_IRQ_EN(word), 0x0);
		io_reg_write(INTC_IRQ_WAKE_EN(word), 0x0);
		io_reg_write(INTC_IRQ_STAGE1_EN(word), 0x0);
		io_reg_write(INTC_IRQ_POL(word), 0xFFFFFFFF);
	}

	for (group = 0; group < INTC_GRP_NUM; group++) {
		for (word = 0; word < INTC_GRP_LEN; word++)
			io_reg_write(INTC_IRQ_GRP(group, word), 0x0);
	}
}

void intc_irq_unmask(enum IRQn_Type irq)
{
	uint32_t word, group;

	if (irq < IRQ_MAX_CHANNEL && intc_desc.irqs[irq].group < INTC_GRP_NUM) {
		word = INTC_WORD(irq);
		group = intc_desc.irqs[irq].group;
		io_reg_update_bits(INTC_IRQ_EN(word), INTC_BIT(irq), INTC_BIT(irq));
	} else {
		tr_err(&intc_tr, "Invalid INTC interrupt %d", irq);
	}
}

void intc_irq_mask(enum IRQn_Type irq)
{
	uint32_t word;

	if (irq < IRQ_MAX_CHANNEL) {
		word = INTC_WORD(irq);
		io_reg_update_bits(INTC_IRQ_EN(word), INTC_BIT(irq), 0);
	} else {
		tr_err(&intc_tr, "Invalid INTC interrupt %d", irq);
	}
}

int intc_irq_enable(enum IRQn_Type irq)
{
	uint32_t word, irq_b, group, pol;
	int ret;

	if (irq < IRQ_MAX_CHANNEL && intc_desc.irqs[irq].group < INTC_GRP_NUM &&
	    intc_desc.irqs[irq].pol < INTC_POL_NUM) {
		word = INTC_WORD(irq);
		irq_b = INTC_BIT(irq);
		group = intc_desc.irqs[irq].group;
		pol = intc_desc.irqs[irq].pol;

		intc_desc.int_en[word] |= irq_b;
		intc_desc.grp_irqs[group][word] |= irq_b;
		io_reg_update_bits(INTC_IRQ_EN(word), irq_b, 0);
		if (pol == INTC_POL_HIGH)
			io_reg_update_bits(INTC_IRQ_POL(word), irq_b, 0);
		else
			io_reg_update_bits(INTC_IRQ_POL(word), irq_b, irq_b);

		io_reg_update_bits(INTC_IRQ_GRP(group, word), irq_b, irq_b);
		io_reg_update_bits(INTC_IRQ_EN(word), irq_b, irq_b);

		ret = 1;
	} else {
		tr_err(&intc_tr, "Invalid INTC interrupt %d", irq);
		ret = 0;
	}

	return ret;
}

int intc_irq_disable(enum IRQn_Type irq)
{
	uint32_t word, irq_b, group;
	int ret;

	if (irq < IRQ_MAX_CHANNEL && intc_desc.irqs[irq].group < INTC_GRP_NUM) {
		word = INTC_WORD(irq);
		irq_b = INTC_BIT(irq);
		group = intc_desc.irqs[irq].group;

		intc_desc.int_en[word] &= ~irq_b;
		intc_desc.grp_irqs[group][word] &= ~irq_b;
		io_reg_update_bits(INTC_IRQ_EN(word), irq_b, 0);
		io_reg_update_bits(INTC_IRQ_GRP(group, word), irq_b, 0);

		ret = 1;
	} else {
		tr_err(&intc_tr, "INTC fail to disable irq %u\n", irq);
		ret = 0;
	}

	return ret;
}

