/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

/**
 * \file zephyr/include/rtos/userspace_data.h
 * \brief Placement of data that user-space threads must be able to access.
 *
 * Only the memory partition placement attributes, so that user-space code
 * can use them without the kernel memory domain helpers in
 * rtos/userspace_helper.h.
 */
#ifndef __ZEPHYR_RTOS_USERSPACE_DATA_H__
#define __ZEPHYR_RTOS_USERSPACE_DATA_H__

#ifndef CONFIG_USERSPACE

#define APP_TASK_BSS
#define APP_TASK_DATA
#define APP_SYSUSER_BSS
#define APP_SYSUSER_DATA

#else

#include <zephyr/app_memory/app_memdomain.h>

#define APP_TASK_BSS	K_APP_BMEM(common_partition)
#define APP_TASK_DATA	K_APP_DMEM(common_partition)

#ifdef CONFIG_SOF_USERSPACE_LL
#define APP_SYSUSER_BSS	K_APP_BMEM(sysuser_partition)
#define APP_SYSUSER_DATA	K_APP_DMEM(sysuser_partition)
#else
#define APP_SYSUSER_BSS
#define APP_SYSUSER_DATA
#endif

#endif /* CONFIG_USERSPACE */

#endif /* __ZEPHYR_RTOS_USERSPACE_DATA_H__ */
