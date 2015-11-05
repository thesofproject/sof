/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_INTERRUPT__
#define __INCLUDE_INTERRUPT__

int interrupt_register(int irq, void(*handler)(void *arg), void *arg);
void interrupt_unregister(int irq);

void interrupt_enable(int irq);
void interrupt_disable(int irq);

void interrupt_set(int irq);
void interrupt_clear(int irq);

#endif
