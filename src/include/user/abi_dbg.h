/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

/**
 * \file include/user/abi_dbg.h
 * \brief ABI definitions for debug interfaces accessed by user space apps.
 * \author Marcin Maka <marcin.maka@linux.intel.com>
 *
 * Follows the rules documented in include/user/abi.h.
 */

#include <kernel/abi.h>

#ifndef __USER_ABI_DBG_H__
#define __USER_ABI_DBG_H__

#define SOF_ABI_DBG_MAJOR 5
#define SOF_ABI_DBG_MINOR 3
#define SOF_ABI_DBG_PATCH 0

#define SOF_ABI_DBG_VERSION SOF_ABI_VER(SOF_ABI_DBG_MAJOR, \
					SOF_ABI_DBG_MINOR, \
					SOF_ABI_DBG_PATCH)

#endif /* __USER_ABI_DBG_H__ */
