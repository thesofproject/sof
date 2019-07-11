/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/include/arch/idc.h
 * \brief Xtensa architecture IDC header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_IDC_H__

#ifndef __ARCH_IDC_H__
#define __ARCH_IDC_H__

#include <config.h>
#include <stdint.h>

struct idc_msg;

#if CONFIG_SMP

void cpu_power_down_core(void);

void idc_enable_interrupts(int target_core, int source_core);
int arch_idc_send_msg(struct idc_msg *msg, uint32_t mode);
int arch_idc_init(void);
void idc_free(void);

#else

static inline int arch_idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	return 0;
}

static inline int arch_idc_init(void) { return 0; }

#endif /* CONFIG_SMP */

#endif /* __ARCH_IDC_H__ */

#else

#error "This file shouldn't be included from outside of sof/idc.h"

#endif /* __SOF_IDC_H__ */
