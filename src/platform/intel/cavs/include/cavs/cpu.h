/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

/**
 * \file cavs/cpu.h
 * \brief DSP parameters, common for cAVS platforms.
 */

#ifndef __INCLUDE_CAVS_CPU__
#define __INCLUDE_CAVS_CPU__

/** \brief Number of available DSP cores (conf. by kconfig) */
#define PLATFORM_CORE_COUNT	CONFIG_CORE_COUNT

#endif
