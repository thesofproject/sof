/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifndef __ZEPHYR_SOF_LIB_IO_H__
#define __ZEPHYR_SOF_LIB_IO_H__

#if CONFIG_LIBRARY

#define io_reg_read16(reg)	0
#define io_reg_write16(reg, val)
#define io_reg_update_bits16(reg, mask, value)
#define io_reg_read(reg)	0
#define io_reg_write(reg, val)
#define io_reg_update_bits(reg, mask, value)

#else

#include <zephyr/arch/cpu.h>

static inline uint8_t io_reg_read8(uint32_t reg)
{
	return sys_read8(reg);
}

static inline void io_reg_write8(uint32_t reg, uint8_t val)
{
	sys_write8(val, reg);
}

static inline void io_reg_update_bits8(uint32_t reg,
				       uint8_t mask,
				       uint8_t value)
{
	io_reg_write8(reg, (io_reg_read8(reg) & (~mask)) | (value & mask));
}

static inline uint16_t io_reg_read16(uint32_t reg)
{
	return sys_read16(reg);
}

static inline void io_reg_write16(uint32_t reg, uint16_t val)
{
	sys_write16(val, reg);
}

static inline void io_reg_update_bits16(uint32_t reg,
					uint16_t mask,
					uint16_t value)
{
	io_reg_write16(reg, (io_reg_read16(reg) & (~mask)) | (value & mask));
}

static inline uint32_t io_reg_read(uint32_t reg)
{
	return sys_read32(reg);
}

static inline void io_reg_write(uint32_t reg, uint32_t val)
{
	sys_write32(val, reg);
}

static inline void io_reg_update_bits(uint32_t reg,
				      uint32_t mask,
				      uint32_t value)
{
	io_reg_write(reg, (io_reg_read(reg) & (~mask)) | (value & mask));
}

static inline uint64_t io_reg_read64(uint32_t reg)
{
#if CONFIG_64BIT
	return sys_read64(reg);
#else
	return (uint64_t)io_reg_read(reg) +
		(((uint64_t)io_reg_read(reg + 4)) << 32);
#endif /* CONFIG_64BIT */
}

#endif /* CONFIG_LIBRARY */

#endif /* __ZEPHYR_SOF_LIB_IO_H__ */
