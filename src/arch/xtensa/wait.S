/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Xtensa architecture initialisation.
 */

#include <xtensa/coreasm.h>
#include <xtensa/config/specreg.h>
#include "xtos-internal.h"

	.text
	.align 4
	.global	arch_wait_for_interrupt
	.type	arch_wait_for_interrupt,@function
arch_wait_for_interrupt:
	abi_entry
	waiti 0
	abi_return

	.size	arch_wait_for_interrupt, . - arch_wait_for_interrupt
