/*
 * Copyright (c) 2018, Intel Corporation
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
 *
 * Author: Keyon Jie <yang.jie@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 *
 */

#include <sof/alloc.h>
#include <sof/sof.h>
#include <sof/interrupt.h>
#include <sof/interrupt-map.h>
#include <sof/cpu.h>
#include <arch/interrupt.h>
#include <platform/interrupt.h>
#include <platform/platcfg.h>
#include <platform/shim.h>
#include <stdint.h>
#include <stdlib.h>

static struct list_item intc = {&intc, &intc};

static void platform_interrupt_handler(void *arg)
{
	const struct cavs_irq *cavs = arg;

	cavs->ops->handler(cavs);
}

int platform_register_interrupt_controller(int irq,
				const struct cavs_irq_ops *ops, void *arg)
{
	struct cavs_irq *cavs;
	struct irq_desc *parent, *desc;
	struct list_item *item;
	unsigned int i;
	int ret;

	if (!ops || !ops->mask || !ops->unmask || !ops->handler)
		return -EINVAL;

	list_for_item (item, &intc) {
		cavs = container_of(item, struct cavs_irq, ext_ctrl);

		if (irq == cavs->desc->irq)
			return -EBUSY;
	}

	cavs = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM,
		       sizeof(struct cavs_irq));
	if (!cavs)
		return -ENOMEM;

	/* Grab the parent interrupt */
	ret = interrupt_register(irq, IRQ_AUTO_UNMASK,
				 platform_interrupt_handler, cavs);
	if (ret < 0)
		goto free;

	/* Find our newly registered IRQ descriptor */
	parent = platform_irq_get_parent(irq);
	if (!parent || list_is_empty(&parent->child[SOF_IRQ_BIT(irq)])) {
		ret = -EINVAL;
		goto unregister;
	}

	list_for_item (item, &parent->child[SOF_IRQ_BIT(irq)]) {
		desc = container_of(item, struct irq_desc, irq_list);
		if (desc->irq == irq)
			break;
	}

	if (&desc->irq_list == &parent->child[SOF_IRQ_BIT(irq)]) {
		/* Not found... */
		ret = -EPROTO;
		goto unregister;
	}

	spinlock_init(&desc->lock);
	for (i = 0; i < ARRAY_SIZE(desc->child); i++)
		list_init(desc->child + i);

	cavs->desc = desc;
	cavs->ops = ops;
	cavs->arg = arg;

	list_item_append(&cavs->ext_ctrl, &intc);

	interrupt_enable(irq);

	return 0;

unregister:
	interrupt_unregister(irq);
free:
	rfree(cavs);

	return ret;
}

static inline void irq_lvl2_handler(void *data, int level, uint32_t ilxsd,
				    uint32_t ilxmsd, uint32_t ilxmcd)
{
	struct irq_desc *parent = (struct irq_desc *)data;
	struct irq_desc *child = NULL;
	struct list_item *clist;
	uint32_t status;
	uint32_t i = 0;
	uint32_t unmask = 0;

	/* mask the parent IRQ */
	arch_interrupt_disable_mask(1 << level);

	/* mask all child interrupts */
	status = irq_read(ilxsd);
	irq_write(ilxmsd, status);

	/* handle each child */
	while (status) {
		/* any IRQ for this child bit ? */
		if ((status & 0x1) == 0)
			goto next;

		/* get child if any and run handler */
		list_for_item(clist, &parent->child[i]) {
			child = container_of(clist, struct irq_desc, irq_list);

			if (child && child->handler) {
				child->handler(child->handler_arg);
				unmask = child->unmask;
			} else {
				/* nobody cared ? */
				trace_irq_error("irq_lvl2_handler() error: "
						"nbc");
			}
		}

		/* unmask this bit i interrupt */
		if (unmask)
			irq_write(ilxmcd, 0x1 << i);

next:
		status >>= 1;
		i++;
	}

	/* clear parent and unmask */
	arch_interrupt_clear(level);
	arch_interrupt_enable_mask(1 << level);
}

#define IRQ_LVL2_HANDLER(n) int core = cpu_get_id(); \
				irq_lvl2_handler(data, \
				 IRQ_NUM_EXT_LEVEL##n, \
				 REG_IRQ_IL##n##SD(core), \
				 REG_IRQ_IL##n##MSD(core), \
				 REG_IRQ_IL##n##MCD(core))

static void irq_lvl2_level2_handler(void *data)
{
	IRQ_LVL2_HANDLER(2);
}

static void irq_lvl2_level3_handler(void *data)
{
	IRQ_LVL2_HANDLER(3);
}

static void irq_lvl2_level4_handler(void *data)
{
	IRQ_LVL2_HANDLER(4);
}

static void irq_lvl2_level5_handler(void *data)
{
	IRQ_LVL2_HANDLER(5);
}

/* DSP internal interrupts */
static struct irq_desc dsp_irq[PLATFORM_CORE_COUNT][4] = {
	#if defined(CONFIG_CANNONLAKE) || defined(CONFIG_ICELAKE)
	{{SOF_IRQ(IRQ_NUM_EXT_LEVEL2, 0, 0, 0), irq_lvl2_level2_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL3, 0, 0, 0), irq_lvl2_level3_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL4, 0, 0, 0), irq_lvl2_level4_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL5, 0, 0, 0), irq_lvl2_level5_handler, } },

	{{SOF_IRQ(IRQ_NUM_EXT_LEVEL2, 0, 0, 0), irq_lvl2_level2_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL3, 0, 0, 0), irq_lvl2_level3_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL4, 0, 0, 0), irq_lvl2_level4_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL5, 0, 0, 0), irq_lvl2_level5_handler, } },
	#endif

	{{SOF_IRQ(IRQ_NUM_EXT_LEVEL2, 0, 0, 0), irq_lvl2_level2_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL3, 0, 0, 0), irq_lvl2_level3_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL4, 0, 0, 0), irq_lvl2_level4_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL5, 0, 0, 0), irq_lvl2_level5_handler, } },

	{{SOF_IRQ(IRQ_NUM_EXT_LEVEL2, 0, 0, 0), irq_lvl2_level2_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL3, 0, 0, 0), irq_lvl2_level3_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL4, 0, 0, 0), irq_lvl2_level4_handler, },
	 {SOF_IRQ(IRQ_NUM_EXT_LEVEL5, 0, 0, 0), irq_lvl2_level5_handler, } },
};

struct irq_desc *platform_irq_get_parent(uint32_t irq)
{
	int core = SOF_IRQ_CPU(irq);

	if (SOF_IRQ_LEVEL(irq) > 0xf) {
		/* cascaded interrupt */
		struct list_item *item;

		list_for_item (item, &intc) {
			const struct cavs_irq *cavs = container_of(item,
						struct cavs_irq, ext_ctrl);
			struct irq_desc *desc = cavs->desc;

			if (SOF_IRQ_NUMBER(irq) == SOF_IRQ_NUMBER(desc->irq) &&
			    SOF_IRQ_ID(irq) == desc->id)
				return desc;
		}
		/* Shouldn't get here... */
	}

	switch (SOF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL2:
		return &dsp_irq[core][0];
	case IRQ_NUM_EXT_LEVEL3:
		return &dsp_irq[core][1];
	case IRQ_NUM_EXT_LEVEL4:
		return &dsp_irq[core][2];
	case IRQ_NUM_EXT_LEVEL5:
		return &dsp_irq[core][3];
	default:
		return NULL;
	}
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{
	int core = SOF_IRQ_CPU(irq);

	if (SOF_IRQ_LEVEL(irq) > 0xf) {
		/* cascaded interrupt */
		struct list_item *item;

		list_for_item (item, &intc) {
			const struct cavs_irq *cavs = container_of(item,
						struct cavs_irq, ext_ctrl);
			const struct irq_desc *desc = cavs->desc;

			if (SOF_IRQ_NUMBER(irq) == SOF_IRQ_NUMBER(desc->irq) &&
			    SOF_IRQ_ID(irq) == desc->id) {
				cavs->ops->mask(irq, mask);
				break;
			}
		}

		return;
	}

	/* mask external interrupt bit */
	switch (SOF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MSD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	default:
		break;
	}
}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{
	int core = SOF_IRQ_CPU(irq);

	if (SOF_IRQ_LEVEL(irq) > 0xf) {
		/* cascaded interrupt */
		struct list_item *item;

		list_for_item (item, &intc) {
			const struct cavs_irq *cavs = container_of(item,
						struct cavs_irq, ext_ctrl);
			const struct irq_desc *desc = cavs->desc;

			if (SOF_IRQ_NUMBER(irq) == SOF_IRQ_NUMBER(desc->irq) &&
			    SOF_IRQ_ID(irq) == desc->id) {
				cavs->ops->unmask(irq, mask);
				break;
			}
		}

		return;
	}

	/* unmask external interrupt bit */
	switch (SOF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL5:
		irq_write(REG_IRQ_IL5MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		irq_write(REG_IRQ_IL4MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		irq_write(REG_IRQ_IL3MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		irq_write(REG_IRQ_IL2MCD(core), 1 << SOF_IRQ_BIT(irq));
		break;
	default:
		break;
	}
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
}

void platform_interrupt_init(void)
{
	int i, j;
	int core = cpu_get_id();

	/* mask all external IRQs by default */
	irq_write(REG_IRQ_IL2MSD(core), REG_IRQ_IL2MD_ALL);
	irq_write(REG_IRQ_IL3MSD(core), REG_IRQ_IL3MD_ALL);
	irq_write(REG_IRQ_IL4MSD(core), REG_IRQ_IL4MD_ALL);
	irq_write(REG_IRQ_IL5MSD(core), REG_IRQ_IL5MD_ALL);

	for (i = 0; i < ARRAY_SIZE(dsp_irq[core]); i++) {
		spinlock_init(&dsp_irq[core][i].lock);
		for (j = 0; j < PLATFORM_IRQ_CHILDREN; j++)
			list_init(&dsp_irq[core][i].child[j]);
	}
}
