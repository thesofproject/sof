/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_INTERRUPT_MAP__
#define __INCLUDE_INTERRUPT_MAP__

#define REEF_IRQ_BIT_SHIFT	24
#define REEF_IRQ_LEVEL_SHIFT	16
#define REEF_IRQ_CPU_SHIFT	8
#define REEF_IRQ_NUM_SHIFT	0
#define REEF_IRQ_NUM_MASK	0xff
#define REEF_IRQ_LEVEL_MASK	0xff
#define REEF_IRQ_BIT_MASK	0xff
#define REEF_IRQ_CPU_MASK	0xff

#define REEF_IRQ(_bit, _level, _cpu, _number) \
	((_bit << REEF_IRQ_BIT_SHIFT) \
	| (_level << REEF_IRQ_LEVEL_SHIFT)\
	| (_number << REEF_IRQ_NUM_SHIFT))

#ifdef CONFIG_IRQ_MAP
/*
 * IRQs are mapped on 4 levels.
 *
 * 1. Peripheral Register bit offset.
 * 2. CPU interrupt level.
 * 3. CPU number.
 * 4. CPU interrupt number.
 */
#define REEF_IRQ_NUMBER(_irq) \
	((_irq >> REEF_IRQ_NUM_SHIFT) & REEF_IRQ_NUM_MASK)
#define REEF_IRQ_LEVEL(_level) \
	((_level >> REEF_IRQ_LEVEL_SHIFT & REEF_IRQ_LEVEL_MASK)
#define REEF_IRQ_BIT(_bit) \
	((_bit >> REEF_IRQ_BIT_SHIFT) & REEF_IRQ_BIT_MASK)
#define REEF_IRQ_CPU(_cpu) \
	((_cpu >> REEF_IRQ_CPU_SHIFT) & REEF_IRQ_CPU_MASK)
#else
/*
 * IRQs are directly mapped onto a single level, bit and level.
 */
#define REEF_IRQ_NUMBER(_irq)	(_irq)
#define REEF_IRQ_LEVEL(_level)	0
#define REEF_IRQ_BIT(_bit)	0
#define REEF_IRQ_CPU(_cpu)	0
#endif

#endif
