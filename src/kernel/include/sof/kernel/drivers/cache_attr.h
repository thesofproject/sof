/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 NXP
 *
 * Author: Iuliana Prodan <iuliana.prodan@nxp.com>
 */

#ifndef __SOF_DRIVERS_CACHE_ATTR_H__
#define __SOF_DRIVERS_CACHE_ATTR_H__

#ifdef CONFIG_COMPILER_WORKAROUND_CACHE_ATTR

#include <stdint.h>

uint32_t glb_addr_attr(void *address);
uint32_t glb_is_cached(void *address);

#endif /* CONFIG_COMPILER_WORKAROUND_CACHE_ATTR */
#endif /* __SOF_DRIVERS_CACHE_ATTR_H__ */
