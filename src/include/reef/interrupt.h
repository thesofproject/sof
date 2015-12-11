/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_INTERRUPT__
#define __INCLUDE_INTERRUPT__

#include <stdint.h>

int interrupt_register(int irq, void(*handler)(void *arg), void *arg);
void interrupt_unregister(int irq);

void interrupt_enable(int irq);
void interrupt_disable(int irq);
void interrupt_enable_sync(void);

void interrupt_set(int irq);
void interrupt_clear(int irq);
uint32_t interrupt_get(void);

void interrupt_local_enable(uint32_t flags);
void interrupt_local_disable(uint32_t flags);

#endif
