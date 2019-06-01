// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file arch/xtensa/up/cpu.c
 * \brief Xtensa UP CPU implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <arch/cpu.h>

void arch_cpu_enable_core(int id) { }

void arch_cpu_disable_core(int id) { }

int arch_cpu_is_core_enabled(int id) { return 1; }
