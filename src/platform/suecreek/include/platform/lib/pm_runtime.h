/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file platform/cannonlake/include/platform/lib/pm_runtime.h
 * \brief Runtime power management header file for Cannonlake
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_LIB_PM_RUNTIME_H__

#ifndef __PLATFORM_LIB_PM_RUNTIME_H__
#define __PLATFORM_LIB_PM_RUNTIME_H__

#include <cavs/lib/pm_runtime.h>

#endif /* __PLATFORM_LIB_PM_RUNTIME_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/pm_runtime.h"

#endif /* __SOF_LIB_PM_RUNTIME_H__ */
