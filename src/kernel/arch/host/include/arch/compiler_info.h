/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

/**
 * \file include/sof/compiler_info.h
 * \brief Compiler version and name descriptor
 * \author Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#ifndef __ARCH_COMPILER_INFO_H__
#define __ARCH_COMPILER_INFO_H__

/* read used compilator name and version */
/* CC_NAME must be null terminated */
/* See declaration of sof_ipc_cc_version. */
#define CC_MAJOR __GNUC__
#define CC_MINOR __GNUC_MINOR__
#define CC_MICRO __GNUC_PATCHLEVEL__
#define CC_NAME "GCC"
#define CC_DESC ""

#endif /* __ARCH_COMPILER_INFO_H__ */
