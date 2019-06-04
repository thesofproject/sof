/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/smp/include/arch/idc.h
 * \brief Xtensa SMP architecture IDC header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __ARCH_IDC_H__
#define __ARCH_IDC_H__

#include <stdint.h>

struct idc_msg;

void cpu_power_down_core(void);

void idc_enable_interrupts(int target_core, int source_core);
int arch_idc_send_msg(struct idc_msg *msg, uint32_t mode);
int arch_idc_init(void);
void idc_free(void);

#endif
