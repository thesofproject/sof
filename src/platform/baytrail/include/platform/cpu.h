/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file platform/cpu.h
 * \brief DSP core parameters.
 */

#ifndef __PLATFORM_CPU_H__
#define __PLATFORM_CPU_H__

/** \brief Number of available DSP cores (conf. by kconfig) */
#define PLATFORM_CORE_COUNT	CONFIG_CORE_COUNT

/** \brief Maximum allowed number of DSP cores */
#define MAX_CORE_COUNT	1

/** \brief Id of master DSP core */
#define PLATFORM_MASTER_CORE_ID	0

#endif /* __PLATFORM_CPU_H__ */
