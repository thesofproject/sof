/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

/**
  * \file include/sof/platform.h
  * \brief Platform API definition
  * \author Marcin Maka <marcin.maka@linux.intel.com>
  */

#ifndef __SOF_PLATFORM_H__
#define __SOF_PLATFORM_H__

#include <platform/platform.h>

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <stdint.h>

struct sof;

/** \addtogroup platform_api Platform API
 *  Platform API specification.
 *  @{
 */

/*
 * APIs declared here are defined for every platform.
 */

/**
 * \brief Platform specific implementation of the On Boot Complete handler.
 * \param[in] boot_message Boot status code.
 * \return 0 if successful, error code otherwise.
 */
int platform_boot_complete(uint32_t boot_message);

/**
 * \brief Platform initialization entry, called during FW initialization.
 * \param[in] sof Context.
 * \return 0 if successful, error code otherwise.
 */
int platform_init(struct sof *sof);

/** @}*/

#define HOST_TO_LOCAL 0
#define LOCAL_TO_HOST 1

#ifdef CONFIG_IMX8ULP
static inline void convert_addr(int type, uint32_t *addr)
{
	switch(type) {
	case HOST_TO_LOCAL:
		*addr = SDRAM0_BASE + (*addr - MEM_RESERVED);
		break;
	case LOCAL_TO_HOST:
		*addr = MEM_RESERVED + (*addr - SDRAM0_BASE);
		break;
	};
}
#else
static inline void convert_addr(int type, uint32_t *addr)
{
	return;
}
#endif

#endif

#endif /* __SOF_PLATFORM_H__ */
