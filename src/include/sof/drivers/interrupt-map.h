/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_INTERRUPT_MAP_H__
#define __SOF_DRIVERS_INTERRUPT_MAP_H__

#define SOF_IRQ_PASSIVE_LEVEL	0
#define SOF_IRQ_ID_SHIFT	29
#define SOF_IRQ_BIT_SHIFT	24
#define SOF_IRQ_LEVEL_SHIFT	16
#define SOF_IRQ_CPU_SHIFT	8
#define SOF_IRQ_NUM_SHIFT	0
#define SOF_IRQ_NUM_MASK	0xff
#define SOF_IRQ_LEVEL_MASK	0xff
#define SOF_IRQ_BIT_MASK	0x1f
#define SOF_IRQ_CPU_MASK	0xff
#define SOF_IRQ_ID_MASK	0x7

#define SOF_IRQ(_bit, _level, _cpu, _number) \
	(((_bit) << SOF_IRQ_BIT_SHIFT)	      \
	 | ((_level) << SOF_IRQ_LEVEL_SHIFT) \
	 | ((_cpu) << SOF_IRQ_CPU_SHIFT)     \
	 | ((_number) << SOF_IRQ_NUM_SHIFT))

/*
 * for chip CNL or later, a group of HW IP(GP-DMA) share
 * the same IRQ. So add id in IRQ to identify each HW IP
 * for this case, it will be 5 levels
 */
#define SOF_ID_IRQ(_id, _bit, _level, _cpu, _number) \
	(((_id) << SOF_IRQ_ID_SHIFT)	      \
	 | ((_bit) << SOF_IRQ_BIT_SHIFT)	      \
	 | ((_level) << SOF_IRQ_LEVEL_SHIFT) \
	 | ((_cpu) << SOF_IRQ_CPU_SHIFT)     \
	 | ((_number) << SOF_IRQ_NUM_SHIFT))

#if CONFIG_IRQ_MAP
/*
 * IRQs are mapped on 4 levels.
 *
 * 1. Peripheral Register bit offset.
 * 2. CPU interrupt level.
 * 3. CPU number.
 * 4. CPU interrupt number.
 */
#define SOF_IRQ_NUMBER(_irq) \
	(((_irq) >> SOF_IRQ_NUM_SHIFT) & SOF_IRQ_NUM_MASK)
#define SOF_IRQ_LEVEL(_level) \
	(((_level) >> SOF_IRQ_LEVEL_SHIFT) & SOF_IRQ_LEVEL_MASK)
#define SOF_IRQ_BIT(_bit) \
	(((_bit) >> SOF_IRQ_BIT_SHIFT) & SOF_IRQ_BIT_MASK)
#define SOF_IRQ_CPU(_cpu) \
	(((_cpu) >> SOF_IRQ_CPU_SHIFT) & SOF_IRQ_CPU_MASK)
#define SOF_IRQ_ID(_bit) \
	(((_bit) >> SOF_IRQ_ID_SHIFT) & SOF_IRQ_ID_MASK)
#else
/*
 * IRQs are directly mapped onto a single level, bit and level.
 */
#define SOF_IRQ_NUMBER(_irq)	(_irq)
#define SOF_IRQ_LEVEL(_level)	0
#define SOF_IRQ_BIT(_bit)	0
#define SOF_IRQ_CPU(_cpu)	0
#define SOF_IRQ_ID(_bit)	0
#endif

#endif /* __SOF_DRIVERS_INTERRUPT_MAP_H__ */
