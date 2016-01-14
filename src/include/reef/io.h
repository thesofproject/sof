/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_IO__
#define __INCLUDE_IO__

#include <stdint.h>

static inline uint32_t io_reg_read(uint32_t reg)
{
	return *((volatile uint32_t*)reg);
}

static inline void io_reg_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)reg) = val;
}

static inline void io_reg_update_bits(uint32_t reg, uint32_t mask, uint32_t value)
{
	io_reg_write(reg, (io_reg_read(reg) & (~mask)) | (value & mask));
}

#endif
