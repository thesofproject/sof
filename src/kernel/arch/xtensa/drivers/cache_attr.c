// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2021 NXP
//
// Author: Iuliana Prodan <iuliana.prodan@nxp.com>

#ifdef CONFIG_COMPILER_WORKAROUND_CACHE_ATTR

#include <xtensa/config/core.h>
#include <sof/drivers/cache_attr.h>

/*
 * We want to avoid buggy compiler optimization (function inlining).
 * So we replace the call to glb_addr_attr() from glb_is_cached()
 * with a function pointer that is initialized here.
 */
uint32_t (*get_addr_attr)(void *address) = glb_addr_attr;

/*
 * The _memmap_cacheattr_reset linker script variable has
 * dedicate cache attribute for every 512M in 4GB space
 * 1: write through
 * 2: cache bypass
 * 4: write back
 * F: invalid access
 */

/*
 * Since each hex digit keeps the attributes for a 512MB region,
 * we have the following address ranges:
 * Address range       - hex digit
 * 0        - 1FFFFFFF - 0
 * 20000000 - 3FFFFFFF - 1
 * 40000000 - 5FFFFFFF - 2
 * 60000000 - 7FFFFFFF - 3
 * 80000000 - 9FFFFFFF - 4
 * A0000000 - BFFFFFFF - 5
 * C0000000 - DFFFFFFF - 6
 * E0000000 - FFFFFFFF - 7
 */

/*
 * For the given address, get the corresponding hex digit
 * from the linker script variable that contains the cache attributes
 */
uint32_t glb_addr_attr(void *address)
{
	uint32_t addr_attr = xthal_get_cacheattr();

	/* Based on the above information, get the address region id (0-7) */
	uint32_t addr_shift = ((uint32_t)(uintptr_t)address) >> 29;

	addr_shift &= 0x7;

	/* Get the position of the cache attribute for a certain memory region.
	 * There are 4 bits per hex digit.
	 */
	addr_shift <<= 2;

	/* Get the corresponding hex digit from the linker script
	 * variable that contains the cache attributes of the address region
	 */
	addr_attr >>= addr_shift;
	addr_attr &= 0xF;

	return addr_attr;
}

/*
 * Check if the address is cacheable or not, by verifying the addr_attr,
 * which for cacheable addresses might be 1 or 4
 */
uint32_t glb_is_cached(void *address)
{
	/* Do not call glb_addr_attr() directly
	 * to avoid buggy compiler optimizations
	 */
	uint32_t addr_attr = get_addr_attr(address);

	if (addr_attr == 1 || addr_attr == 4)
		return 1;

	return 0;
}
#endif /* CONFIG_COMPILER_WORKAROUND_CACHE_ATTR */
