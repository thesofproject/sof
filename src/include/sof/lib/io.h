/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_LIB_IO_H__
#define __SOF_LIB_IO_H__


#include <stdint.h>

#if CONFIG_LIBRARY

static inline uint32_t io_reg_read(uint32_t reg) { return 0; }
static inline void io_reg_write(uint32_t reg, uint32_t val) {}
static inline void io_reg_update_bits(uint32_t reg, uint32_t mask,
				      uint32_t value) {}
static inline uint16_t io_reg_read16(uint32_t reg) { return 0; }
static inline void io_reg_write16(uint32_t reg, uint16_t val) {}
static inline void io_reg_update_bits16(uint32_t reg, uint16_t mask,
					uint16_t value) {}

#else

static inline uint32_t io_reg_read(uint32_t reg)
{
	return *((volatile uint32_t*)reg);
}

static inline void io_reg_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)reg) = val;
}

static inline void io_reg_update_bits(uint32_t reg, uint32_t mask,
				      uint32_t value)
{
	io_reg_write(reg, (io_reg_read(reg) & (~mask)) | (value & mask));
}

static inline uint16_t io_reg_read16(uint32_t reg)
{
	return *((volatile uint16_t*)reg);
}

static inline void io_reg_write16(uint32_t reg, uint16_t val)
{
	*((volatile uint16_t*)reg) = val;
}

static inline uint64_t io_reg_read_64(uint32_t reg)
{
	return (uint64_t)io_reg_read(reg) +
		(((uint64_t)io_reg_read(reg + 4)) << 32);
}

static inline void io_reg_write_64(uint32_t reg, uint64_t val)
{
	*((volatile uint64_t*)reg) = val;
}

static inline void io_reg_update_bits16(uint32_t reg, uint16_t mask,
					uint16_t value)
{
	io_reg_write16(reg, (io_reg_read16(reg) & (~mask)) | (value & mask));
}

static inline uint8_t io_reg_read8(uint32_t reg)
{
	return *((volatile uint8_t*)reg);
}

static inline void io_reg_write8(uint32_t reg, uint8_t val)
{
	*((volatile uint8_t*)reg) = val;
}

static inline void io_reg_update_bits8(uint32_t reg, uint8_t mask,
				       uint8_t value)
{
	io_reg_write8(reg, (io_reg_read8(reg) & (~mask)) | (value & mask));
}

#endif

#endif /* __SOF_LIB_IO_H__ */
