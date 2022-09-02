// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Anup Kulkarni <anup.kulkarni@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>
#include <sof/common.h>
#include <platform/chip_offset_byte.h>
#include <platform/chip_registers.h>
#include <rtos/interrupt.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <xtensa/hal.h>
#include <xtensa/config/core.h>
#include <xtensa/config/specreg.h>
#include <xtensa/core-macros.h>
#include "xtos-internal.h"
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*  6533d0eb-b785-4709-84f5-347c81720189*/
DECLARE_SOF_UUID("irq-acp", irq_acp_uuid, 0x6533d0eb, 0xb785, 0x4709,
		0x84, 0xf5, 0x34, 0x7c, 0x81, 0x72, 0x01, 0x89);

DECLARE_TR_CTX(acp_irq_tr, SOF_UUID(irq_acp_uuid), LOG_LEVEL_INFO);

#define IRQ_INT_MASK(irq)	(1 << (irq))
#define RESERVED_IRQS_NUM	0
#define IRQS_NUM		8
#define IRQS_PER_LINE		1

static inline uint32_t acp_irq_status_read(uint32_t reg)
{
	return io_reg_read(PU_REGISTER_BASE + reg);
}

static inline void acp_irq_update_bits(uint32_t reg, uint32_t mask,
				      uint32_t value)
{
	io_reg_update_bits(PU_REGISTER_BASE + reg, mask, value);
}

static uint32_t acp_irq_get_status(uint32_t index)
{
	/* 0-7 interrupts are used */
	if (index > 7)
		return 0;
	return acp_irq_status_read(ACP_DSP0_INTR_STAT);
}

static void acp_irq_mask_int(uint32_t irq)
{
	uint32_t mask;

	if (irq < RESERVED_IRQS_NUM || irq >= IRQS_NUM) {
		tr_err(&acp_irq_tr, "Invalid interrupt");
		return;
	}
	mask = IRQ_INT_MASK(irq);
	acp_irq_update_bits(ACP_DSP0_INTR_CNTL, mask, 0);
}

static void acp_irq_unmask_int(uint32_t irq)
{
	uint32_t mask;

	if (irq < RESERVED_IRQS_NUM || irq >= IRQS_NUM) {
		tr_err(&acp_irq_tr, "Invalid interrupt");
		return;
	}
	mask = IRQ_INT_MASK(irq);
	acp_irq_update_bits(ACP_DSP0_INTR_CNTL, mask, mask);
}

static uint64_t acp_get_irq_interrupts(uint32_t index)
{
	return acp_irq_get_status(index);
}

static int get_first_irq(uint64_t ints)
{
	return ffs(ints) - 1;
}

static inline void acp_handle_irq(struct irq_cascade_desc *cascade,
				    uint32_t line_index, uint64_t status)
{
	int core = cpu_get_id();
	struct list_item *clist;
	struct irq_desc *child = NULL;
	int bit;
	bool handled;
	k_spinlock_key_t key;

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
			tr_err(&acp_irq_tr, "irq_handler(): not handled, bit %d",
					bit);
			acp_irq_mask_int(line_index * IRQS_PER_LINE + bit);
		}
	}
}

static inline void irq_handler(void *data, uint32_t line_index)
{
	struct irq_desc *parent = data;
	struct irq_cascade_desc *cascade =
		container_of(parent, struct irq_cascade_desc, desc);
	uint64_t status;

	status = acp_get_irq_interrupts(line_index);

	if (status)
		/* Handle current interrupts */
		acp_handle_irq(cascade, line_index, status);
	else
		tr_err(&acp_irq_tr, "invalid interrupt status");
}

#define DEFINE_IRQ_HANDLER(n) \
	static void irqhandler_##n(void *arg) \
	{ \
		irq_handler(arg, n); \
	}

DEFINE_IRQ_HANDLER(0)
DEFINE_IRQ_HANDLER(1)
DEFINE_IRQ_HANDLER(3)
DEFINE_IRQ_HANDLER(4)
DEFINE_IRQ_HANDLER(5)

static void acp_irq_mask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	acp_irq_mask_int(irq);
}

static void acp_irq_unmask(struct irq_desc *desc, uint32_t irq, unsigned int core)
{
	acp_irq_unmask_int(irq);
}

static const struct irq_cascade_ops irq_ops = {
	.mask = acp_irq_mask,
	.unmask = acp_irq_unmask,
};

static const struct irq_cascade_tmpl dsp_irq[] = {
	{
		.irq = IRQ_NUM_TIMER0,
		.handler = irqhandler_0,
		.ops = &irq_ops,
		.global_mask = false,
	}, {
		.irq = IRQ_NUM_SOFTWARE0,
		.handler = irqhandler_1,
		.ops = &irq_ops,
		.global_mask = false,
	},
	{
		.irq = IRQ_NUM_EXT_LEVEL3,
		.handler = irqhandler_3,
		.ops = &irq_ops,
		.global_mask = false,
	},
	{
		.irq = IRQ_NUM_EXT_LEVEL4,
		.handler = irqhandler_4,
		.ops = &irq_ops,
		.global_mask = false,
	},
	{
		.irq = IRQ_NUM_EXT_LEVEL5,
		.handler = irqhandler_5,
		.ops = &irq_ops,
		.global_mask = false,
	},
};

void platform_interrupt_init(void)
{
	int i;

	acp_intr_route();
	/* disable all interrupts and their service routines */
	acp_intr_disable();
	for (i = 0; i < ARRAY_SIZE(dsp_irq); i++)
		interrupt_cascade_register(dsp_irq + i);
	acp_intr_enable();
	acp_dsp_sw_intr_enable();
}

void platform_interrupt_set(uint32_t irq)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_set(irq);
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_TIMER0:
	case IRQ_NUM_SOFTWARE0:
	case IRQ_NUM_EXT_LEVEL3:
	case IRQ_NUM_EXT_LEVEL4:
	case IRQ_NUM_EXT_LEVEL5:
		if (interrupt_is_dsp_direct(irq))
			arch_interrupt_clear(irq);
		break;
	default:
		break;
	}
}

uint32_t platform_interrupt_get_enabled(void)
{
	/* TODO */
	return 0;
}

void interrupt_mask(uint32_t irq, unsigned int cpu)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_clear(irq);
}

void interrupt_unmask(uint32_t irq, unsigned int cpu)
{
	if (interrupt_is_dsp_direct(irq))
		arch_interrupt_set(irq);
}


void acp_intr_route(void)
{
	dsp_interrupt_routing_ctrl_t interrupt_rout_cntl;
	/* Routing the particular interrupts to the particular level */
	interrupt_rout_cntl = (dsp_interrupt_routing_ctrl_t) io_reg_read(
			(PU_REGISTER_BASE + DSP_INTERRUPT_ROUTING_CTRL));
	interrupt_rout_cntl.bits.dma_intr_level	= acp_interrupt_level_5;
	interrupt_rout_cntl.bits.az_sw_i2s_intr_level	 = acp_interrupt_level_5;
	interrupt_rout_cntl.bits.host_to_dsp_intr1_level = acp_interrupt_level_3;
	interrupt_rout_cntl.bits.wov_intr_level		 = acp_interrupt_level_4;
	io_reg_write((PU_REGISTER_BASE + DSP_INTERRUPT_ROUTING_CTRL),
						interrupt_rout_cntl.u32all);
}

void acp_dsp_sw_intr_enable(void)
{
	acp_dsp_sw_intr_cntl_t  sw_intr_ctrl_reg;

	xthal_set_intclear(IRQ_NUM_EXT_LEVEL5);
	sw_intr_ctrl_reg = (acp_dsp_sw_intr_cntl_t) io_reg_read((PU_REGISTER_BASE +
							ACP_DSP_SW_INTR_CNTL));
	sw_intr_ctrl_reg.bits.dsp0_to_host_intr_mask   = INTERRUPT_ENABLE;
	/* Write the Software Interrupt controller register */
	io_reg_write((PU_REGISTER_BASE + ACP_DSP_SW_INTR_CNTL), sw_intr_ctrl_reg.u32all);
	/* Enabling software interuppts */
	platform_interrupt_set(IRQ_NUM_EXT_LEVEL3);
}

void acp_intr_enable(void)
{
	acp_dsp0_intr_cntl_t interrupt_cntl;
	acp_dsp0_intr_stat_t interrupt_sts;
	acp_external_intr_enb_t ext_interrupt_enb;

	platform_interrupt_clear(IRQ_NUM_EXT_LEVEL5, 0);
	platform_interrupt_clear(IRQ_NUM_TIMER0, 0);
	interrupt_sts.u32all = 0;
	/* Clear status of all interrupts in ACP_DSP0_INTR_STAT register */
	interrupt_sts.bits.dmaiocstat		 = 0xFF;
	interrupt_sts.bits.audio_buffer_int_stat = 0x3F;
	interrupt_sts.bits.wov_dma_stat		 = INTERRUPT_ENABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP0_INTR_STAT),
					interrupt_sts.u32all);
	/* Disable the ACP to Host interrupts */
	ext_interrupt_enb.bits.acpextintrenb = INTERRUPT_ENABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_EXTERNAL_INTR_ENB),
					ext_interrupt_enb.u32all);
	interrupt_cntl = (acp_dsp0_intr_cntl_t) io_reg_read((PU_REGISTER_BASE +
							ACP_DSP0_INTR_CNTL));
	interrupt_cntl.u32all = 0;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP0_INTR_CNTL), interrupt_cntl.u32all);
	interrupt_cntl.bits.dmaiocmask			= 0xFF;
	interrupt_cntl.bits.audio_buffer_int_mask	= INTERRUPT_DISABLE;
	interrupt_cntl.bits.wov_dma_intr_mask		= INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP0_INTR_CNTL), interrupt_cntl.u32all);
	platform_interrupt_set(IRQ_NUM_EXT_LEVEL5);
	platform_interrupt_set(IRQ_NUM_EXT_LEVEL4);
}

void acp_intr_disable(void)
{
	acp_dsp0_intr_stat_t interrupt_status;
	acp_external_intr_enb_t ext_interrupt_enb;
	acp_dsp0_intr_cntl_t interrupt_cntl;

	interrupt_status = (acp_dsp0_intr_stat_t) io_reg_read((PU_REGISTER_BASE +
							ACP_DSP0_INTR_STAT));
	/* Check and Clear all the Interrupt status bits */
	interrupt_status.bits.dmaiocstat		= 0xFF;
	interrupt_status.bits.audio_buffer_int_stat	= 0x3F;
	interrupt_status.bits.wov_dma_stat		= INTERRUPT_ENABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP0_INTR_STAT), interrupt_status.u32all);
	/* Disable the ACP to Host interrupts */
	ext_interrupt_enb.bits.acpextintrenb = INTERRUPT_CLEAR;
	io_reg_write((PU_REGISTER_BASE + ACP_EXTERNAL_INTR_ENB),  ext_interrupt_enb.u32all);
	/* Disable all the required ACP interrupts
	 * in acp_dsp0_intr_cntl_t register
	 */
	interrupt_cntl = (acp_dsp0_intr_cntl_t) io_reg_read((PU_REGISTER_BASE +
							ACP_DSP0_INTR_CNTL));
	interrupt_cntl.bits.dmaiocmask			= INTERRUPT_DISABLE;
	interrupt_cntl.bits.audio_buffer_int_mask	= INTERRUPT_DISABLE;
	interrupt_cntl.bits.wov_dma_intr_mask		= INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP0_INTR_CNTL),  interrupt_cntl.u32all);
	platform_interrupt_clear(IRQ_NUM_EXT_LEVEL5, 0);
	platform_interrupt_clear(IRQ_NUM_EXT_LEVEL3, 0);
	platform_interrupt_clear(IRQ_NUM_EXT_LEVEL4, 0);
	platform_interrupt_clear(IRQ_NUM_TIMER1, 0);
	platform_interrupt_clear(IRQ_NUM_TIMER0, 0);
}

void acp_dsp_sw_intr_disable(void)
{
	acp_dsp_sw_intr_cntl_t  sw_intr_ctrl_reg;
	/* Read the register */
	sw_intr_ctrl_reg = (acp_dsp_sw_intr_cntl_t) io_reg_read((PU_REGISTER_BASE +
							ACP_DSP_SW_INTR_CNTL));
	sw_intr_ctrl_reg.bits.dsp0_to_host_intr_mask = INTERRUPT_DISABLE;
	/* Write the Software Interrupt controller register */
	io_reg_write((PU_REGISTER_BASE + ACP_DSP_SW_INTR_CNTL), sw_intr_ctrl_reg.u32all);
	platform_interrupt_clear(IRQ_NUM_EXT_LEVEL3, 0);
}

/* This function triggers a host interrupt from ACP DSP */
void acp_dsp_to_host_Intr_trig(void)
{
	acp_sw_intr_trig_t  sw_intr_trig;
	/* Read the Software Interrupt controller register and update */
	sw_intr_trig = (acp_sw_intr_trig_t) io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_ENABLE;
	/* Write the Software Interrupt controller register */
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
}

/* Clear the Acknowledge ( status) for the host to DSP inteerupt */
void acp_ack_intr_from_host(void)
{
	/* acknowledge the host interrupt */
	acp_dsp_sw_intr_stat_t sw_intr_stat;

	sw_intr_stat = (acp_dsp_sw_intr_stat_t)io_reg_read(PU_REGISTER_BASE +
							ACP_DSP_SW_INTR_STAT);
	sw_intr_stat.bits.host_to_dsp0_intr1_stat = INTERRUPT_ENABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP_SW_INTR_STAT), sw_intr_stat.u32all);
}
