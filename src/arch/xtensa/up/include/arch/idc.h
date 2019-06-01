/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/up/include/arch/idc.h
 * \brief Xtensa UP architecture IDC header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __ARCH_IDC_H__
#define __ARCH_IDC_H__

struct idc_msg;

/**
 * \brief Sends IDC message.
 * \param[in,out] msg Pointer to IDC message.
 * \param[in] mode Is message blocking or not.
 * \return Error code.
 */
static inline int arch_idc_send_msg(struct idc_msg *msg,
				    uint32_t mode) { return 0; }

/**
 * \brief Initializes IDC data and registers for interrupt.
 */
static inline int arch_idc_init(void) { return 0; }

#endif
