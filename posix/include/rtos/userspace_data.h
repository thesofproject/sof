/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

/**
 * \file posix/include/rtos/userspace_data.h
 * \brief Placement of data that user-space threads must be able to access.
 *
 * Only the memory partition placement attributes, so that user-space code
 * can use them without the kernel memory domain helpers in
 * rtos/userspace_helper.h.
 */
#ifndef __POSIX_RTOS_USERSPACE_DATA_H__
#define __POSIX_RTOS_USERSPACE_DATA_H__

#define APP_TASK_BSS
#define APP_TASK_DATA
#define APP_SYSUSER_BSS
#define APP_SYSUSER_DATA

#endif /* __POSIX_RTOS_USERSPACE_DATA_H__ */
